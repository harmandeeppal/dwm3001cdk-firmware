$hexPath = Join-Path $PSScriptRoot 'DWM-01-HEAD__official-qani-v7-positive-add-controlee__760225805.hex'
$binPath = 'C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\artifacts\official_qani_positive_add_controlee_fix\head_responder_v7_qani_postflash_0x00000000_0x00054fff.bin'

$imageBytes = @{}
$baseAddress = 0
foreach ($line in Get-Content -LiteralPath $hexPath) {
    if (-not $line.StartsWith(':')) { continue }
    $count = [Convert]::ToInt32($line.Substring(1, 2), 16)
    $address = [Convert]::ToInt32($line.Substring(3, 4), 16)
    $type = [Convert]::ToInt32($line.Substring(7, 2), 16)
    if ($type -eq 4) {
        $baseAddress = [Convert]::ToInt32($line.Substring(9, 4), 16) * 65536
        continue
    }
    if ($type -eq 2) {
        $baseAddress = [Convert]::ToInt32($line.Substring(9, 4), 16) * 16
        continue
    }
    if ($type -ne 0) { continue }
    for ($index = 0; $index -lt $count; $index++) {
        $imageBytes[$baseAddress + $address + $index] = [Convert]::ToByte($line.Substring(9 + (2 * $index), 2), 16)
    }
}

$readback = [System.IO.File]::ReadAllBytes($binPath)
$mismatches = 0
foreach ($address in $imageBytes.Keys) {
    if ($address -ge $readback.Length -or $readback[$address] -ne $imageBytes[$address]) {
        $mismatches++
    }
}

Write-Output "Image-defined bytes: $($imageBytes.Count)"
Write-Output "Mismatches: $mismatches"
if ($mismatches -ne 0) { exit 1 }
