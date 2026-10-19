param(
    [string]$Port = 'COM16',
    [int]$DurationSeconds = 30,
    [int]$SettlingSeconds = 3,
    [string]$OutputDirectory = '.\hardware\outputs\records\captures\2026-10-07_three_node_star_bringup'
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $OutputDirectory)) {
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
}

$nowIso = Get-Date -Format 'yyyy-MM-dd_HHmmss'
$outputPath = Join-Path -Path $OutputDirectory -ChildPath "three_node_star_test_${nowIso}.json"

$nodeNames = @{
    0 = "HEAD"
    1 = "PELVIS"
    2 = "LEFT-WRIST"
}

$pairNames = @{
    0 = "PELVIS <-> HEAD"
    1 = "PELVIS <-> LEFT-WRIST"
}

$diagNames = @{
    0xB0 = 'dw3000_tx_count'
    0xB1 = 'interval_min_lead_us'
    0xB2 = 'dw3000_late_tx_count'
    0xB3 = 'fira_stall_recovery_count'
    0xB4 = 'fira_steady_fail_count'
    0xB5 = 'fira_steady_total'
    0xB6 = 'fira_last_block_index'
    0xC1 = 'prog_mac_ready'
    0xC2 = 'prog_sched_ready'
    0xC3 = 'prog_session_ready'
    0xC4 = 'prog_mac_started'
    0xC5 = 'prog_session_started'
    0xD7 = 'err_add_controlee'
    0xEE = 'session_status_reason'
}

$serial = [System.IO.Ports.SerialPort]::new($Port, 115200, 'None', 8, 'One')
$serial.DtrEnable = $true
$serial.RtsEnable = $true
$serial.ReadTimeout = 250

$buffer = New-Object System.Collections.Generic.List[byte]
$readBuf = New-Object byte[] 4096

$imuCounts = @{ 0 = 0; 1 = 0; 2 = 0 }
$uwbRanges = @{
    0 = New-Object System.Collections.Generic.List[int]
    1 = New-Object System.Collections.Generic.List[int]
}
$uwbTimestamps = @{
    0 = New-Object System.Collections.Generic.List[double]
    1 = New-Object System.Collections.Generic.List[double]
}
$diagnostics = New-Object System.Collections.Generic.List[object]
$firstRangeElapsed = @{ 0 = $null; 1 = $null }

$start = Get-Date

try {
    $serial.Open()
    Write-Host "==========================================================" -ForegroundColor Cyan
    Write-Host "COMP971 3-NODE STAR BRING-UP TEST (PELVIS + HEAD + LEFT WRIST)" -ForegroundColor Cyan
    Write-Host "Gateway Port: $Port | Duration: $DurationSeconds s ($SettlingSeconds s settling)" -ForegroundColor Cyan
    Write-Host "Target output: $outputPath" -ForegroundColor Cyan
    Write-Host "==========================================================" -ForegroundColor Cyan

    while (((Get-Date) - $start).TotalSeconds -lt $DurationSeconds) {
        $elapsed = ((Get-Date) - $start).TotalSeconds
        $isSettling = ($elapsed -lt $SettlingSeconds)

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
            $payload = $buffer.GetRange(6, $len).ToArray()

            # Type 1: IMU sample (38 bytes)
            if ($type -eq 1 -and $len -ge 38) {
                $nodeIdx = [int]$payload[2]
                if (-not $isSettling) {
                    if ($imuCounts.ContainsKey($nodeIdx)) {
                        $imuCounts[$nodeIdx]++
                    } else {
                        $imuCounts[$nodeIdx] = 1
                    }
                }
            }

            # Type 3: UWB Range (24 bytes)
            if ($type -eq 3 -and $len -ge 24) {
                $sourceNode = [int]$payload[2]
                $pairIdx = [int]$payload[3]
                $pelvisTs = [BitConverter]::ToUInt32($payload, 4)
                $rangeCm = [BitConverter]::ToInt32($payload, 8)
                $flags = [BitConverter]::ToUInt16($payload, 12)
                $valid = ($rangeCm -ge 0) -and (($flags -band 1) -ne 0)

                if ($valid -and ($firstRangeElapsed[$pairIdx] -eq $null)) {
                    $firstRangeElapsed[$pairIdx] = [math]::Round($elapsed, 3)
                    $pName = if ($pairNames.ContainsKey($pairIdx)) { $pairNames[$pairIdx] } else { "Pair $pairIdx" }
                    Write-Host ("[FIRST UWB RANGE] {0}: {1} cm at {2:F2}s" -f $pName, $rangeCm, $elapsed) -ForegroundColor Green
                }

                if (-not $isSettling -and $valid) {
                    if ($uwbRanges.ContainsKey($pairIdx)) {
                        $uwbRanges[$pairIdx].Add($rangeCm)
                        $uwbTimestamps[$pairIdx].Add([math]::Round($elapsed, 3))
                    }
                }
            }

            # Type 5: UWB Diagnostic (16 or 24 bytes)
            if ($type -eq 5 -and $len -ge 16) {
                $srcNode = [int]$payload[2]
                $pairIdx = [int]$payload[3]
                $stageCode = [int]$payload[4]
                $subCode = [int]$payload[5]
                $tsUs = [BitConverter]::ToUInt32($payload, 8)
                $detailVal = [BitConverter]::ToInt32($payload, 12)

                if (-not $isSettling) {
                    $diagName = if ($diagNames.ContainsKey($stageCode)) { $diagNames[$stageCode] } else { ("0x{0:X2}" -f $stageCode) }
                    $diagnostics.Add([ordered]@{
                        elapsed_s = [math]::Round($elapsed, 3)
                        source_node = $srcNode
                        pair_index = $pairIdx
                        stage_code = ("0x{0:X2}" -f $stageCode)
                        stage_name = $diagName
                        sub_code = $subCode
                        timestamp_us = $tsUs
                        detail = $detailVal
                    })
                }
            }

            $buffer.RemoveRange(0, 6 + $len)
        }
    }
} finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}

