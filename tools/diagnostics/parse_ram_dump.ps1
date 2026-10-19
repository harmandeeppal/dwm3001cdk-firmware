param(
    [string]$HeadHex = "$PSScriptRoot/../../outputs/records/diagnostics/ram_dumps_2026-10-05/ram_head.hex",
    [string]$PelvisHex = "$PSScriptRoot/../../outputs/records/diagnostics/ram_dumps_2026-10-05/ram_pelvis.hex"
)

function Parse-IntelHex {
    param([string]$FilePath)
    $mem = @{}
    $base = 0
    if (-not (Test-Path $FilePath)) {
        Write-Warning "Hex file not found: $FilePath"
        return $mem
    }
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

function Read-U32($mem, [int]$addr) {
    $bytes = [byte[]]@(0..3 | ForEach-Object { Read-U8 $mem ($addr + $_) })
    return [BitConverter]::ToUInt32($bytes, 0)
}

function Read-I32($mem, [int]$addr) {
    $bytes = [byte[]]@(0..3 | ForEach-Object { Read-U8 $mem ($addr + $_) })
    return [BitConverter]::ToInt32($bytes, 0)
}

Write-Host "=== HEAD NODE (Role: Controller / Initiator) ==="
$headMem = Parse-IntelHex $HeadHex
Write-Host "m_cb_call_count:          $((Read-U32 $headMem 0x20004c74))"
Write-Host "m_cb_last_type:           $((Read-U32 $headMem 0x20004c78))"
Write-Host "m_session_state:          $((Read-U8 $headMem 0x20004db4)) (0=INIT, 1=DEINIT, 2=ACTIVE, 3=IDLE)"
Write-Host "m_cb_last_status:         $((Read-I32 $headMem 0x20004080)) (0=OK, 32=NO_RESP, 33=RX_TIMEOUT, 34=TX_FAILED)"
Write-Host "m_cb_last_n_meas:         $((Read-I32 $headMem 0x20004078))"
Write-Host "m_cb_last_block_index:    $((Read-I32 $headMem 0x20004064))"
Write-Host "m_cb_last_slot_index:     $((Read-U8 $headMem 0x2000407e))"
Write-Host "m_cb_last_stopped:        $((Read-U8 $headMem 0x20004084))"
Write-Host "m_cb_last_diag_action:    $((Read-U8 $headMem 0x20004068))"
Write-Host "m_cb_last_diag_extra_stat:$((Read-U8 $headMem 0x2000406c))"
Write-Host "g_dw3000_tx_count:        $((Read-U32 $headMem 0x20012b1c))"
Write-Host "g_dw3000_last_tx_status:  $((Read-I32 $headMem 0x20012b18))"
Write-Host "g_dw3000_last_tx_diff:    $((Read-I32 $headMem 0x20012b14))"
$repDist = Read-I32 $headMem 0x20004ccc
$repStat = Read-U8 $headMem 0x20004cd0
$repValid = Read-U8 $headMem 0x20004cd2
Write-Host "m_latest_report:          valid=$repValid, dist=$repDist cm, status=$repStat"

Write-Host ""
Write-Host "=== PELVIS NODE (Role: Controlee / Responder) ==="
$pelvisMem = Parse-IntelHex $PelvisHex
Write-Host "m_cb_call_count:          $((Read-U32 $pelvisMem 0x20004c74))"
Write-Host "m_cb_last_type:           $((Read-U32 $pelvisMem 0x20004c78))"
Write-Host "m_session_state:          $((Read-U8 $pelvisMem 0x20004dac)) (0=INIT, 1=DEINIT, 2=ACTIVE, 3=IDLE)"
Write-Host "m_cb_last_status:         $((Read-I32 $pelvisMem 0x20004080)) (0=OK, 32=NO_RESP, 33=RX_TIMEOUT, 34=TX_FAILED)"
Write-Host "m_cb_last_n_meas:         $((Read-I32 $pelvisMem 0x20004078))"
Write-Host "m_cb_last_block_index:    $((Read-I32 $pelvisMem 0x20004064))"
Write-Host "g_dw3000_tx_count:        $((Read-U32 $pelvisMem 0x20012b14))"
