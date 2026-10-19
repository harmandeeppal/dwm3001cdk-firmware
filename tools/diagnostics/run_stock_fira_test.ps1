param(
    [string]$RespPort = 'COM6',
    [string]$InitPort = 'COM13',
    [int]$DurationSeconds = 10
)

$ErrorActionPreference = 'Stop'

Write-Host "Opening ports: Responder on $RespPort, Controller on $InitPort" -ForegroundColor Cyan

$spResp = [System.IO.Ports.SerialPort]::new($RespPort, 115200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$spInit = [System.IO.Ports.SerialPort]::new($InitPort, 115200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)

$spResp.DtrEnable = $true
$spResp.RtsEnable = $true
$spResp.ReadTimeout = 200

$spInit.DtrEnable = $true
$spInit.RtsEnable = $true
$spInit.ReadTimeout = 200

try {
    $spResp.Open()
    $spInit.Open()

    # Clear initial buffers
    Start-Sleep -Milliseconds 200
    while ($spResp.BytesToRead -gt 0) { $null = $spResp.ReadExisting() }
    while ($spInit.BytesToRead -gt 0) { $null = $spInit.ReadExisting() }

    # Start Responder first
    Write-Host "Starting Responder on $RespPort (RESPF)..." -ForegroundColor Yellow
    $spResp.Write("RESPF`r`n")
    Start-Sleep -Milliseconds 300

    # Start Controller
    Write-Host "Starting Controller on $InitPort (INITF)..." -ForegroundColor Yellow
    $spInit.Write("INITF`r`n")

    $respLog = ""
    $initLog = ""

    $start = Get-Date
    Write-Host "Monitoring output for $DurationSeconds seconds..." -ForegroundColor Cyan
    while (((Get-Date) - $start).TotalSeconds -lt $DurationSeconds) {
        try {
            if ($spResp.BytesToRead -gt 0) {
                $text = $spResp.ReadExisting()
                $respLog += $text
                Write-Host "[RESP $RespPort] $text" -NoNewline -ForegroundColor Green
            }
        } catch {}

        try {
            if ($spInit.BytesToRead -gt 0) {
                $text = $spInit.ReadExisting()
                $initLog += $text
                Write-Host "[INIT $InitPort] $text" -NoNewline -ForegroundColor Magenta
            }
        } catch {}

        Start-Sleep -Milliseconds 50
    }

    # Stop both sessions
    Write-Host "`nStopping sessions..." -ForegroundColor Yellow
    $spResp.Write("STOP`r`n")
    $spInit.Write("STOP`r`n")
    Start-Sleep -Milliseconds 300

} finally {
    if ($spResp.IsOpen) { $spResp.Close() }
    if ($spInit.IsOpen) { $spInit.Close() }
    $spResp.Dispose()
    $spInit.Dispose()
}

Write-Host "`n=== SUMMARY ===" -ForegroundColor Cyan
$initCount = if ($initRanges) { $initRanges.Count } else { 0 }
$respCount = if ($respRanges) { $respRanges.Count } else { 0 }
Write-Host "Controller distance measurements found: $initCount"
if ($initCount -gt 0) {
    foreach ($m in $initRanges | Select-Object -First 10) {
        Write-Host "  -> Measured: $($m.Value)" -ForegroundColor Green
    }
}
Write-Host "Responder distance measurements found: $respCount"
if ($respCount -gt 0) {
    foreach ($m in $respRanges | Select-Object -First 10) {
        Write-Host "  -> Measured: $($m.Value)" -ForegroundColor Green
    }
}
