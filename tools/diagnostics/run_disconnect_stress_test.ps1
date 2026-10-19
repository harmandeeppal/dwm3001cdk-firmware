param(
    [string]$Port = 'COM16',
    [int]$TotalCycles = 60,
    [int]$CycleCadenceSeconds = 5,
    [int]$SettlingSeconds = 2,
    [string]$OutputDirectory = 'C:\comp971_raw_captures'
)

$ErrorActionPreference = 'Stop'
$captureId = Get-Date -Format 'yyyy-MM-dd_HHmmss'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$dataPath = Join-Path $OutputDirectory ("gateway_disconnect_stress_{0}.bin" -f $captureId)
$metadataPath = Join-Path $OutputDirectory ("gateway_disconnect_stress_{0}.json" -f $captureId)

# Memory addresses from current ELF build
$headForceDiscAddr = "0x20004600"
$pelvisForceDiscAddr = "0x20004600"
$headSnr = "760225805"
$pelvisSnr = "760224787"

# Total duration: Initial 15s steady state + cycles * cadence + 45s post-test steady state
$totalDuration = 15 + ($TotalCycles * $CycleCadenceSeconds) + 45

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " 60-Cycle BLE Disconnect & Reconnect Stress Test ($totalDuration s)" -ForegroundColor Cyan
Write-Host " Target Nodes: HEAD ($headSnr), PELVIS ($pelvisSnr)" -ForegroundColor Cyan
Write-Host " Cadence: 1 disconnect event every $CycleCadenceSeconds seconds" -ForegroundColor Cyan
Write-Host " Capture Output: $dataPath" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# Launch capture_gateway_high_perf.py in background so USB capture runs in dedicated thread without blocking on nrfjprog CLI
$pyExe = "C:\Users\harma\AppData\Local\DWMTools\platformio-venv2\Scripts\python.exe"
$captureScript = "C:\comp971_raw_captures\capture_gateway_high_perf.py"

$captureProc = Start-Process -FilePath $pyExe `
    -ArgumentList @($captureScript, "--port", $Port, "--duration", "$totalDuration", "--settling", "$SettlingSeconds", "--output", $dataPath) `
    -PassThru -NoNewWindow

Start-Sleep -Seconds ($SettlingSeconds + 1)

$start = Get-Date
$nextInjectionTime = $start.AddSeconds(15)
$cycle = 0

function Trigger-Disconnect([string]$snr, [string]$addr) {
    try {
        $p = Start-Process -FilePath "nrfjprog" -ArgumentList "--snr", $snr, "--family", "NRF52", "--ramwr", $addr, "--val", "1" -NoNewWindow -PassThru
        if (-not $p.WaitForExit(3000)) {
            $p.Kill()
            Write-Warning "nrfjprog timed out for SNR $snr"
        }
    } catch {
        Write-Warning "Failed to execute nrfjprog: $_"
    }
}

Write-Host "Recording live gateway stream while executing $TotalCycles disconnect cycles..." -ForegroundColor Green

while ($cycle -lt $TotalCycles) {
    $now = Get-Date
    if ($now -ge $nextInjectionTime) {
        $cycle++
        $elapsedSec = [Math]::Round(($now - $start).TotalSeconds, 1)

        # Cycle pattern:
        # Cycles 1..25: Alternating node-initiated disconnects (odd = HEAD, even = PELVIS)
        # Cycles 26..50: Simultaneous dual-node disconnects
        # Cycles 51..59: Rapid alternating disconnects
        # Cycle 60: Final dual disconnect
        if ($cycle -le 25) {
            if ($cycle % 2 -eq 1) {
                Write-Host "[$elapsedSec s] Cycle $cycle/${TotalCycles}: Node disconnect on HEAD ($headSnr)..." -ForegroundColor Yellow
                Trigger-Disconnect $headSnr $headForceDiscAddr
            } else {
                Write-Host "[$elapsedSec s] Cycle $cycle/${TotalCycles}: Node disconnect on PELVIS ($pelvisSnr)..." -ForegroundColor Yellow
                Trigger-Disconnect $pelvisSnr $pelvisForceDiscAddr
            }
        } elseif ($cycle -le 50) {
            Write-Host "[$elapsedSec s] Cycle $cycle/${TotalCycles}: Dual-node disconnect (HEAD + PELVIS)..." -ForegroundColor Magenta
            Trigger-Disconnect $headSnr $headForceDiscAddr
            Trigger-Disconnect $pelvisSnr $pelvisForceDiscAddr
        } else {
            if ($cycle -eq 60) {
                Write-Host "[$elapsedSec s] Cycle $cycle/${TotalCycles}: Final dual-node disconnect..." -ForegroundColor Magenta
                Trigger-Disconnect $headSnr $headForceDiscAddr
                Trigger-Disconnect $pelvisSnr $pelvisForceDiscAddr
            } else {
                Write-Host "[$elapsedSec s] Cycle $cycle/${TotalCycles}: Rapid alternating disconnect..." -ForegroundColor Yellow
                $targetSnr = if ($cycle % 2 -eq 1) { $headSnr } else { $pelvisSnr }
                $targetAddr = if ($cycle % 2 -eq 1) { $headForceDiscAddr } else { $pelvisForceDiscAddr }
                Trigger-Disconnect $targetSnr $targetAddr
            }
        }

        $nextInjectionTime = $now.AddSeconds($CycleCadenceSeconds)
    }
    Start-Sleep -Milliseconds 200
}

Write-Host "All $TotalCycles disconnect cycles completed. Waiting for post-test capture window..." -ForegroundColor Cyan
if (-not $captureProc.WaitForExit(60000)) {
    $captureProc.Kill()
    Write-Warning "Capture process timed out after test completion and was stopped."
}

Write-Host "Disconnect stress test complete. Raw capture saved to: $dataPath" -ForegroundColor Green
