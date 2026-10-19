param(
    [string]$Port = 'COM16',
    [string]$Condition = 'static_standing',
    [ValidateRange(1, 1000)]
    [int]$KnownDistanceCm = 41,
    [int]$TotalDurationSeconds = 130, # 10s settling + 120s (2 min) active
    [int]$DiscardSeconds = 10,
    [double]$BatteryVoltageHead = 3.83,
    [double]$BatteryVoltagePelvis = 3.79,
    [string]$BatteryInfo = 'LiPo 3.7V',
    [string]$OutputDirectory = '.\hardware\outputs\records\calibration\on_body'
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
    $robustSpread = [math]::Round((1.4826 * $mad), 2)

    $mean = [math]::Round((($sorted | Measure-Object -Average).Average), 2)
    $sumSquares = 0.0
    $outliers = 0
    foreach ($value in $sorted) { 
        $sumSquares += [math]::Pow(($value - $mean), 2)
        if ([math]::Abs($value - $median) -gt 10.0) {
            $outliers++
        }
    }
    $variance = if ($count -gt 1) { $sumSquares / ($count - 1) } else { 0.0 }
    $stdDev = [math]::Round([math]::Sqrt($variance), 2)
    $outlierFrac = [math]::Round(($outliers / [double]$count), 4)

    return [ordered]@{
        minimum_cm = $sorted[0]
        median_cm = $median
        mad_cm = $mad
        robust_spread_cm = $robustSpread
        mean_cm = $mean
        standard_deviation_cm = $stdDev
        maximum_cm = $sorted[$count - 1]
        outliers_gt_10cm = $outliers
        outlier_fraction_gt_10cm = $outlierFrac
    }
}

if (-not (Test-Path -LiteralPath $OutputDirectory)) {
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
}

$nowIso = Get-Date -Format 'yyyy-MM-dd_HHmmss'
$outputFileName = "onbody_${Condition}_${KnownDistanceCm}cm_${nowIso}.json"
$outputPath = Join-Path -Path $OutputDirectory -ChildPath $outputFileName

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
    Write-Host "==========================================================" -ForegroundColor Cyan
    Write-Host "On-Body Validation: Condition=[$Condition] @ Tape Dist $KnownDistanceCm cm" -ForegroundColor Cyan
    Write-Host "Duration: $TotalDurationSeconds s (First $DiscardSeconds s settling discard)" -ForegroundColor Cyan
    Write-Host "HEAD Battery: $BatteryVoltageHead V | PELVIS Battery: $BatteryVoltagePelvis V" -ForegroundColor Cyan
    Write-Host "Target output file: $outputPath" -ForegroundColor Cyan
    Write-Host "==========================================================" -ForegroundColor Cyan

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
            # Wire protocol sync word: 0xC9 0x71
            if ($buffer[0] -ne 0xC9 -or $buffer[1] -ne 0x71) {
                $buffer.RemoveAt(0)
                continue
            }

            $len = [int]$buffer[4] + (256 * [int]$buffer[5])
            if ($buffer.Count -lt 6 + $len) { break }
            $type = $buffer[3]

            # Type 3 = UWB Range telemetry (24 bytes payload)
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

$calibratedMedian = if ($statistics.median_cm -ne $null) { [math]::Round((($statistics.median_cm + 2.16) / 1.0486), 2) } else { $null }

$result = [ordered]@{
    experiment_phase = "Phase E: On-Body Wearable Validation"
    condition = $Condition
    tape_measured_distance_cm = $KnownDistanceCm
    battery_info = $BatteryInfo
    battery_voltage_head_v = $BatteryVoltageHead
    battery_voltage_pelvis_v = $BatteryVoltagePelvis
    total_duration_s = $TotalDurationSeconds
    discard_settling_s = $DiscardSeconds
    active_measurement_s = $TotalDurationSeconds - $DiscardSeconds
    time_to_first_valid_range_s = $firstValidElapsed
    discarded_samples_during_settling = $discardedCount
    valid_samples = $samples.Count
    invalid_attempts = $invalidCount
    delivery_rate = $deliveryRate
    bench_calibrated_median_cm = $calibratedMedian
    statistics = $statistics
    samples = $sampleArray
}

$result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $outputPath -Encoding UTF8
Write-Host "Saved On-Body record: $outputPath" -ForegroundColor Green
Write-Host ("Results: Valid: {0}; Invalid: {1} ({2}% delivery); Raw Median: {3} cm; Calibrated: {4} cm (Tape: {5} cm); SD: {6} cm; Outliers (>10cm): {7} ({8}%)" -f $samples.Count, $invalidCount, ($deliveryRate * 100), $statistics.median_cm, $calibratedMedian, $KnownDistanceCm, $statistics.standard_deviation_cm, $statistics.outliers_gt_10cm, ($statistics.outlier_fraction_gt_10cm * 100)) -ForegroundColor Yellow