$activeDuration = $DurationSeconds - $SettlingSeconds

Write-Host "`n==========================================================" -ForegroundColor Cyan
Write-Host "3-NODE STAR TELEMETRY SUMMARY ($activeDuration seconds active):" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

Write-Host "--- IMU TELEMETRY (Expected ~60 Hz per node) ---" -ForegroundColor Yellow
foreach ($k in ($imuCounts.Keys | Sort-Object)) {
    $nName = if ($nodeNames.ContainsKey($k)) { $nodeNames[$k] } else { "Node $k" }
    $cnt = $imuCounts[$k]
    $rate = [math]::Round(($cnt / [double]$activeDuration), 2)
    Write-Host ("  Node {0} ({1}): {2} samples ({3} Hz)" -f $k, $nName, $cnt, $rate)
}

Write-Host "`n--- UWB RANGE TELEMETRY (Expected ~5.0 Hz per pair) ---" -ForegroundColor Yellow
$summaryRanges = @{}
foreach ($pairIdx in ($uwbRanges.Keys | Sort-Object)) {
    $pName = if ($pairNames.ContainsKey($pairIdx)) { $pairNames[$pairIdx] } else { "Pair $pairIdx" }
    $rList = $uwbRanges[$pairIdx]
    $cnt = $rList.Count
    $rate = [math]::Round(($cnt / [double]$activeDuration), 2)

    if ($cnt -gt 0) {
        $sorted = @($rList | Sort-Object)
        $median = if ($cnt % 2 -eq 1) { $sorted[[int]($cnt / 2)] } else { ($sorted[($cnt / 2) - 1] + $sorted[$cnt / 2]) / 2.0 }
        $min = $sorted[0]
        $max = $sorted[$cnt - 1]
        Write-Host ("  Pair {0} ({1}): {2} ranges ({3} Hz) | Median: {4} cm (Min: {5}, Max: {6})" -f $pairIdx, $pName, $cnt, $rate, $median, $min, $max) -ForegroundColor Green
        $summaryRanges[$pairIdx] = [ordered]@{
            pair_name = $pName
            count = $cnt
            rate_hz = $rate
            median_cm = $median
            min_cm = $min
            max_cm = $max
            first_range_s = $firstRangeElapsed[$pairIdx]
        }
    } else {
        Write-Host ("  Pair {0} ({1}): 0 ranges (NO RANGING DETECTED)" -f $pairIdx, $pName) -ForegroundColor Red
        $summaryRanges[$pairIdx] = [ordered]@{
            pair_name = $pName
            count = 0
            rate_hz = 0
            median_cm = $null
            first_range_s = $null
        }
    }
}

Write-Host "`n--- UWB HEALTH & DIAGNOSTICS ---" -ForegroundColor Yellow
$lateTxDiags = @($diagnostics | Where-Object { $_.stage_name -eq 'dw3000_late_tx_count' })
$minLeadDiags = @($diagnostics | Where-Object { $_.stage_name -eq 'interval_min_lead_us' })
if ($lateTxDiags.Count -gt 0) {
    $lastLateTx = $lateTxDiags[-1].detail
    Write-Host ("  Late TX count: {0}" -f $lastLateTx) -ForegroundColor $(if ($lastLateTx -eq 0) { 'Green' } else { 'Yellow' })
}
if ($minLeadDiags.Count -gt 0) {
    $minLead = $minLeadDiags[-1].detail
    Write-Host ("  Min lead margin: {0} us ({1:F2} ms)" -f $minLead, ($minLead / 1000.0)) -ForegroundColor Green
}

$imuCountsStr = [ordered]@{}
foreach ($k in $imuCounts.Keys) { $imuCountsStr["Node_$k"] = $imuCounts[$k] }

$firstRangeStr = [ordered]@{}
foreach ($k in $firstRangeElapsed.Keys) { $firstRangeStr["Pair_$k"] = $firstRangeElapsed[$k] }

$summaryRangesStr = [ordered]@{}
foreach ($k in $summaryRanges.Keys) { $summaryRangesStr["Pair_$k"] = $summaryRanges[$k] }

$report = [ordered]@{
    test = "3-node star bring-up"
    port = $Port
    timestamp = $nowIso
    duration_s = $DurationSeconds
    active_duration_s = $activeDuration
    imu_counts = $imuCountsStr
    uwb_summary = $summaryRangesStr
    first_range_elapsed = $firstRangeStr
    diagnostics = $diagnostics
}

$report | ConvertTo-Json -Depth 6 | Out-File -FilePath $outputPath -Encoding utf8
Write-Host "`nSaved full bring-up test record to: $outputPath" -ForegroundColor Cyan


