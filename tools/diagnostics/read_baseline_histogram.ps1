param(
    [int]$RunSeconds = 60
)

function Parse-IntelHex {
    param([string]$FilePath)
    $mem = @{}
    $base = 0
    Get-Content $FilePath | ForEach-Object {
        $line = $_.Trim()
        if ($line.StartsWith(':')) {
            $byteCount = [Convert]::ToInt32($line.Substring(1, 2), 16)
            $addr = [Convert]::ToInt32($line.Substring(3, 4), 16)
            $type = [Convert]::ToInt32($line.Substring(7, 2), 16)
            $dataStr = $line.Substring(9, $byteCount * 2)

            if ($type -eq 0) {
                $fullAddr = $base + $addr
                for ($i = 0; $i -lt $byteCount; $i++) {
                    $mem[$fullAddr + $i] = [Convert]::ToByte($dataStr.Substring($i * 2, 2), 16)
                }
            } elseif ($type -eq 4) {
                $base = [Convert]::ToInt32($dataStr, 16) -shl 16
            }
        }
    }
    return $mem
}

function Read-U8($mem, [int]$addr) {
    if ($mem.ContainsKey($addr)) { return $mem[$addr] }
    return 0
}

function Read-U16($mem, [int]$addr) {
    $bytes = [byte[]]@(0..1 | ForEach-Object { Read-U8 $mem ($addr + $_) })
    return [BitConverter]::ToUInt16($bytes, 0)
}

function Read-U32($mem, [int]$addr) {
    $bytes = [byte[]]@(0..3 | ForEach-Object { Read-U8 $mem ($addr + $_) })
    return [BitConverter]::ToUInt32($bytes, 0)
}

