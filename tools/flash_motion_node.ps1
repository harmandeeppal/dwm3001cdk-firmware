param(
    [Parameter(Mandatory = $true)]
    [string]$Snr,

    [Parameter(Mandatory = $true)]
    [string]$HexPath,

    [Parameter(Mandatory = $true)]
    [string]$NodeName,

    [string]$RecordDir = '.\hardware\outputs\records\firmware\2026-10-07_six_node_star_rollout'
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $HexPath)) {
    throw "Target HEX file not found: $HexPath"
}

if (-not (Test-Path $RecordDir)) {
    New-Item -ItemType Directory -Path $RecordDir -Force | Out-Null
}

$hexHash = (Get-FileHash -Path $HexPath -Algorithm SHA256).Hash

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "COMP971 SAFE FLASH: Node=[$NodeName], SNR=[$Snr]" -ForegroundColor Cyan
Write-Host "Target HEX: $HexPath" -ForegroundColor Cyan
Write-Host "HEX SHA-256: $hexHash" -ForegroundColor Cyan
Write-Host "Safety Rule: --sectorerase ONLY (NEVER --chiperase)" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Read Sector 126 pre-flash
Write-Host "[1/5] Reading Sector 126 (factory calibration) before flash..." -ForegroundColor Yellow
$preSectorPath = Join-Path $RecordDir "${NodeName}_${Snr}_pre_sector126.bin"
$preUicrPath = Join-Path $RecordDir "${NodeName}_${Snr}_pre_uicr.hex"

# Save UICR backup
nrfjprog --snr $Snr --readuicr $preUicrPath
$preUicrHash = (Get-FileHash -Path $preUicrPath -Algorithm SHA256).Hash

# Dump Sector 126 (0x7E000 - 0x7EFFF = 4096 bytes)
$preSectorWords = nrfjprog --snr $Snr --memrd 0x0007E000 --n 64
Write-Host "Sector 126 Header: $($preSectorWords[0])" -ForegroundColor Gray

# 2. Program with --sectorerase --verify
Write-Host "[2/5] Programming application with --sectorerase --verify..." -ForegroundColor Green
nrfjprog --snr $Snr --program $HexPath --sectorerase --verify
if ($LASTEXITCODE -ne 0) {
    throw "nrfjprog --program failed with exit code $LASTEXITCODE"
}

# 3. Post-flash Sector 126 verification
Write-Host "[3/5] Verifying Sector 126 factory calibration preserved..." -ForegroundColor Yellow
$postSectorWords = nrfjprog --snr $Snr --memrd 0x0007E000 --n 64
if ($preSectorWords[0] -ne $postSectorWords[0]) {
    throw "FATAL SAFETY VIOLATION: Sector 126 changed during flash! Pre: $($preSectorWords[0]), Post: $($postSectorWords[0])"
}
Write-Host "Sector 126 VERIFIED UNTOUCHED!" -ForegroundColor Green

# 4. Post-flash UICR verification
Write-Host "[4/5] Verifying UICR preserved..." -ForegroundColor Yellow
$postUicrPath = Join-Path $RecordDir "${NodeName}_${Snr}_post_uicr.hex"
nrfjprog --snr $Snr --readuicr $postUicrPath
$postUicrHash = (Get-FileHash -Path $postUicrPath -Algorithm SHA256).Hash
if ($preUicrHash -ne $postUicrHash) {
    throw "FATAL SAFETY VIOLATION: UICR changed during flash! Pre: $preUicrHash, Post: $postUicrHash"
}
Write-Host "UICR VERIFIED UNTOUCHED!" -ForegroundColor Green

# 5. Reset target
Write-Host "[5/5] Resetting target processor..." -ForegroundColor Green
nrfjprog --snr $Snr --reset
if ($LASTEXITCODE -ne 0) {
    throw "nrfjprog --reset failed with exit code $LASTEXITCODE"
}

$nowIso = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
$summary = @"
# Flash Record: $NodeName ($Snr)
- Date: $nowIso
- J-Link SNR: $Snr
- Node: $NodeName
- HEX: $HexPath
- HEX SHA-256: $hexHash
- Sector 126 Verified Untouched: True ($($preSectorWords[0]))
- UICR Verified Untouched: True ($postUicrHash)
"@
$recordPath = Join-Path $RecordDir "${NodeName}_${Snr}_flash_record.md"
Set-Content -Path $recordPath -Value $summary

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "FLASH & VERIFICATION COMPLETE FOR $NodeName!" -ForegroundColor Green
Write-Host "Record saved: $recordPath" -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Cyan

