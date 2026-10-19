param(
    [string]$PelvisPort = 'COM17',
    [string]$LeftWristPort = 'COM18',
    [int]$DurationSeconds = 60
)

$ErrorActionPreference = 'Stop'
$tool = 'C:\QorvoPython310\Scripts\run_fira_twr.exe'
$sessionDir = Join-Path $PSScriptRoot ("official_default_100cm_{0}" -f (Get-Date -Format 'yyyyMMdd_HHmmss'))
New-Item -ItemType Directory -Path $sessionDir | Out-Null

# Start the responder first so it is ready before PELVIS creates the session.
$controlee = Start-Process -FilePath $tool `
    -ArgumentList "-p $LeftWristPort -t $($DurationSeconds + 10) --controlee" `
    -WorkingDirectory $sessionDir `
    -RedirectStandardOutput (Join-Path $sessionDir 'left_wrist_controlee.txt') `
    -RedirectStandardError (Join-Path $sessionDir 'left_wrist_controlee_error.txt') `
    -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 2
$controller = Start-Process -FilePath $tool `
    -ArgumentList "-p $PelvisPort -t $DurationSeconds --stats --diag_dump" `
    -WorkingDirectory $sessionDir `
    -RedirectStandardOutput (Join-Path $sessionDir 'pelvis_controller.txt') `
    -RedirectStandardError (Join-Path $sessionDir 'pelvis_controller_error.txt') `
    -PassThru -WindowStyle Hidden

[pscustomobject]@{
    SessionDirectory = $sessionDir
    PelvisProcessId = $controller.Id
    LeftWristProcessId = $controlee.Id
    ExpectedFinish = (Get-Date).AddSeconds($DurationSeconds + 12).ToString('s')
} | Format-List
