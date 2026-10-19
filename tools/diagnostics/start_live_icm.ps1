param(
    [Parameter(Mandatory = $true)]
    [string]$Port,
    [Parameter(Mandatory = $true)]
    [string]$DeviceId
)

$ErrorActionPreference = "Stop"
$hardwareRoot = Split-Path -Parent $PSScriptRoot
$registry = Get-Content -LiteralPath (Join-Path $hardwareRoot "config\device_registry.json") -Raw |
    ConvertFrom-Json
$telemetryProfile = Get-Content -LiteralPath (Join-Path $hardwareRoot "config\profiles\six_node_telemetry_v1.json") -Raw |
    ConvertFrom-Json
$device = $registry.devices | Where-Object { $_.device_id -eq $DeviceId }
if ($null -eq $device -or $device.status -ne "registered") {
    throw "DeviceId must identify a registered device in config/device_registry.json."
}

$outputDirectory = Join-Path $hardwareRoot "outputs\diagnostics\dwm3001cdk_icm20948_stream\live"
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$baudRate = [int]$telemetryProfile.telemetry.serial_baud
$expectedRateHz = [int]$telemetryProfile.telemetry.imu_rate_hz
$logPath = Join-Path $outputDirectory ("{0}_ICM20948_{1}Hz_{2}.csv" -f $device.permanent_name, $expectedRateHz, $stamp)

$serial = [System.IO.Ports.SerialPort]::new(
    $Port,
    $BaudRate,
    [System.IO.Ports.Parity]::None,
    8,
    [System.IO.Ports.StopBits]::One
)
$serial.NewLine = "`n"
$serial.ReadTimeout = 1000
$writer = [System.IO.StreamWriter]::new($logPath, $false, [System.Text.UTF8Encoding]::new($false))
$writer.WriteLine("seq,t_ms,ax_mps2,ay_mps2,az_mps2,gx_dps,gy_dps,gz_dps,mx_uT,my_uT,mz_uT,temp_c,mag_drdy,mag_overflow")
$writer.AutoFlush = $true

try {
    $serial.Open()
    $firstSequence = $null
    $firstTimeMs = $null

    while ($true) {
        try {
            $line = $serial.ReadLine().Trim()
        }
        catch [System.TimeoutException] {
            continue
        }

        if (-not $line.StartsWith("ICM,")) {
            continue
        }

        $p = $line.Split(',')
        if ($p.Count -ne 15) {
            continue
        }

        $seq = [uint32]$p[1]
        $timeMs = [uint32]$p[2]
        $ax = [int16]$p[3] / 16384.0 * 9.80665
        $ay = [int16]$p[4] / 16384.0 * 9.80665
        $az = [int16]$p[5] / 16384.0 * 9.80665
        $gx = [int16]$p[6] / 131.0
        $gy = [int16]$p[7] / 131.0
        $gz = [int16]$p[8] / 131.0
        $mx = [int16]$p[9] * 0.15
        $my = [int16]$p[10] * 0.15
        $mz = [int16]$p[11] * 0.15
        $temp = [int16]$p[12] / 333.87 + 21.0
        $magDrdy = (([int]$p[13] -band 1) -ne 0)
        $magOverflow = (([int]$p[14] -band 8) -ne 0)

        if ($null -eq $firstSequence) {
            $firstSequence = $seq
            $firstTimeMs = $timeMs
        }

        $elapsedMs = [double]($timeMs - $firstTimeMs)
        $observedRate = if ($elapsedMs -gt 0) {
            (($seq - $firstSequence) * 1000.0) / $elapsedMs
        } else {
            0.0
        }

        $writer.WriteLine((
            "{0},{1},{2:F6},{3:F6},{4:F6},{5:F6},{6:F6},{7:F6},{8:F3},{9:F3},{10:F3},{11:F3},{12},{13}" -f
            $seq,$timeMs,$ax,$ay,$az,$gx,$gy,$gz,$mx,$my,$mz,$temp,[int]$magDrdy,[int]$magOverflow
        ))

        if (($seq % 6) -eq 0) {
            Clear-Host
            Write-Host "$($device.permanent_name) - ICM-20948 LIVE" -ForegroundColor Cyan
            Write-Host "Configured output: $expectedRateHz Hz | Observed: $($observedRate.ToString('F2')) Hz | Sample: $seq"
            Write-Host "Log: $logPath"
            Write-Host ""
            Write-Host "ACCELEROMETER (m/s^2)"
            Write-Host ("  X {0,10:F4}   Y {1,10:F4}   Z {2,10:F4}" -f $ax,$ay,$az)
            Write-Host ""
            Write-Host "GYROSCOPE (degrees/s)"
            Write-Host ("  X {0,10:F4}   Y {1,10:F4}   Z {2,10:F4}" -f $gx,$gy,$gz)
            Write-Host ""
            Write-Host "MAGNETOMETER (microtesla)"
            Write-Host ("  X {0,10:F3}   Y {1,10:F3}   Z {2,10:F3}" -f $mx,$my,$mz)
            Write-Host ""
            Write-Host ("TEMPERATURE  {0:F2} C" -f $temp)
            Write-Host ("MAG STATUS   data-ready={0}  overflow={1}" -f $magDrdy,$magOverflow)
            Write-Host ""
            Write-Host "Move or rotate the ICM board to see the values change."
            Write-Host "Press Ctrl+C to stop."
        }
    }
}
finally {
    if ($serial.IsOpen) {
        $serial.Close()
    }
    $serial.Dispose()
    $writer.Dispose()
}
