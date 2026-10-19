param(
    [string]$Port = 'COM16',
    [int]$BaudRate = 115200,
    [ValidateRange(-1, 14)]
    [int]$CapturePair = -1,
    [ValidateRange(0, 10000)]
    [int]$KnownDistanceCm = 0,
    [ValidateRange(5, 300)]
    [int]$CaptureDurationSeconds = 45,
    [string]$CaptureOutputDirectory = '.\hardware\outputs\records\calibration\2026-09-28_pelvis_left_wrist_txrf_fix'
)

$ErrorActionPreference = 'Stop'

$nodeNames = @('HEAD', 'PELVIS', 'LEFT-WRIST', 'RIGHT-WRIST', 'LEFT-ANKLE', 'RIGHT-ANKLE')
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

$nodes = @()
foreach ($name in $nodeNames) {
    $nodes += [pscustomobject]@{
        Name = $name; Sequence = $null; TimestampUs = $null
        Ax = $null; Ay = $null; Az = $null; Gx = $null; Gy = $null; Gz = $null
        SampleRateHz = $null; Seen = $null
    }
}

$ranges = @{}
for ($pair = 0; $pair -lt 15; $pair++) {
    $ranges[$pair] = [pscustomobject]@{
        LastValidCm = $null; LastValidSeen = $null
        LastEventCm = $null; LastEventSeen = $null; InvalidCount = 0
    }
}

function Get-AgeText($seen) {
    if ($null -eq $seen) { return 'waiting' }
    $age = ((Get-Date) - $seen).TotalSeconds
    if ($age -lt 2) { return ('live {0:N1}s' -f $age) }
    return ('stale {0:N1}s' -f $age)
}

function Get-RangeStatistics {
    param([double[]]$Values)
    if ($Values.Count -eq 0) {
        return [ordered]@{ minimum_cm = $null; median_cm = $null; mean_cm = $null; standard_deviation_cm = $null; maximum_cm = $null }
    }
    $sorted = @($Values | Sort-Object)
    $count = $sorted.Count
    $median = if ($count % 2 -eq 1) { $sorted[[int]($count / 2)] } else { ($sorted[($count / 2) - 1] + $sorted[$count / 2]) / 2.0 }
    $mean = ($sorted | Measure-Object -Average).Average
    $sumSquares = 0.0
    foreach ($value in $sorted) { $sumSquares += [math]::Pow(($value - $mean), 2) }
    $std = if ($count -gt 1) { [math]::Sqrt($sumSquares / ($count - 1)) } else { 0.0 }
    return [ordered]@{
        minimum_cm = [math]::Round($sorted[0], 2); median_cm = [math]::Round($median, 2)
        mean_cm = [math]::Round($mean, 2); standard_deviation_cm = [math]::Round($std, 2)
        maximum_cm = [math]::Round($sorted[$count - 1], 2)
    }
}

