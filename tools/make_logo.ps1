<#
.SYNOPSIS
  Converts the logo image into Micromat\logo_aeg.h: an LVGL alpha mask (A8)
  that the UI draws in the logo's red, on the white splash and the black
  sleep screen alike.

.DESCRIPTION
  Needs Windows PowerShell 5.1 (powershell.exe, not pwsh): it uses Windows'
  own image decoder (WinRT, which reads AVIF/PNG/JPG; AVIF needs the "AV1
  Video Extension" that Windows usually has) and System.Drawing for scaling.
  - Decodes the source image.
  - Crops to the logo itself: the bounding box of non-white pixels.
  - Scales it to -Width pixels wide (height follows) with high-quality bicubic
    filtering, flattened onto white.
  - Turns each pixel into coverage (0 = white, 255 = full logo red), measured
    on the green channel, where the red (#DF252C, G = 0x25) and white differ
    most, and writes it as an A8 lv_image_dsc_t.

.EXAMPLE
  powershell.exe -ExecutionPolicy Bypass -File tools\make_logo.ps1
#>
param(
  [string]$In = (Join-Path (Split-Path -Parent $PSScriptRoot) 'Logo-940x520.avif'),
  [string]$Out = (Join-Path (Split-Path -Parent $PSScriptRoot) 'Micromat\logo_aeg.h'),
  [int]$Width = 296
)
$ErrorActionPreference = 'Stop'
if ($PSVersionTable.PSEdition -ne 'Desktop') { throw 'Run with Windows PowerShell 5.1 (powershell.exe), not pwsh.' }
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Runtime.WindowsRuntime
$null = [Windows.Storage.StorageFile, Windows.Storage, ContentType = WindowsRuntime]
$null = [Windows.Graphics.Imaging.BitmapDecoder, Windows.Graphics.Imaging, ContentType = WindowsRuntime]
$null = [Windows.Graphics.Imaging.BitmapEncoder, Windows.Graphics.Imaging, ContentType = WindowsRuntime]

$ext = [System.WindowsRuntimeSystemExtensions].GetMethods()
$asOp = ($ext | Where-Object { $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' })[0]
$asAction = ($ext | Where-Object { $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncAction' })[0]
function Await($op, [Type]$t) { $task = $asOp.MakeGenericMethod($t).Invoke($null, @($op)); $task.Wait(-1) | Out-Null; $task.Result }

# Decode with WinRT and re-encode as PNG in memory (System.Drawing can't read AVIF).
$file = Await ([Windows.Storage.StorageFile]::GetFileFromPathAsync((Resolve-Path $In).Path)) ([Windows.Storage.StorageFile])
$src = Await ($file.OpenAsync([Windows.Storage.FileAccessMode]::Read)) ([Windows.Storage.Streams.IRandomAccessStream])
$dec = Await ([Windows.Graphics.Imaging.BitmapDecoder]::CreateAsync($src)) ([Windows.Graphics.Imaging.BitmapDecoder])
$sb = Await ($dec.GetSoftwareBitmapAsync([Windows.Graphics.Imaging.BitmapPixelFormat]::Bgra8, [Windows.Graphics.Imaging.BitmapAlphaMode]::Premultiplied)) ([Windows.Graphics.Imaging.SoftwareBitmap])
$mem = New-Object Windows.Storage.Streams.InMemoryRandomAccessStream
$enc = Await ([Windows.Graphics.Imaging.BitmapEncoder]::CreateAsync([Windows.Graphics.Imaging.BitmapEncoder]::PngEncoderId, $mem)) ([Windows.Graphics.Imaging.BitmapEncoder])
$enc.SetSoftwareBitmap($sb)
$asAction.Invoke($null, @($enc.FlushAsync())).Wait(-1) | Out-Null
$net = [System.IO.WindowsRuntimeStreamExtensions]::AsStreamForRead($mem.GetInputStreamAt(0))
$img = [System.Drawing.Bitmap]::FromStream($net)
Write-Host "decoded $($img.Width)x$($img.Height)"

# Crop to the non-white (and non-transparent) pixels.
$minX = $img.Width; $minY = $img.Height; $maxX = -1; $maxY = -1
for ($y = 0; $y -lt $img.Height; $y++) {
  for ($x = 0; $x -lt $img.Width; $x++) {
    $p = $img.GetPixel($x, $y)
    if ($p.A -gt 32 -and ($p.R + $p.G + $p.B) -lt 720) {
      if ($x -lt $minX) { $minX = $x }; if ($x -gt $maxX) { $maxX = $x }
      if ($y -lt $minY) { $minY = $y }; if ($y -gt $maxY) { $maxY = $y }
    }
  }
}
if ($maxX -lt 0) { throw 'No logo pixels found' }
$cw = $maxX - $minX + 1; $ch = $maxY - $minY + 1
$h = [int][Math]::Round($Width * $ch / $cw)
Write-Host "logo at $minX,$minY size ${cw}x$ch -> ${Width}x$h"

# Scale onto white.
$dst = New-Object System.Drawing.Bitmap $Width, $h
$g = [System.Drawing.Graphics]::FromImage($dst)
$g.Clear([System.Drawing.Color]::White)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
$attr = New-Object System.Drawing.Imaging.ImageAttributes
$attr.SetWrapMode([System.Drawing.Drawing2D.WrapMode]::TileFlipXY)  # no grey fringe at the crop edges
$g.DrawImage($img, (New-Object System.Drawing.Rectangle 0, 0, $Width, $h), $minX, $minY, $cw, $ch, [System.Drawing.GraphicsUnit]::Pixel, $attr)
$g.Dispose()

# A8: one coverage byte per pixel.
$logoG = 0x25
$sbuf = New-Object System.Text.StringBuilder
$n = 0
for ($y = 0; $y -lt $h; $y++) {
  for ($x = 0; $x -lt $Width; $x++) {
    $p = $dst.GetPixel($x, $y)
    $a = [int][Math]::Round((255 - [int]$p.G) * 255.0 / (255 - $logoG))
    $a = [Math]::Max(0, [Math]::Min(255, $a))
    if ($n % 24 -eq 0) { [void]$sbuf.Append("`n  ") }
    [void]$sbuf.AppendFormat('0x{0:x2},', $a)
    $n++
  }
}

$name = Split-Path $In -Leaf
$header = @"
// The AEG logo: ${Width}x$h px alpha mask (A8); the UI colours it red.
// Generated by tools/make_logo.ps1 from $name; don't edit by hand.
#pragma once
#include <lvgl.h>

static const uint8_t LOGO_AEG_PIXELS[] = {$($sbuf.ToString())
};

static const lv_image_dsc_t LOGO_AEG = {
  .header = {
    .magic = LV_IMAGE_HEADER_MAGIC,
    .cf = LV_COLOR_FORMAT_A8,
    .flags = 0,
    .w = $Width,
    .h = $h,
    .stride = $Width,
    .reserved_2 = 0,
  },
  .data_size = sizeof(LOGO_AEG_PIXELS),
  .data = LOGO_AEG_PIXELS,
  .reserved = nullptr,
  .reserved_2 = nullptr,
};
"@
[System.IO.File]::WriteAllText($Out, $header.Replace("`r`n", "`n"))
Write-Host "wrote $Out ($($Width * $h) bytes of pixels)"
