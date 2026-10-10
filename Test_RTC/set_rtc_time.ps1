# Sets the PCF85063A RTC (via Test_RTC running on the ESP32) to this PC's UTC time.
# Usage: pwsh Test_RTC\set_rtc_time.ps1 [-Port COM5]
# The PCF85063A's first tick after a set comes ~0.508 s later, so time X is sent
# at PC time X.482 (X.492 minus ~10 ms USB latency). Result: within ~30 ms of the PC.
param([string]$Port = 'COM5')

$p = New-Object System.IO.Ports.SerialPort $Port, 115200
$p.DtrEnable = $true
$p.NewLine = "`n"
$p.ReadTimeout = 2000
$p.Open()
try {
    Start-Sleep -Milliseconds 300
    $null = $p.ReadExisting()

    $now = [DateTime]::UtcNow
    $target = $now.AddTicks(-($now.Ticks % 10000000)).AddSeconds(2)
    while ([DateTime]::UtcNow -lt $target.AddMilliseconds(482)) { }
    $p.WriteLine('T ' + $target.ToString('yyyy-MM-dd HH:mm:ss'))
    Write-Output "Sent UTC $($target.ToString('yyyy-MM-dd HH:mm:ss'))"

    $deadline = (Get-Date).AddSeconds(3)
    while ((Get-Date) -lt $deadline) {
        $line = $p.ReadLine().Trim()
        if ($line -like 'SET*') { Write-Output $line; break }
    }
}
finally {
    $p.Close()
}
