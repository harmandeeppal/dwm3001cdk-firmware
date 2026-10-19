param(
    [string]$Port = 'COM16',
    [ValidateRange(0, 14)]
    [int]$Pair = 0,
    [ValidateRange(1, 10000)]
    [int]$KnownDistanceCm,
    [ValidateRange(5, 300)]
    [int]$DurationSeconds = 45,
    [string]$OutputDirectory = '.\hardware\outputs\records\calibration'
)

$ErrorActionPreference = 'Stop'

function Get-Statistics {
    param([double[]]$Values)
    if ($Values.Count -eq 0) {
        return [ordered]@{ minimum_cm = $null; median_cm = $null; mean_cm = $null; standard_deviation_cm = $null; maximum_cm = $null }
    }

    $sorted = @($Values | Sort-Object)
    $count = $sorted.Count
    $median = if ($count % 2 -eq 1) {
        $sorted[[int]($count / 2)]
    } else {
        ($sorted[($count / 2) - 1] + $sorted[$count / 2]) / 2.0
    }
    $mean = ($sorted | Measure-Object -Average).Average
    $sumSquares = 0.0
    foreach ($value in $sorted) { $sumSquares += [math]::Pow(($value - $mean), 2) }
    $std = if ($count -gt 1) { [math]::Sqrt($sumSquares / ($count - 1)) } else { 0.0 }

    return [ordered]@{
        minimum_cm = [math]::Round($sorted[0], 2)
        median_cm = [math]::Round($median, 2)
        mean_cm = [math]::Round($mean, 2)
        standard_deviation_cm = [math]::Round($std, 2)
        maximum_cm = [math]::Round($sorted[$count - 1], 2)
    }
}

$pairNames = @(
    'HEAD <-> PELVIS',
    'HEAD <-> LEFT-WRIST',
    'HEAD <-> RIGHT-WRIST',
    'HEAD <-> LEFT-ANKLE',
    'HEAD <-> RIGHT-ANKLE',
    'PELVIS <-> LEFT-WRIST',
    'PELVIS <-> RIGHT-WRIST',
    'PELVIS <-> LEFT-ANKLE',
    'PELVIS <-> RIGHT-ANKLE',
    'LEFT-WRIST <-> RIGHT-WRIST',
    'LEFT-WRIST <-> LEFT-ANKLE',
    'LEFT-WRIST <-> RIGHT-ANKLE',
    'RIGHT-WRIST <-> LEFT-ANKLE',
    'RIGHT-WRIST <-> RIGHT-ANKLE',
    'LEFT-ANKLE <-> RIGHT-ANKLE'
)
$pairName = $pairNames[$Pair]
$safePairName = $pairName.ToLowerInvariant().Replace(' <-> ', '_').Replace('-', '_')
$captureId = (Get-Date).ToString('yyyy-MM-dd_HHmmss')
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$outputPath = Join-Path $OutputDirectory ("{0}_{1}cm_{2}.json" -f $safePairName, $KnownDistanceCm, $captureId)

$serial = [System.IO.Ports.SerialPort]::new($Port, 115200, 'None', 8, 'One')
$serial.DtrEnable = $true
$serial.RtsEnable = $true
$serial.ReadTimeout = 250
$samples = New-Object System.Collections.Generic.List[object]
$invalidCount = 0
$buffer = New-Object System.Collections.Generic.List[byte]
$readBuf = New-Object byte[] 4096
$start = Get-Date

try {
    $serial.Open()
    Write-Host "Collecting pair $Pair at stated $KnownDistanceCm cm for $DurationSeconds seconds from $Port (binary v1 protocol)." -ForegroundColor Cyan
    while (((Get-Date) - $start).TotalSeconds -lt $DurationSeconds) {
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
                if ($pairIdx -eq $Pair) {
                    $pelvisTs = [BitConverter]::ToUInt32($buffer.ToArray(), 6 + 4)
                    $rangeCm = [BitConverter]::ToInt32($buffer.ToArray(), 6 + 8)
                    $flags = [BitConverter]::ToUInt16($buffer.ToArray(), 6 + 12)
                    $valid = ($rangeCm -ge 0) -and (($flags -band 1) -ne 0)
                    if ($valid) {
                        $samples.Add([ordered]@{
                            gateway_received_utc = (Get-Date).ToUniversalTime().ToString('o')
                            pelvis_timestamp_us = $pelvisTs
                            range_cm = $rangeCm
                            flags = ('0x{0:X4}' -f $flags)
                        })
                    } else {
                        $invalidCount++
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
$statistics = Get-Statistics -Values $ranges
$sampleArray = [object[]]$samples.ToArray()
$result = [ordered]@{
    captured_at_utc = $start.ToUniversalTime().ToString('o')
    duration_s = $DurationSeconds
    stated_antenna_to_antenna_distance_cm = $KnownDistanceCm
    pair_index = $Pair
    pair_name = $pairName
    firmware = 'FiRa binary stream v1; raw ranges'
    valid_samples = $samples.Count
    invalid_attempts = $invalidCount
    statistics = $statistics
    samples = $sampleArray
}

$result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $outputPath -Encoding UTF8
Write-Host "Saved: $outputPath" -ForegroundColor Green
Write-Host ("Valid: {0}; invalid: {1}; median: {2} cm; mean: {3} cm; SD: {4} cm" -f $samples.Count, $invalidCount, $statistics.median_cm, $statistics.mean_cm, $statistics.standard_deviation_cm) -ForegroundColor Yellow
