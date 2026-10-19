param(
    [string]$Port = 'COM16',
    [ValidateSet('smoke', 'boot1', 'boot2', 'boot3', 'reposition')]
    [string]$RunType = 'boot1',
    [ValidateRange(1, 1000)]
    [int]$KnownDistanceCm = 100,
    [int]$TotalDurationSeconds = 180,
    [int]$DiscardSeconds = 30,
    [double]$BatteryVoltageStart = 0.0,
    [double]$BatteryVoltageEnd = 0.0,
    [string]$BatteryInfo = 'LiPo 3.7V',
    [string]$OutputDirectory = '.\hardware\outputs\records\calibration\phase_b_prime'
)

$ErrorActionPreference = 'Stop'

function Get-Robust-Statistics {
    param([double[]]$Values)
    if ($Values.Count -eq 0) {
        return [ordered]@{
            minimum_cm = $null
            median_cm = $null
            mad_cm = $null
            robust_spread_cm = $null
            mean_cm = $null
            standard_deviation_cm = $null
            maximum_cm = $null
            outliers_gt_10cm = $null
            outlier_fraction_gt_10cm = $null
        }
    }

    $sorted = @($Values | Sort-Object)
    $count = $sorted.Count
    $median = if ($count % 2 -eq 1) {
        $sorted[[int]($count / 2)]
    } else {
        ($sorted[($count / 2) - 1] + $sorted[$count / 2]) / 2.0
    }

    $devs = @()
    foreach ($v in $sorted) {
        $devs += [math]::Abs($v - $median)
    }
    $sortedDevs = @($devs | Sort-Object)
    $mad = if ($count % 2 -eq 1) {
        $sortedDevs[[int]($count / 2)]
    } else {
        ($sortedDevs[($count / 2) - 1] + $sortedDevs[$count / 2]) / 2.0
    }
    $robustSpread = 1.4826 * $mad

    $mean = ($sorted | Measure-Object -Average).Average
    $sumSquares = 0.0
    $outliers = 0
    foreach ($value in $sorted) { 
        $sumSquares += [math]::Pow(($value - $mean), 2)
        if ([math]::Abs($value - $median) -gt 10.0) {
            $outliers++
        }
    }
    $std = if ($count -gt 1) { [math]::Sqrt($sumSquares / ($count - 1)) } else { 0.0 }

    return [ordered]@{
        minimum_cm = [math]::Round($sorted[0], 2)
        median_cm = [math]::Round($median, 2)
        mad_cm = [math]::Round($mad, 2)
        robust_spread_cm = [math]::Round($robustSpread, 2)
        mean_cm = [math]::Round($mean, 2)
        standard_deviation_cm = [math]::Round($std, 2)
        maximum_cm = [math]::Round($sorted[$count - 1], 2)
        outliers_gt_10cm = $outliers
        outlier_fraction_gt_10cm = [math]::Round(($outliers / [double]$count), 4)
    }
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$captureId = (Get-Date).ToString('yyyy-MM-dd_HHmmss')
$filename = "phaseB_prime_${RunType}_${KnownDistanceCm}cm_${captureId}.json"
$outputPath = Join-Path $OutputDirectory $filename

$serial = [System.IO.Ports.SerialPort]::new($Port, 115200, 'None', 8, 'One')
$serial.DtrEnable = $true
$serial.RtsEnable = $true
$serial.ReadTimeout = 250
$samples = New-Object System.Collections.Generic.List[object]
$discardedCount = 0
$invalidCount = 0
$firstValidElapsed = $null
$buffer = New-Object System.Collections.Generic.List[byte]
$readBuf = New-Object byte[] 4096
$start = Get-Date

try {
    $serial.Open()
    Write-Host "==========================================================" -ForegroundColor Green
    Write-Host "Phase B' Block: RunType=[$RunType] at stated $KnownDistanceCm cm." -ForegroundColor Cyan
    Write-Host "Duration: $TotalDurationSeconds s (First $DiscardSeconds s discarded for settling)..." -ForegroundColor Yellow
    Write-Host "==========================================================" -ForegroundColor Green

    while (((Get-Date) - $start).TotalSeconds -lt $TotalDurationSeconds) {
        $elapsed = ((Get-Date) - $start).TotalSeconds
        $isSettling = ($elapsed -lt $DiscardSeconds)

        try {
            $n = $serial.Read($readBuf, 0, $readBuf.Length)
            if ($n -gt 0) {
                for ($k = 0; $k -lt $n; $k++) { $buffer.Add($readBuf[$k]) }
            }
        } catch [System.TimeoutException] { }

        while ($buffer.Count -ge 6) {
            if ($buffer[0] -ne 0xC9 -or $buffer[1] -ne 0x71) {
                $buffer.RemoveAt(0)
                continue
            }
            $len = [int]$buffer[4] + (256 * [int]$buffer[5])
            if ($buffer.Count -lt 6 + $len) { break }
            $type = $buffer[3]
            if ($type -eq 3 -and $len -eq 24) {
                $pairIdx = [int]$buffer[6 + 3]
                if ($pairIdx -eq 0) {
                    $pelvisTs = [BitConverter]::ToUInt32($buffer.ToArray(), 6 + 4)
                    $rangeCm = [BitConverter]::ToInt32($buffer.ToArray(), 6 + 8)
                    $flags = [BitConverter]::ToUInt16($buffer.ToArray(), 6 + 12)
                    $valid = ($rangeCm -ge 0) -and (($flags -band 1) -ne 0)

                    if ($valid -and ($firstValidElapsed -eq $null)) {
                        $firstValidElapsed = [math]::Round($elapsed, 3)
                        Write-Host ("First valid UWB range arrived at: {0} s (Range: {1} cm)" -f $firstValidElapsed, $rangeCm) -ForegroundColor DarkCyan
                    }

                    if ($isSettling) {
                        $discardedCount++
                    } else {
                        if ($valid) {
                            $samples.Add([ordered]@{
                                elapsed_s = [math]::Round($elapsed, 3)
                                pelvis_timestamp_us = $pelvisTs
                                range_cm = $rangeCm
                                flags = ('0x{0:X4}' -f $flags)
                            })
                        } else {
                            $invalidCount++
                        }
                    }
                }
            }
            $buffer.RemoveRange(0, 6 + $len)
        }
    }
} finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}

