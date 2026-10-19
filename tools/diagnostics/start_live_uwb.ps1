param(
    [Parameter(Mandatory = $true)]
    [string]$Port,
    [Parameter(Mandatory = $true)]
    [string]$DeviceId,
    [Parameter(Mandatory = $true)]
    [string]$PeerDeviceId,
    [string]$PythonPath
)

$ErrorActionPreference = "Stop"
$hardwareRoot = Split-Path -Parent $PSScriptRoot
$registry = Get-Content -LiteralPath (Join-Path $hardwareRoot "config\device_registry.json") -Raw |
    ConvertFrom-Json
$telemetryProfile = Get-Content -LiteralPath (Join-Path $hardwareRoot "config\profiles\six_node_telemetry_v1.json") -Raw |
    ConvertFrom-Json
$source = $registry.devices | Where-Object { $_.device_id -eq $DeviceId }
$peer = $registry.devices | Where-Object { $_.device_id -eq $PeerDeviceId }
if ($null -eq $source -or $null -eq $peer) {
    throw "DeviceId and PeerDeviceId must both be registered in config/device_registry.json."
}
if ($source.status -ne "registered" -or $peer.status -ne "registered") {
    throw "Both selected devices must have status=registered."
}

if (-not $PythonPath) {
    $python = Get-Command python -ErrorAction SilentlyContinue
    if ($null -eq $python) {
        throw "Python was not found. Pass -PythonPath to an environment with pyserial installed."
    }
    $PythonPath = $python.Source
}
if (-not (Test-Path -LiteralPath $PythonPath)) {
    throw "Python executable was not found: $PythonPath"
}

$readerPath = Join-Path $PSScriptRoot "read_uwb_range.py"
Write-Host "Live UWB viewer"
Write-Host "Source: $($source.permanent_name)"
Write-Host "Peer:   $($peer.permanent_name)"
Write-Host "Port:   $Port (connection detail, not device identity)"
Write-Host "Press Ctrl+C to stop."
Write-Host ""

& $PythonPath $readerPath `
    --port $Port `
    --seconds 0 `
    --live `
    --baud $telemetryProfile.telemetry.serial_baud `
    --source $source.permanent_name `
    --peer $peer.permanent_name