function Read-I32($mem, [int]$addr) {
    $bytes = [byte[]]@(0..3 | ForEach-Object { Read-U8 $mem ($addr + $_) })
    return [BitConverter]::ToInt32($bytes, 0)
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Phase 1: High-Precision Baseline UWB Profiling ($RunSeconds s)" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# Safe flash HEAD (760225805)
Write-Host "Flashing HEAD (SN: 760225805)..." -ForegroundColor Yellow
nrfjprog --snr 760225805 --program "C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_HEAD_v5\DWM3001CDK-COMP971-FreeRTOS.hex" --sectorerase --verify | Out-Null

# Safe flash PELVIS (760224787)
Write-Host "Flashing PELVIS (SN: 760224787)..." -ForegroundColor Yellow
nrfjprog --snr 760224787 --program "C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_PELVIS_v5\DWM3001CDK-COMP971-FreeRTOS.hex" --sectorerase --verify | Out-Null

# Synchronous Reset: Reset both nodes simultaneously so they start together
Write-Host "Issuing synchronized reset to both nodes..." -ForegroundColor Yellow
$sw = [System.Diagnostics.Stopwatch]::StartNew()
nrfjprog --snr 760225805 --reset | Out-Null
nrfjprog --snr 760224787 --reset | Out-Null

Write-Host "Both nodes running unhindered without SWD attachment for $RunSeconds seconds..." -ForegroundColor Green
Start-Sleep -Seconds $RunSeconds

Write-Host "Sampling final histogram from RAM..." -ForegroundColor Yellow
nrfjprog --snr 760225805 --readram hardware/outputs/records/ram_hist_head.hex | Out-Null
nrfjprog --snr 760225805 --run | Out-Null
nrfjprog --snr 760224787 --readram hardware/outputs/records/ram_hist_pelvis.hex | Out-Null
nrfjprog --snr 760224787 --run | Out-Null
$sw.Stop()
$actualSeconds = [Math]::Round($sw.Elapsed.TotalSeconds, 2)

$headMem = Parse-IntelHex "hardware/outputs/records/ram_hist_head.hex"
$pelvisMem = Parse-IntelHex "hardware/outputs/records/ram_hist_pelvis.hex"

# Function to extract symbol map from ELF
function Get-ElfSymbols {
    param([string]$ElfPath)
    $syms = @{}
    $lines = & arm-none-eabi-nm -n $ElfPath
    foreach ($line in $lines) {
        $parts = $line.Trim() -split '\s+'
        if ($parts.Count -ge 3) {
            $addr = [Convert]::ToInt32($parts[0], 16)
            $name = $parts[2]
            $syms[$name] = $addr
        }
    }
    return $syms
}

$headSyms = Get-ElfSymbols "C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_HEAD_v5\DWM3001CDK-COMP971-FreeRTOS.elf"
$pelvisSyms = Get-ElfSymbols "C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_PELVIS_v5\DWM3001CDK-COMP971-FreeRTOS.elf"

# HEAD Metrics
$hTotal = Read-U32 $headMem $headSyms['g_fira_hist_total_blocks']
$hSucc = Read-U32 $headMem $headSyms['g_fira_hist_success']
$hTxFailed = Read-U32 $headMem $headSyms['g_fira_hist_tx_failed']
$hRxTo = Read-U32 $headMem $headSyms['g_fira_hist_rx_timeout']
$hRxPhy = Read-U32 $headMem $headSyms['g_fira_hist_rx_phy_dec']
$hOther = Read-U32 $headMem $headSyms['g_fira_hist_other']
$hFirstBlk = Read-U32 $headMem $headSyms['g_fira_first_block_index']
$hLastBlk = Read-U32 $headMem $headSyms['g_fira_last_block_index']
$hTxCount = Read-U32 $headMem $headSyms['g_dw3000_tx_count']
$hMinLead = Read-I32 $headMem $headSyms['g_dw3000_min_tx_diff']
$hLateCount = Read-U32 $headMem $headSyms['g_dw3000_late_tx_count']
$hStalls = Read-U32 $headMem $headSyms['g_fira_stall_recovery_count']
$hDist = Read-I32 $headMem $headSyms['m_latest_report']
$hValid = Read-U8 $headMem ($headSyms['m_latest_report'] + 6)
$hTlCount = Read-U16 $headMem $headSyms['g_fira_timeline_count']
$hSteadyTotal = Read-U32 $headMem $headSyms['g_fira_steady_total']
$hSteadyFail = Read-U32 $headMem $headSyms['g_fira_steady_fail_count']
$hStartTick = Read-U32 $headMem $headSyms['g_fira_session_start_tick']

# PELVIS Metrics
$pTotal = Read-U32 $pelvisMem $pelvisSyms['g_fira_hist_total_blocks']
$pSucc = Read-U32 $pelvisMem $pelvisSyms['g_fira_hist_success']
$pTxFailed = Read-U32 $pelvisMem $pelvisSyms['g_fira_hist_tx_failed']
$pRxTo = Read-U32 $pelvisMem $pelvisSyms['g_fira_hist_rx_timeout']
$pRxPhy = Read-U32 $pelvisMem $pelvisSyms['g_fira_hist_rx_phy_dec']
$pOther = Read-U32 $pelvisMem $pelvisSyms['g_fira_hist_other']
$pFirstBlk = Read-U32 $pelvisMem $pelvisSyms['g_fira_first_block_index']
$pLastBlk = Read-U32 $pelvisMem $pelvisSyms['g_fira_last_block_index']
$pTxCount = Read-U32 $pelvisMem $pelvisSyms['g_dw3000_tx_count']
$pMinLead = Read-I32 $pelvisMem $pelvisSyms['g_dw3000_min_tx_diff']
$pLateCount = Read-U32 $pelvisMem $pelvisSyms['g_dw3000_late_tx_count']
$pStalls = Read-U32 $pelvisMem $pelvisSyms['g_fira_stall_recovery_count']
$pTlCount = Read-U16 $pelvisMem $pelvisSyms['g_fira_timeline_count']
$pSteadyTotal = Read-U32 $pelvisMem $pelvisSyms['g_fira_steady_total']
$pSteadyFail = Read-U32 $pelvisMem $pelvisSyms['g_fira_steady_fail_count']
$pStartTick = Read-U32 $pelvisMem $pelvisSyms['g_fira_session_start_tick']

Write-Host ""
Write-Host "=== CONTROLLER / INITIATOR (HEAD) HISTOGRAM ===" -ForegroundColor Cyan
Write-Host "Total Elapsed Time:     $actualSeconds s"
Write-Host "Session Start Tick:     $hStartTick ms"
Write-Host "Block Index Range:      $hFirstBlk -> $hLastBlk (Span: $(if ($hLastBlk -ge $hFirstBlk) { $hLastBlk - $hFirstBlk + 1 } else { 0 }))"
if ($actualSeconds -gt 0) {
    Write-Host "Observed Block Rate:    $([Math]::Round($hTotal / $actualSeconds, 2)) blocks/sec (Nominal: 5.0 blocks/sec @ 200ms)"
}
Write-Host "Total Ranging Blocks:   $hTotal"
Write-Host "SUCCESS (Status 0):     $hSucc"
Write-Host "TX_FAILED (Status 32):  $hTxFailed"
Write-Host "RX_TIMEOUT (Status 33): $hRxTo"
Write-Host "RX_PHY_DEC (Status 34): $hRxPhy"
Write-Host "Other Status:           $hOther"
if ($hTotal -gt 0) {
    $pct = [Math]::Round(($hSucc / $hTotal) * 100, 2)
    Write-Host "Success Rate:           $pct %" -ForegroundColor Yellow
}
Write-Host "Steady-State Window:    $hSteadyTotal blocks evaluated (>= Blk 10)"
Write-Host "Steady-State Failures:  $hSteadyFail" -ForegroundColor $(if ($hSteadyFail -eq 0) { 'Green' } else { 'Red' })
Write-Host "Total Radio Transmits:  $hTxCount"
Write-Host "Min TX Lead Margin:     $hMinLead DTU ($([Math]::Round($hMinLead / 15.6, 1)) us)"
Write-Host "Late TX Count:          $hLateCount"
Write-Host "Stall Recoveries:       $hStalls"
Write-Host "Last Distance:          $hDist cm (valid=$hValid)"

# Analyze HEAD timeline for failure clustering
Write-Host "Timeline Sample ($hTlCount blocks recorded):" -ForegroundColor DarkCyan
$hFailures = @()
$hTlBase = $headSyms['g_fira_status_timeline']
for ($i = 0; $i -lt [Math]::Min([int]$hTlCount, 350); $i++) {
    $st = Read-U8 $headMem ($hTlBase + $i)
    if ($st -ne 0) {
        $hFailures += "Blk ${i}: Status ${st}"
    }
}
if ($hFailures.Count -eq 0) {
    Write-Host "  -> ALL recorded blocks were SUCCESS (Status 0)!" -ForegroundColor Green
} else {
    Write-Host "  -> Non-success blocks ($($hFailures.Count)): $($hFailures -join ', ')" -ForegroundColor Yellow
}
if ($headSyms.ContainsKey('g_fira_fail_block_indices') -and $headSyms.ContainsKey('g_fira_fail_ring_count')) {
    $hRingCount = Read-U8 $headMem $headSyms['g_fira_fail_ring_count']
    $hFailEntries = @()
    $hRingLimit = [Math]::Min([int]$hRingCount, 32)
    for ($i = 0; $i -lt $hRingLimit; $i++) {
        $bIdx = Read-U32 $headMem ($headSyms['g_fira_fail_block_indices'] + ($i * 4))
        $bSt = Read-U8 $headMem ($headSyms['g_fira_fail_statuses'] + $i)
        $hFailEntries += "Blk $bIdx (Status $bSt)"
    }
    if ($hFailEntries.Count -gt 0) {
        Write-Host "  -> Failure Ring Buffer ($hRingCount total): $($hFailEntries -join ', ')" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "=== RESPONDER / CONTROLEE (PELVIS) HISTOGRAM ===" -ForegroundColor Cyan
Write-Host "Session Start Tick:     $pStartTick ms"
Write-Host "Block Index Range:      $pFirstBlk -> $pLastBlk (Span: $(if ($pLastBlk -ge $pFirstBlk) { $pLastBlk - $pFirstBlk + 1 } else { 0 }))"
if ($actualSeconds -gt 0) {
    Write-Host "Observed Block Rate:    $([Math]::Round($pTotal / $actualSeconds, 2)) blocks/sec (Nominal: 5.0 blocks/sec @ 200ms)"
}
Write-Host "Total Ranging Blocks:   $pTotal"
Write-Host "SUCCESS (Status 0):     $pSucc"
Write-Host "TX_FAILED (Status 32):  $pTxFailed"
Write-Host "RX_TIMEOUT (Status 33): $pRxTo"
Write-Host "RX_PHY_DEC (Status 34): $pRxPhy"
Write-Host "Other Status:           $pOther"
if ($pTotal -gt 0) {
    $pPct = [Math]::Round(($pSucc / $pTotal) * 100, 2)
    Write-Host "Success Rate:           $pPct %" -ForegroundColor Yellow
}
Write-Host "Steady-State Window:    $pSteadyTotal blocks evaluated (>= Blk 10)"
Write-Host "Steady-State Failures:  $pSteadyFail" -ForegroundColor $(if ($pSteadyFail -eq 0) { 'Green' } else { 'Red' })
Write-Host "Total Radio Transmits:  $pTxCount"
Write-Host "Min TX Lead Margin:     $pMinLead DTU ($([Math]::Round($pMinLead / 15.6, 1)) us)"
Write-Host "Late TX Count:          $pLateCount"
Write-Host "Stall Recoveries:       $pStalls"

# Analyze PELVIS timeline for failure clustering
Write-Host "Timeline Sample ($pTlCount blocks recorded):" -ForegroundColor DarkCyan
$pFailures = @()
$pTlBase = $pelvisSyms['g_fira_status_timeline']
for ($i = 0; $i -lt [Math]::Min([int]$pTlCount, 350); $i++) {
    $st = Read-U8 $pelvisMem ($pTlBase + $i)
    if ($st -ne 0) {
        $pFailures += "Blk ${i}: Status ${st}"
    }
}
if ($pFailures.Count -eq 0) {
    Write-Host "  -> ALL recorded blocks were SUCCESS (Status 0)!" -ForegroundColor Green
} else {
    Write-Host "  -> Non-success blocks ($($pFailures.Count)): $($pFailures -join ', ')" -ForegroundColor Yellow
}
if ($pelvisSyms.ContainsKey('g_fira_fail_block_indices') -and $pelvisSyms.ContainsKey('g_fira_fail_ring_count')) {
    $pRingCount = Read-U8 $pelvisMem $pelvisSyms['g_fira_fail_ring_count']
    $pFailEntries = @()
    $pRingLimit = [Math]::Min([int]$pRingCount, 32)
    for ($i = 0; $i -lt $pRingLimit; $i++) {
        $bIdx = Read-U32 $pelvisMem ($pelvisSyms['g_fira_fail_block_indices'] + ($i * 4))
        $bSt = Read-U8 $pelvisMem ($pelvisSyms['g_fira_fail_statuses'] + $i)
        $pFailEntries += "Blk $bIdx (Status $bSt)"
    }
    if ($pFailEntries.Count -gt 0) {
        Write-Host "  -> Failure Ring Buffer ($pRingCount total): $($pFailEntries -join ', ')" -ForegroundColor Yellow
    }
}
Write-Host "==========================================================" -ForegroundColor Cyan