$ranges = @($samples | ForEach-Object { [double]$_.range_cm })
$statistics = Get-Robust-Statistics -Values $ranges
$sampleArray = [object[]]$samples.ToArray()
$totalPostSettling = $samples.Count + $invalidCount
$deliveryRate = if ($totalPostSettling -gt 0) { [math]::Round(($samples.Count / [double]$totalPostSettling), 4) } else { 0.0 }

$result = [ordered]@{
    experiment_phase = "Phase B-Prime: Repeatability & Placement (Battery Only)"
    run_type = $RunType
    stated_antenna_distance_cm = $KnownDistanceCm
    battery_info = $BatteryInfo
    battery_voltage_start_v = $BatteryVoltageStart
    battery_voltage_end_v = $BatteryVoltageEnd
    total_duration_s = $TotalDurationSeconds
    discard_settling_s = $DiscardSeconds
    active_measurement_s = $TotalDurationSeconds - $DiscardSeconds
    time_to_first_valid_range_s = $firstValidElapsed
    discarded_samples_during_settling = $discardedCount
    valid_samples = $samples.Count
    invalid_attempts = $invalidCount
    delivery_rate = $deliveryRate
    statistics = $statistics
    samples = $sampleArray
}

$result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $outputPath -Encoding UTF8
Write-Host "Saved Phase B' record: $outputPath" -ForegroundColor Green
Write-Host ("Results: Valid: {0}; Invalid: {1} ({2}% delivery); Median: {3} cm; MAD: {4} cm; Robust Spread: {5} cm; Mean: {6} cm; SD: {7} cm; Outliers (>10cm): {8} ({9}%)" -f $samples.Count, $invalidCount, ($deliveryRate * 100), $statistics.median_cm, $statistics.mad_cm, $statistics.robust_spread_cm, $statistics.mean_cm, $statistics.standard_deviation_cm, $statistics.outliers_gt_10cm, ($statistics.outlier_fraction_gt_10cm * 100)) -ForegroundColor Yellow
