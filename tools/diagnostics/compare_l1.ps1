$flashMem = @{}
$base = 0
Get-Content 'hardware/outputs/records/head_full_flash_backup.hex' | ForEach-Object {
    $line = $_.Trim()
    if ($line.StartsWith(':')) {
        $byteCount = [Convert]::ToInt32($line.Substring(1, 2), 16)
        $addr = [Convert]::ToInt32($line.Substring(3, 4), 16)
        $type = [Convert]::ToInt32($line.Substring(7, 2), 16)
        $dataStr = $line.Substring(9, $byteCount * 2)
        if ($type -eq 0) {
            $fullAddr = $base + $addr
            for ($i = 0; $i -lt $byteCount; $i++) {
                $flashMem[$fullAddr + $i] = [Convert]::ToByte($dataStr.Substring($i * 2, 2), 16)
            }
        } elseif ($type -eq 4) {
            $base = [Convert]::ToInt32($dataStr, 16) -shl 16
        }
    }
}

$ramMem = @{}
$base = 0
Get-Content 'hardware/outputs/records/ram_head.hex' | ForEach-Object {
    $line = $_.Trim()
    if ($line.StartsWith(':')) {
        $byteCount = [Convert]::ToInt32($line.Substring(1, 2), 16)
        $addr = [Convert]::ToInt32($line.Substring(3, 4), 16)
        $type = [Convert]::ToInt32($line.Substring(7, 2), 16)
        $dataStr = $line.Substring(9, $byteCount * 2)
        if ($type -eq 0) {
            $fullAddr = $base + $addr
            for ($i = 0; $i -lt $byteCount; $i++) {
                $ramMem[$fullAddr + $i] = [Convert]::ToByte($dataStr.Substring($i * 2, 2), 16)
            }
        } elseif ($type -eq 4) {
            $base = [Convert]::ToInt32($dataStr, 16) -shl 16
        }
    }
}

$diffCount = 0
$ramL1Addr = 0x20015038
for ($i = 0; $i -lt 1004; $i++) {
    $fByte = if ($flashMem.ContainsKey(0x7F000 + $i)) { $flashMem[0x7F000 + $i] } else { 0xFF }
    $rByte = if ($ramMem.ContainsKey($ramL1Addr + $i)) { $ramMem[$ramL1Addr + $i] } else { 0x00 }
    if ($fByte -ne $rByte) {
        if ($diffCount -lt 20) {
            Write-Host ("Offset 0x{0:X3} ({0}): Flash=0x{1:X2}, RAM=0x{2:X2}" -f $i, $fByte, $rByte)
        }
        $diffCount++
    }
}
Write-Host "Total byte differences out of 1004: $diffCount"
