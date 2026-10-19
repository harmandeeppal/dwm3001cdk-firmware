param(
    [string]$Port = 'COM16',
    [int]$DurationSeconds = 180,
    [int]$SettlingSeconds = 3,
    [int]$BlockDurationMs = 200,
    [string]$OutputDirectory = '.\hardware\outputs\records\captures\2026-10-07_three_node_star_bringup'
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $OutputDirectory)) {
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
}

$nowIso = Get-Date -Format 'yyyy-MM-dd_HHmmss'
$outputPath = Join-Path -Path $OutputDirectory -ChildPath "six_node_star_test_${nowIso}.json"

$nodeNames = @{
    0 = "HEAD"
    1 = "PELVIS"
    2 = "LEFT-WRIST"
    3 = "RIGHT-WRIST"
    4 = "LEFT-ANKLE"
    5 = "RIGHT-ANKLE"
}

$pairNames = @{
    0 = "PELVIS <-> HEAD"
    1 = "PELVIS <-> LEFT-WRIST"
    2 = "PELVIS <-> RIGHT-WRIST"
    3 = "PELVIS <-> LEFT-ANKLE"
    4 = "PELVIS <-> RIGHT-ANKLE"
}

$diagNames = @{
    0x42 = 'uwb_responder_invalid_range'
    0xB0 = 'dw3000_tx_count'
    0xB1 = 'interval_min_lead_us'
    0xB2 = 'dw3000_late_tx_count'
    0xB3 = 'fira_stall_recovery_count'
    0xB4 = 'fira_steady_fail_count'
    0xB5 = 'fira_steady_total'
    0xB6 = 'fira_last_block_index'
    0xB7 = 'fira_lifetime_min_lead'
    0xB8 = 'dw3000_sys_status_lo'
    0xB9 = 'dw3000_sys_status_hi'
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

$imuCounts = @{ 0 = 0; 1 = 0; 2 = 0; 3 = 0; 4 = 0; 5 = 0 }
$uwbValidRanges = @{
    0 = New-Object System.Collections.Generic.List[int]
    1 = New-Object System.Collections.Generic.List[int]
    2 = New-Object System.Collections.Generic.List[int]
    3 = New-Object System.Collections.Generic.List[int]
    4 = New-Object System.Collections.Generic.List[int]
}
$uwbInvalidCounts = @{ 0 = 0; 1 = 0; 2 = 0; 3 = 0; 4 = 0 }
$uwbStatusCounts = @{
    0 = @{}
    1 = @{}
    2 = @{}
    3 = @{}
    4 = @{}
}
$starRangeBlocks = New-Object System.Collections.Generic.List[object]
$uwbTimestamps = @{
    0 = New-Object System.Collections.Generic.List[double]
    1 = New-Object System.Collections.Generic.List[double]
    2 = New-Object System.Collections.Generic.List[double]
    3 = New-Object System.Collections.Generic.List[double]
    4 = New-Object System.Collections.Generic.List[double]
}
$nodeStatusLatest = @{}
$diagnostics = New-Object System.Collections.Generic.List[object]
$stallSnapshots = New-Object System.Collections.Generic.List[object]
$baselineDiags = @{}
$finalDiags = @{}
$pairToNode = @{ 0 = 0; 1 = 2; 2 = 3; 3 = 4; 4 = 5 }
$nodeLatestUptimeUs = @{}
$firstPelvisUptime = @{ 0 = $null; 1 = $null; 2 = $null; 3 = $null; 4 = $null }
$firstControleeUptime = @{ 0 = $null; 1 = $null; 2 = $null; 3 = $null; 4 = $null }
$firstRangeElapsed = @{ 0 = $null; 1 = $null; 2 = $null; 3 = $null; 4 = $null }

$start = Get-Date

try {
    $serial.Open()
    Write-Host "==========================================================" -ForegroundColor Cyan
    Write-Host "COMP971 6-NODE STAR BENCHMARK (PELVIS + 5 CONTROLEES)" -ForegroundColor Cyan
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
                $sampleLocalUs = [BitConverter]::ToUInt32($payload, 8)
                $nodeLatestUptimeUs[$nodeIdx] = $sampleLocalUs
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
                $statusCode = [BitConverter]::ToUInt16($payload, 14)
                $valid = ($rangeCm -ge 0) -and (($flags -band 1) -ne 0)

                if ($valid -and ($firstRangeElapsed[$pairIdx] -eq $null)) {
                    $firstRangeElapsed[$pairIdx] = [math]::Round($elapsed, 3)
                    $firstPelvisUptime[$pairIdx] = [math]::Round($pelvisTs / 1000000.0, 3)
                    $cNode = $pairToNode[$pairIdx]
                    $cUptime = if ($nodeLatestUptimeUs.ContainsKey($cNode)) { [math]::Round($nodeLatestUptimeUs[$cNode] / 1000000.0, 3) } else { "N/A" }
                    $firstControleeUptime[$pairIdx] = $cUptime
                    $pName = if ($pairNames.ContainsKey($pairIdx)) { $pairNames[$pairIdx] } else { "Pair $pairIdx" }
                    Write-Host ("[FIRST UWB RANGE] {0}: {1} cm | Elapsed: {2:F2}s | Pelvis Uptime: {3:F3}s | Controlee Uptime: {4}s" -f $pName, $rangeCm, $elapsed, ($pelvisTs / 1000000.0), $cUptime) -ForegroundColor Green
                }

                if (-not $isSettling) {
                    if ($valid) {
                        if ($uwbValidRanges.ContainsKey($pairIdx)) {
                            $uwbValidRanges[$pairIdx].Add($rangeCm)
                            $uwbTimestamps[$pairIdx].Add([math]::Round($elapsed, 3))
                        }
                    } else {
                        if ($uwbInvalidCounts.ContainsKey($pairIdx)) {
                            $uwbInvalidCounts[$pairIdx]++
                        }
                        if ($uwbStatusCounts.ContainsKey($pairIdx)) {
                            $sKey = "status_$statusCode"
                            if (-not $uwbStatusCounts[$pairIdx].ContainsKey($sKey)) {
                                $uwbStatusCounts[$pairIdx][$sKey] = 0
                            }
                            $uwbStatusCounts[$pairIdx][$sKey]++
                        }
                    }
                }
            }

            # Type 7: Packed Star Ranges (40 bytes = 32 bytes BLE payload + 8 bytes gateway receive timestamp)
            if ($type -eq 7 -and $len -ge 40) {
                $srcNode = [int]$payload[2]
                $pelvisTs = [BitConverter]::ToUInt32($payload, 4)
                $blockIdx = [BitConverter]::ToUInt32($payload, 8)

                for ($p = 0; $p -lt 5; $p++) {
                    $pOff = 12 + ($p * 4)
                    $rangeCm = [BitConverter]::ToInt16($payload, $pOff)
                    $statusCode = [int]$payload[$pOff + 2]
                    $flags = [int]$payload[$pOff + 3]
                    $valid = ($rangeCm -ge 0) -and (($flags -band 1) -ne 0)

                    if ($valid -and ($firstRangeElapsed[$p] -eq $null)) {
                        $firstRangeElapsed[$p] = [math]::Round($elapsed, 3)
                        $firstPelvisUptime[$p] = [math]::Round($pelvisTs / 1000000.0, 3)
                        $cNode = $pairToNode[$p]
                        $cUptime = if ($nodeLatestUptimeUs.ContainsKey($cNode)) { [math]::Round($nodeLatestUptimeUs[$cNode] / 1000000.0, 3) } else { "N/A" }
                        $firstControleeUptime[$p] = $cUptime
                        $pName = if ($pairNames.ContainsKey($p)) { $pairNames[$p] } else { "Pair $p" }
                        Write-Host ("[FIRST UWB RANGE] {0}: {1} cm (Block {2}) | Elapsed: {3:F2}s | Pelvis Uptime: {4:F3}s | Controlee Uptime: {5}s" -f $pName, $rangeCm, $blockIdx, $elapsed, ($pelvisTs / 1000000.0), $cUptime) -ForegroundColor Green
                    }

                    if (-not $isSettling) {
                        if ($valid) {
                            if ($uwbValidRanges.ContainsKey($p)) {
                                $uwbValidRanges[$p].Add($rangeCm)
                                $uwbTimestamps[$p].Add([math]::Round($elapsed, 3))
                            }
                        } else {
                            if ($uwbInvalidCounts.ContainsKey($p)) {
                                $uwbInvalidCounts[$p]++
                            }
                        }
                        if ($uwbStatusCounts.ContainsKey($p)) {
                            $sKey = "status_$statusCode"
                            if (-not $uwbStatusCounts[$p].ContainsKey($sKey)) {
                                $uwbStatusCounts[$p][$sKey] = 0
                            }
                            $uwbStatusCounts[$p][$sKey]++
                        }
                    }
                }

                if (-not $isSettling) {
                    $pairStatusArr = @(0..4 | ForEach-Object {
                        $pOff = 12 + ($_ * 4)
                        [ordered]@{
                            range_cm = [BitConverter]::ToInt16($payload, $pOff)
                            status = [int]$payload[$pOff + 2]
                            flags = [int]$payload[$pOff + 3]
                        }
                    })
                    $starRangeBlocks.Add([ordered]@{
                        elapsed_s = [math]::Round($elapsed, 3)
                        block_index = $blockIdx
                        pairs = $pairStatusArr
                    })
                }
            }

            # Type 6: Stall Snapshot (52 bytes)
            if ($type -eq 6 -and $len -ge 48) {
                $snapNode = [int]$payload[2]
                $sBoot = [BitConverter]::ToUInt32($payload, 8)
                $sReset = [BitConverter]::ToUInt32($payload, 12)
                $sStage = [int]$payload[16]
                $sDur = [BitConverter]::ToUInt32($payload, 20)
                $sBlock = [BitConverter]::ToUInt32($payload, 24)
                $sMinLead = [BitConverter]::ToInt32($payload, 28)
                $sLateTx = [BitConverter]::ToUInt32($payload, 32)
                $cb0_block = [BitConverter]::ToUInt16($payload, 36)
                $cb0_status = [int]$payload[38]
                $cb0_lead = [int]$payload[39]
                $snName = if ($nodeNames.ContainsKey($snapNode)) { $nodeNames[$snapNode] } else { "Node $snapNode" }
                Write-Host ("[STALL SNAPSHOT RECEIVED] Node {0} ({1}): Boot {2} | Reset 0x{3:X2} | Stage {4} | StallDur: {5} ms | Block {6} | MinLead: {7} DTU ({8:F1} us) | LateTx: {9} | LastCB: blk={10} stat={11} lead={12}ms" -f `
                    $snapNode, $snName, $sBoot, $sReset, $sStage, $sDur, $sBlock, $sMinLead, ($sMinLead / 249.6), $sLateTx, $cb0_block, $cb0_status, $cb0_lead) -ForegroundColor Red
                $stallSnapshots.Add([ordered]@{
                    elapsed_s = [math]::Round($elapsed, 3)
                    node_index = $snapNode
                    node_name = $snName
                    boot_count = $sBoot
                    reset_reason = ("0x{0:X2}" -f $sReset)
                    stage = $sStage
                    stall_duration_ms = $sDur
                    last_block_index = $sBlock
                    min_lead_dtu = $sMinLead
                    min_lead_us = [math]::Round(($sMinLead / 249.6), 1)
                    min_lead_ms = [math]::Round(($sMinLead / 249600.0), 4)
                    late_tx_count = $sLateTx
                    last_cb_block = $cb0_block
                    last_cb_status = $cb0_status
                    last_cb_lead_ms = $cb0_lead
                })
            }

            # Type 4: Node Status (60 bytes)
            if ($type -eq 4 -and $len -ge 52) {
                $nodeIdx = [int]$payload[2]
                $nodeStatusLatest[$nodeIdx] = [ordered]@{
                    node_index = $nodeIdx
                    flags = [int]$payload[3]
                    sync_epoch = [BitConverter]::ToUInt32($payload, 4)
                    imu_samples_acquired = [BitConverter]::ToUInt32($payload, 8)
                    imu_queue_put_ok = [BitConverter]::ToUInt32($payload, 12)
                    imu_queue_dropped = [BitConverter]::ToUInt32($payload, 16)
                    imu_queue_high_water = [BitConverter]::ToUInt32($payload, 20)
                    ble_imu_notify_ok = [BitConverter]::ToUInt32($payload, 24)
                    ble_imu_notify_failed = [BitConverter]::ToUInt32($payload, 28)
                    ble_uwb_notify_failed = [BitConverter]::ToUInt32($payload, 32)
                    uwb_range_attempts = [BitConverter]::ToUInt32($payload, 36)
                    uwb_range_valid = [BitConverter]::ToUInt32($payload, 40)
                    uwb_range_invalid = [BitConverter]::ToUInt32($payload, 44)
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
                $diagName = if ($diagNames.ContainsKey($stageCode)) { $diagNames[$stageCode] } else { ("0x{0:X2}" -f $stageCode) }

                $diagKey = "$srcNode-$diagName"
                if (-not $baselineDiags.ContainsKey($diagKey)) {
                    $baselineDiags[$diagKey] = $detailVal
                }
                $finalDiags[$diagKey] = $detailVal

                if (-not $isSettling) {
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

                    if ($stageCode -eq 0x42 -and $uwbStatusCounts.ContainsKey($pairIdx)) {
                        $sKey = "status_$detailVal"
                        if (-not $uwbStatusCounts[$pairIdx].ContainsKey($sKey)) {
                            $uwbStatusCounts[$pairIdx][$sKey] = 0
                        }
                        $uwbStatusCounts[$pairIdx][$sKey]++
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

$activeDuration = $DurationSeconds - $SettlingSeconds

Write-Host "`n==========================================================" -ForegroundColor Cyan
Write-Host "COMP971 6-NODE STAR TELEMETRY SUMMARY ($activeDuration seconds active):" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

Write-Host "--- IMU TELEMETRY (Expected ~60 Hz per node) ---" -ForegroundColor Yellow
foreach ($k in ($imuCounts.Keys | Sort-Object)) {
    $nName = if ($nodeNames.ContainsKey($k)) { $nodeNames[$k] } else { "Node $k" }
    $cnt = $imuCounts[$k]
    $rate = [math]::Round(($cnt / [double]$activeDuration), 2)
    $statInfo = if ($nodeStatusLatest.ContainsKey($k)) {
        $st = $nodeStatusLatest[$k]
        (" | Internal: acq={0}, q_drop={1}, ble_fail={2}" -f $st.imu_samples_acquired, $st.imu_queue_dropped, $st.ble_imu_notify_failed)
    } else { " | Internal: (status packet dropped/pending)" }
    Write-Host ("  Node {0} ({1,-11}): {2,6} samples ({3,5:F2} Hz){4}" -f $k, $nName, $cnt, $rate, $statInfo)
}

Write-Host "`n--- UWB RANGE TELEMETRY ---" -ForegroundColor Yellow
$nominalRateHz = 1000.0 / [double]$BlockDurationMs
$expectedRounds = [math]::Round($activeDuration * $nominalRateHz)
Write-Host ("Nominal FiRa round budget: {0} expected rounds per pair @ {1:F2} Hz (Block: {2} ms)" -f $expectedRounds, $nominalRateHz, $BlockDurationMs) -ForegroundColor Gray
$summaryRanges = @{}
foreach ($pairIdx in ($uwbValidRanges.Keys | Sort-Object)) {
    $pName = if ($pairNames.ContainsKey($pairIdx)) { $pairNames[$pairIdx] } else { "Pair $pairIdx" }
    $rList = $uwbValidRanges[$pairIdx]
    $validCnt = $rList.Count
    $invalidCnt = if ($uwbInvalidCounts.ContainsKey($pairIdx)) { $uwbInvalidCounts[$pairIdx] } else { 0 }
    $totalAttempts = $validCnt + $invalidCnt
    $rate = [math]::Round(($validCnt / [double]$activeDuration), 2)
    $successPct = if ($totalAttempts -gt 0) { [math]::Round(($validCnt * 100.0 / $totalAttempts), 1) } else { 0.0 }
    $trueYieldPct = if ($expectedRounds -gt 0) { [math]::Round(($validCnt * 100.0 / $expectedRounds), 1) } else { 0.0 }
    $statusStr = ($uwbStatusCounts[$pairIdx].GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Key)=$($_.Value)" }) -join ", "

    if ($validCnt -gt 0) {
        $sorted = @($rList | Sort-Object)
        $median = if ($validCnt % 2 -eq 1) { $sorted[[int]($validCnt / 2)] } else { ($sorted[($validCnt / 2) - 1] + $sorted[$validCnt / 2]) / 2.0 }
        $min = $sorted[0]
        $max = $sorted[$validCnt - 1]
        Write-Host ("  Pair {0} ({1,-21}): {2,5} valid / {3,5} expected ({4,5:F1}% true yield) | Wire: {5,5} total ({6,5:F1}%) | Med: {7,3} cm (Min: {8,3}, Max: {9,3}) | {10}" -f $pairIdx, $pName, $validCnt, $expectedRounds, $trueYieldPct, $totalAttempts, $successPct, $median, $min, $max, $statusStr) -ForegroundColor Green
        $summaryRanges[$pairIdx] = [ordered]@{
            pair_name = $pName
            valid_count = $validCnt
            invalid_count = $invalidCnt
            total_attempts = $totalAttempts
            expected_rounds = $expectedRounds
            true_yield_pct = $trueYieldPct
            wire_success_pct = $successPct
            rate_hz = $rate
            median_cm = $median
            min_cm = $min
            max_cm = $max
            first_range_s = $firstRangeElapsed[$pairIdx]
            status_breakdown = $uwbStatusCounts[$pairIdx]
        }
    } else {
        Write-Host ("  Pair {0} ({1,-21}): {2,5} valid / {3,5} expected (  0.0% true yield) | Wire: {4,5} total (  0.0%) | NO VALID RANGING | {5}" -f $pairIdx, $pName, 0, $expectedRounds, $totalAttempts, $statusStr) -ForegroundColor Red
        $summaryRanges[$pairIdx] = [ordered]@{
            pair_name = $pName
            valid_count = 0
            invalid_count = $invalidCnt
            total_attempts = $totalAttempts
            expected_rounds = $expectedRounds
            true_yield_pct = 0.0
            wire_success_pct = 0.0
            rate_hz = 0
            median_cm = $null
            first_range_s = $null
            status_breakdown = $uwbStatusCounts[$pairIdx]
        }
    }
}

# Analyze Complete-Matrix Yield and Contiguous Dropout Streaks
$totalBlocksReceived = $starRangeBlocks.Count
$completeMatrixRounds = 0
$maxDropoutRounds = @{ 0 = 0; 1 = 0; 2 = 0; 3 = 0; 4 = 0 }
$currDropoutRounds = @{ 0 = 0; 1 = 0; 2 = 0; 3 = 0; 4 = 0 }

foreach ($blk in $starRangeBlocks) {
    $allFiveValid = $true
    for ($p = 0; $p -lt 5; $p++) {
        $pData = $blk.pairs[$p]
        $isValid = ($pData.range_cm -ge 0) -and (($pData.flags -band 1) -ne 0)
        if ($isValid) {
            $currDropoutRounds[$p] = 0
        } else {
            $allFiveValid = $false
            $currDropoutRounds[$p]++
            if ($currDropoutRounds[$p] -gt $maxDropoutRounds[$p]) {
                $maxDropoutRounds[$p] = $currDropoutRounds[$p]
            }
        }
    }
    if ($allFiveValid) {
        $completeMatrixRounds++
    }
}

$completeMatrixYieldPct = if ($expectedRounds -gt 0) { [math]::Round(($completeMatrixRounds * 100.0 / $expectedRounds), 2) } else { 0.0 }
$completeMatrixWirePct = if ($totalBlocksReceived -gt 0) { [math]::Round(($completeMatrixRounds * 100.0 / $totalBlocksReceived), 2) } else { 0.0 }

Write-Host "`n--- STAR MATRIX YIELD & DROPOUT ANALYSIS ---" -ForegroundColor Yellow
Write-Host ("  Complete 5-Node Matrix Rounds : {0} / {1} expected ({2:F2}% Complete Yield) | {3} received ({4:F2}% wire)" -f `
    $completeMatrixRounds, $expectedRounds, $completeMatrixYieldPct, $totalBlocksReceived, $completeMatrixWirePct) -ForegroundColor $(if ($completeMatrixYieldPct -ge 95.0) { 'Green' } else { 'Yellow' })

foreach ($p in 0..4) {
    $pName = if ($pairNames.ContainsKey($p)) { $pairNames[$p] } else { "Pair $p" }
    $maxRounds = $maxDropoutRounds[$p]
    $maxSec = [math]::Round($maxRounds * ($BlockDurationMs / 1000.0), 2)
    Write-Host ("  Pair {0} ({1,-21}): Max Contiguous Dropout: {2,5} rounds ({3,6:F2} s)" -f $p, $pName, $maxRounds, $maxSec)
    if ($summaryRanges.ContainsKey($p)) {
        $summaryRanges[$p]["max_contiguous_dropout_rounds"] = $maxRounds
        $summaryRanges[$p]["max_contiguous_dropout_s"] = $maxSec
    }
}

Write-Host "`n--- UWB HEALTH & DIAGNOSTICS (PER NODE) ---" -ForegroundColor Yellow
$diagByNode = $diagnostics | Group-Object source_node
foreach ($grp in ($diagByNode | Sort-Object { [int]$_.Name })) {
    $nIdx = [int]$grp.Name
    $nName = if ($nodeNames.ContainsKey($nIdx)) { $nodeNames[$nIdx] } else { "Node $nIdx" }
    $lateTx = @($grp.Group | Where-Object { $_.stage_name -eq 'dw3000_late_tx_count' })
    $minLead = @($grp.Group | Where-Object { $_.stage_name -eq 'fira_lifetime_min_lead' })
    # Periodic health 0xB3 metric has detail as raw count (typically <= 65535), whereas boot snapshot packs reset reason in top byte
    $stallRec = @($grp.Group | Where-Object { $_.stage_name -eq 'fira_stall_recovery_count' -and $_.detail -ge 0 -and $_.detail -le 65535 })

    $lateTxVal = if ($lateTx.Count -gt 0) { $lateTx[-1].detail } else { 'N/A' }
    $minLeadVal = if ($minLead.Count -gt 0) { ("{0} DTU ({1:F1} us)" -f $minLead[-1].detail, ($minLead[-1].detail / 249.6)) } else { 'N/A' }
    $stallRecVal = if ($stallRec.Count -gt 0) { $stallRec[-1].detail } else { 'N/A' }

    $startLateTx = if ($baselineDiags.ContainsKey("$nIdx-dw3000_late_tx_count")) { $baselineDiags["$nIdx-dw3000_late_tx_count"] } else { $lateTxVal }
    $endLateTx = if ($finalDiags.ContainsKey("$nIdx-dw3000_late_tx_count")) { $finalDiags["$nIdx-dw3000_late_tx_count"] } else { $lateTxVal }
    $deltaLateTx = if ($startLateTx -ne 'N/A' -and $endLateTx -ne 'N/A') { ($endLateTx - $startLateTx) } else { 0 }

    $startRec = if ($baselineDiags.ContainsKey("$nIdx-fira_stall_recovery_count") -and $baselineDiags["$nIdx-fira_stall_recovery_count"] -le 65535) { $baselineDiags["$nIdx-fira_stall_recovery_count"] } else { $stallRecVal }
    $endRec = if ($finalDiags.ContainsKey("$nIdx-fira_stall_recovery_count") -and $finalDiags["$nIdx-fira_stall_recovery_count"] -le 65535) { $finalDiags["$nIdx-fira_stall_recovery_count"] } else { $stallRecVal }
    $deltaRec = if ($startRec -ne 'N/A' -and $endRec -ne 'N/A') { ($endRec - $startRec) } else { 0 }

    $bootSnap = @($grp.Group | Where-Object { $_.stage_name -eq 'fira_stall_recovery_count' -and ($_.detail -gt 65535 -or $_.detail -lt 0) })
    $bootCountVal = if ($bootSnap.Count -gt 0) { $bootSnap[-1].sub_code } else { 'N/A' }
    $resetReasVal = if ($bootSnap.Count -gt 0) { ("0x{0:X2}" -f (($bootSnap[-1].detail -shr 24) -band 0xFF)) } else { 'N/A' }

    $sysLo = @($grp.Group | Where-Object { $_.stage_name -eq 'dw3000_sys_status_lo' })
    $sysHi = @($grp.Group | Where-Object { $_.stage_name -eq 'dw3000_sys_status_hi' })
    $sysStr = if ($sysLo.Count -gt 0) {
        $loVal = [uint32]$sysLo[-1].detail
        $hiVal = if ($sysHi.Count -gt 0) { [uint32]$sysHi[-1].detail } else { 0 }
        (" | DW3000 Status: 0x{0:X2}_{1:X8}" -f $hiVal, $loVal)
    } else { "" }

    Write-Host ("  Node {0} ({1,-11}): Boot: {2,2} | Reset: {3,-4} | Late TX: {4} (d={5}) | Min Lead: {6,-22} | Recs: {7} (d={8}){9}" -f `
        $nIdx, $nName, $bootCountVal, $resetReasVal, $endLateTx, $deltaLateTx, $minLeadVal, $endRec, $deltaRec, $sysStr)
}

$imuCountsStr = [ordered]@{}
foreach ($k in $imuCounts.Keys) { $imuCountsStr["Node_$k"] = $imuCounts[$k] }

$firstRangeStr = [ordered]@{}
foreach ($k in $firstRangeElapsed.Keys) { $firstRangeStr["Pair_$k"] = $firstRangeElapsed[$k] }

$firstPelvisUptimeStr = [ordered]@{}
foreach ($k in $firstPelvisUptime.Keys) { $firstPelvisUptimeStr["Pair_$k"] = $firstPelvisUptime[$k] }

$firstControleeUptimeStr = [ordered]@{}
foreach ($k in $firstControleeUptime.Keys) { $firstControleeUptimeStr["Pair_$k"] = $firstControleeUptime[$k] }

$summaryRangesStr = [ordered]@{}
foreach ($k in $summaryRanges.Keys) { $summaryRangesStr["Pair_$k"] = $summaryRanges[$k] }

$nodeStatusLatestStr = [ordered]@{}
foreach ($k in $nodeStatusLatest.Keys) { $nodeStatusLatestStr["Node_$k"] = $nodeStatusLatest[$k] }

$report = [ordered]@{
    test = "6-node star benchmark"
    port = $Port
    timestamp = $nowIso
    duration_s = $DurationSeconds
    active_duration_s = $activeDuration
    complete_matrix_rounds = $completeMatrixRounds
    complete_matrix_yield_pct = $completeMatrixYieldPct
    complete_matrix_wire_pct = $completeMatrixWirePct
    imu_counts = $imuCountsStr
    node_status_latest = $nodeStatusLatestStr
    uwb_summary = $summaryRangesStr
    first_range_elapsed = $firstRangeStr
    first_pelvis_uptime_s = $firstPelvisUptimeStr
    first_controlee_uptime_s = $firstControleeUptimeStr
    star_range_blocks = $starRangeBlocks
    diagnostics = $diagnostics
    stall_snapshots = $stallSnapshots
    baseline_diagnostics = $baselineDiags
    final_diagnostics = $finalDiags
}

$report | ConvertTo-Json -Depth 6 | Out-File -FilePath $outputPath -Encoding utf8
Write-Host "`nSaved benchmark record to: $outputPath" -ForegroundColor Cyan