function Write-Node($index) {
    $node = $nodes[$index]
    $age = Get-AgeText $node.Seen
    $value = if ($null -eq $node.Sequence) {
        'waiting for IMU'
    } else {
        'seq {0}  {1,5:N1} Hz  A[{2,6} {3,6} {4,6}]  G[{5,6} {6,6} {7,6}]' -f `
            $node.Sequence, $node.SampleRateHz, $node.Ax, $node.Ay, $node.Az, $node.Gx, $node.Gy, $node.Gz
    }
    Write-Host ('{0,-12} {1,-12} {2}' -f $node.Name, $age, $value)
}

function Draw-Viewer {
    Clear-Host
    Write-Host 'COMP971 live six-node telemetry' -ForegroundColor Cyan
    Write-Host ("Gateway USB: {0} at {1} baud   Refresh: 1 second   Exit: Ctrl+C" -f $Port, $BaudRate)
    Write-Host 'Raw IMU values: accelerometer and gyroscope registers. Orientation is not calculated in this viewer.' -ForegroundColor DarkGray
    Write-Host ''
    Write-Host 'Body-position IMU stream' -ForegroundColor Yellow
    Write-Node 0
    Write-Node 2
    Write-Node 1
    Write-Node 3
    Write-Node 4
    Write-Node 5
    Write-Host ''
    Write-Host 'UWB distance matrix (latest raw range)' -ForegroundColor Yellow
    for ($pair = 0; $pair -lt 15; $pair++) {
        $entry = $ranges[$pair]
        $age = Get-AgeText $entry.LastValidSeen
        $range = if ($null -eq $entry.LastValidCm) { 'waiting for valid range' } else { ('last valid {0,5} cm' -f $entry.LastValidCm) }
        $eventAge = if ($null -eq $entry.LastEventSeen) { 999 } else { ((Get-Date) - $entry.LastEventSeen).TotalSeconds }
        $warning = if ($entry.LastEventCm -lt 0 -and $eventAge -lt 2) { '  invalid just now' } else { '' }
        Write-Host ('{0,2}: {1,-31} {2,-12} {3}  invalids: {4}{5}' -f $pair, $pairNames[$pair], $age, $range, $entry.InvalidCount, $warning)
    }
    Write-Host ''
    Write-Host 'Note: last valid UWB values are live but not calibrated. An invalid attempt is counted separately and does not replace the last valid value.' -ForegroundColor DarkGray
}

$serial = [System.IO.Ports.SerialPort]::new($Port, $BaudRate, 'None', 8, 'One')
$serial.DtrEnable = $true
$serial.RtsEnable = $true
$serial.ReadTimeout = 150
$captureSamples = New-Object System.Collections.Generic.List[object]
$captureInvalids = 0
$captureStart = Get-Date

try {
    $serial.Open()
    $nextDraw = Get-Date
    while ($true) {
        try {
            $line = $serial.ReadLine()
            if ($line -match 'GATEWAY_IMU node=(\d+) seq=(\d+) t_us=(\d+) ax=(-?\d+) ay=(-?\d+) az=(-?\d+) gx=(-?\d+) gy=(-?\d+) gz=(-?\d+) temp=(-?\d+)') {
                $index = [int]$matches[1]
                if ($index -ge 0 -and $index -lt $nodes.Count) {
                    $node = $nodes[$index]
                    if ($null -ne $node.Sequence -and $null -ne $node.TimestampUs) {
                        $sequenceDelta = [int64]([uint32]$matches[2]) - [int64]$node.Sequence
                        $timeDeltaUs = [int64]([uint32]$matches[3]) - [int64]$node.TimestampUs
                        if ($sequenceDelta -gt 0 -and $timeDeltaUs -gt 0) {
                            $node.SampleRateHz = [math]::Round(($sequenceDelta * 1000000.0) / $timeDeltaUs, 1)
                        }
                    }
                    $node.Sequence = [uint32]$matches[2]
                    $node.TimestampUs = [uint32]$matches[3]
                    $node.Ax = [int]$matches[4]; $node.Ay = [int]$matches[5]; $node.Az = [int]$matches[6]
                    $node.Gx = [int]$matches[7]; $node.Gy = [int]$matches[8]; $node.Gz = [int]$matches[9]
                    $node.Seen = Get-Date
                }
            } elseif ($line -match 'GATEWAY_UWB pair=(\d+) source=1 t_us=(\d+) range_cm=(-?\d+) flags=0x([0-9A-Fa-f]+)') {
                $pair = [int]$matches[1]
                if ($pair -ge 0 -and $pair -lt 15) {
                    $entry = $ranges[$pair]
                    $entry.LastEventCm = [int]$matches[3]
                    $entry.LastEventSeen = Get-Date
                    if ($entry.LastEventCm -ge 0 -and $matches[4] -eq '0001') {
                        $entry.LastValidCm = $entry.LastEventCm
                        $entry.LastValidSeen = $entry.LastEventSeen
                    } else {
                        $entry.InvalidCount++
                    }
                    if ($pair -eq $CapturePair) {
                        if ($entry.LastEventCm -ge 0 -and $matches[4] -eq '0001') {
                            $captureSamples.Add([ordered]@{
                                gateway_received_utc = (Get-Date).ToUniversalTime().ToString('o')
                                pelvis_timestamp_us = [uint64]$matches[2]
                                range_cm = $entry.LastEventCm
                                flags = ('0x' + $matches[4].ToUpperInvariant())
                            })
                        } elseif ($entry.LastEventCm -lt 0) {
                            $captureInvalids++
                        }
                    }
                }
            }
        } catch [System.TimeoutException] {
        }

        if ((Get-Date) -ge $nextDraw) {
            Draw-Viewer
            $nextDraw = (Get-Date).AddSeconds(1)
        }
        if ($CapturePair -ge 0 -and ((Get-Date) - $captureStart).TotalSeconds -ge $CaptureDurationSeconds) {
            $rangesForStatistics = @($captureSamples | ForEach-Object { [double]$_.range_cm })
            $statistics = Get-RangeStatistics -Values $rangesForStatistics
            New-Item -ItemType Directory -Force -Path $CaptureOutputDirectory | Out-Null
            $sampleArray = [object[]]$captureSamples.ToArray()
            $result = [ordered]@{
                captured_at_utc = $captureStart.ToUniversalTime().ToString('o')
                duration_s = $CaptureDurationSeconds
                stated_antenna_to_antenna_distance_cm = $KnownDistanceCm
                pair_index = $CapturePair
                pair_name = $pairNames[$CapturePair]
                firmware = 'DS-TWR with explicit Qorvo channel-5 TX RF configuration (PGdly 0x34, power 0xfdfdfdfd)'
                calibration_status = 'raw measurement only; no correction stored'
                valid_samples = $captureSamples.Count
                invalid_attempts = $captureInvalids
                statistics = $statistics
                samples = $sampleArray
            }
            $safePairName = ($pairNames[$CapturePair].ToLowerInvariant() -replace '[^a-z0-9]+', '_').Trim('_')
            $path = Join-Path $CaptureOutputDirectory ("{0}_{1}cm_raw_crosscheck.json" -f $safePairName, $KnownDistanceCm)
            $result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $path -Encoding UTF8
            Write-Host "`nSaved: $path" -ForegroundColor Green
            Write-Host ("Valid: {0}; invalid: {1}; median: {2} cm; SD: {3} cm" -f $captureSamples.Count, $captureInvalids, $statistics.median_cm, $statistics.standard_deviation_cm) -ForegroundColor Yellow
            break
        }
    }
} finally {
    if ($serial.IsOpen) { $serial.Close() }
}
