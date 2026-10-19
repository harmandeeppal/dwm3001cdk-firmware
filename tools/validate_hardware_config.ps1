$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$projectVenv = Join-Path $workspaceRoot ".venv\Scripts\python.exe"

$pythonCandidates = @(
    $projectVenv,
    (Join-Path $env:LOCALAPPDATA "DWMTools\platformio-venv2\Scripts\python.exe"),
    (Join-Path $env:LOCALAPPDATA "DWMTools\platformio-venv\Scripts\python.exe")
)

$pythonPath = $null
foreach ($cand in $pythonCandidates) {
    try {
        if ([System.IO.File]::Exists($cand)) {
            $pythonPath = $cand
            break
        }
    } catch {
        # ignore access restrictions on outside paths
    }
}

if (-not $pythonPath) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
    if ($pythonCommand) {
        $pythonPath = $pythonCommand.Source
    }
}

if (-not $pythonPath) {
    throw "No suitable Python installation was found."
}

& $pythonPath (Join-Path $PSScriptRoot "validation\validate_config.py")
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
