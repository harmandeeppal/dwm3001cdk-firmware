param(
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$buildDir = "C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_UNIFIED_star"
$srcDir = 'C:\Users\harma\OneDrive - AUT University\AUT - COMP971 Dissertation (60 points)\hardware\firmware\qorvo_qm33_motion_node\official_sdk_project\DWM3001CDK'

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "Building Unified COMP971 Motion Node (FICR Auto-Discovery)" -ForegroundColor Cyan
Write-Host "Target: Single firmware image for all 6 boards" -ForegroundColor Cyan
Write-Host "Build Directory: $buildDir" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Path $buildDir -Recurse -Force
}

if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
}

$cmakeExe = 'C:\Program Files\CMake\bin\cmake.exe'
$makeExe = 'C:\msys64\ucrt64\bin\mingw32-make.exe'
$toolchain = 'C:/COMP971_QORVO_SDK_1_1_1/Projects/Common/cmakefiles/arm-none-eabi-gcc.cmake'
$commonPath = 'C:/COMP971_QORVO_SDK_1_1_1/Projects/Common/cmakefiles'
$projectBase = 'C:/COMP971_QORVO_SDK_1_1_1'
$projectCommon = 'C:\Users\harma\OneDrive - AUT University\AUT - COMP971 Dissertation (60 points)\hardware\firmware\qorvo_qm33_motion_node\official_sdk_project\Common'
$targetScript = 'C:\Users\harma\OneDrive - AUT University\AUT - COMP971 Dissertation (60 points)\hardware\firmware\qorvo_qm33_motion_node\official_sdk_project\DWM3001CDK\project_COMP971.cmake'

$cmakeArgs = @(
    '-G', 'MinGW Makefiles',
    '-B', $buildDir,
    '-S', $srcDir,
    "-DCMAKE_MAKE_PROGRAM=$makeExe",
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
    "-DCOMMON_PATH=$commonPath",
    "-DPROJECT_BASE=$projectBase",
    "-DPROJECT_COMMON=$projectCommon",
    "-DMY_TARGET_SCRIPT=$targetScript",
    "-DLIBS_PATH=Libs",
    "-Duwbstack_bundle_DIR=C:/COMP971_QORVO_SDK_1_1_1/Libs/uwbstack_libs/delivery/fira/Release/lib/arm-cortex-m4-hard_floating/uwbstack_bundle/cmake",
    "-DCOMP971_NODE_INDEX=0",
    "-DCOMP971_DEVICE_NAME=COMP971-NODE",
    "-DCOMP971_UWB_ROLE=1",
    "-DCOMP971_NUM_CONTROLEES=5"
)

Write-Host "Configuring CMake..." -ForegroundColor Green
& $cmakeExe @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed with exit code $LASTEXITCODE"
}

Write-Host "Compiling with mingw32-make..." -ForegroundColor Green
& $makeExe -C $buildDir -j4
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE"
}

$hexPath = Join-Path $buildDir 'DWM3001CDK-COMP971-FreeRTOS.hex'
if (-not (Test-Path $hexPath)) {
    throw "Build completed but output HEX not found: $hexPath"
}

$hexItem = Get-Item $hexPath
$hash = (Get-FileHash -Path $hexPath -Algorithm SHA256).Hash

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "UNIFIED BUILD SUCCESSFUL!" -ForegroundColor Green
Write-Host "HEX File: $hexPath" -ForegroundColor Green
Write-Host "Size: $($hexItem.Length) bytes" -ForegroundColor Green
Write-Host "SHA256: $hash" -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Cyan

