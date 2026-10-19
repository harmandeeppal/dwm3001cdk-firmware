$mem = @{}
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
                $mem[$fullAddr + $i] = [Convert]::ToByte($dataStr.Substring($i * 2, 2), 16)
            }
        } elseif ($type -eq 4) {
            $base = [Convert]::ToInt32($dataStr, 16) -shl 16
        }
    }
}
$sha = (0..31 | ForEach-Object { '{0:X2}' -f $mem[0x7E000 + $_] }) -join ' '
$hdr = (0..15 | ForEach-Object { '{0:X2}' -f $mem[0x7F000 + $_] }) -join ' '
Write-Host "Flash 0x7E000 SHA: $sha"
Write-Host "Flash 0x7F000 Header: $hdr"

$size = [BitConverter]::ToUInt16(@($mem[0x7F000 + 4], $mem[0x7F000 + 5]), 0)
$version = [BitConverter]::ToUInt16(@($mem[0x7F000 + 0], $mem[0x7F000 + 1]), 0)
Write-Host "Version: $version, Size: $size"
