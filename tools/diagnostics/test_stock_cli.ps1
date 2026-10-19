param(
    [string]$Port1 = 'COM6',
    [string]$Port2 = 'COM13'
)

$ErrorActionPreference = 'Stop'

function Probe-Port($port) {
    try {
        $sp = [System.IO.Ports.SerialPort]::new($port, 115200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
        $sp.DtrEnable = $true
        $sp.RtsEnable = $true
        $sp.ReadTimeout = 500
        $sp.NewLine = "`r`n"
        $sp.Open()
        $sp.Write("`r`nhelp`r`n")
        Start-Sleep -Milliseconds 400
        $resp = ""
        while ($sp.BytesToRead -gt 0) {
            $resp += $sp.ReadExisting()
        }
        $sp.Close()
        $sp.Dispose()
        return $resp
    } catch {
        return "ERROR: $_"
    }
}

Write-Host "Probing Port 1 ($Port1)..." -ForegroundColor Cyan
$r1 = Probe-Port $Port1
Write-Host "Response from ${Port1}:`n$r1"

Write-Host "Probing Port 2 ($Port2)..." -ForegroundColor Cyan
$r2 = Probe-Port $Port2
Write-Host "Response from ${Port2}:`n$r2"
