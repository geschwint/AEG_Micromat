<#
.SYNOPSIS
  Builds the Micromat firmware and saves a deployable image stamped with its
  version, build date and fingerprint, e.g.
  firmware\Micromat_v1.0.0_2026-10-11_aa911e1e.bin

.DESCRIPTION
  Always build through this script (people and Claude alike), so every image
  that may end up on the oven can be identified from its file name.
  - version     FIRMWARE_VERSION from Micromat\about.h (bump it when releasing)
  - date        the day the image was built
  - fingerprint first 8 hex digits of the ELF SHA-256 stored in the image's app
                descriptor: exactly what the oven shows (boot log, service
                screen, About, update page)
  The stamped .bin is what Settings -> Firmware update takes. firmware\ is
  git-ignored (*.bin).

.PARAMETER Upload
  Also upload over USB (default COM5) after building.

.EXAMPLE
  .\tools\build.ps1
  .\tools\build.ps1 -Upload
  .\tools\build.ps1 -Upload -Port COM7
#>
param(
  [switch]$Upload,
  [string]$Port = 'COM5'
)
$ErrorActionPreference = 'Stop'

$Root = Split-Path -Parent $PSScriptRoot
$Sketch = Join-Path $Root 'Micromat'
$Fqbn = 'esp32:esp32:esp32s3:CDCOnBoot=cdc,USBMode=hwcdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB'
$Image = Join-Path $Sketch 'build\esp32.esp32.esp32s3\Micromat.ino.bin'
$OutDir = Join-Path $Root 'firmware'

# Version from about.h
$about = Get-Content (Join-Path $Sketch 'about.h') -Raw
if ($about -notmatch 'FIRMWARE_VERSION\s*=\s*"([^"]+)"') { throw 'FIRMWARE_VERSION not found in Micromat\about.h' }
$version = $Matches[1]

# Build (--export-binaries writes Micromat\build\..., also git-ignored)
Write-Host "Building Micromat v$version ..."
arduino-cli compile --fqbn $Fqbn --export-binaries $Sketch
if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)" }

# Fingerprint from the image's app descriptor: image header (24 bytes) +
# first segment header (8) = offset 32; the descriptor starts with magic
# 0xABCD5432 and has app_elf_sha256 at +144.
$bytes = [IO.File]::ReadAllBytes($Image)
if ([BitConverter]::ToInt32($bytes, 32) -ne 0xABCD5432) { throw 'App descriptor not found in the image' }  # both Int32 in PowerShell
$fingerprint = ($bytes[176..179] | ForEach-Object { $_.ToString('x2') }) -join ''

$date = Get-Date -Format 'yyyy-MM-dd'
New-Item -ItemType Directory -Force $OutDir | Out-Null
$out = Join-Path $OutDir "Micromat_v${version}_${date}_${fingerprint}.bin"
Copy-Item $Image $out -Force
Write-Host "Firmware image: $out"
Write-Host "  version $version, built $date, fingerprint $fingerprint, $($bytes.Length) bytes"

if ($Upload) {
  Write-Host "Uploading over USB ($Port) ..."
  arduino-cli upload -p $Port --fqbn $Fqbn $Sketch
  if ($LASTEXITCODE -ne 0) { throw "Upload failed (exit $LASTEXITCODE)" }
}
