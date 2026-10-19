$serial = [System.IO.Ports.SerialPort]::new('COM16', 115200, 'None', 8, 'One')
$serial.DtrEnable = $true
$serial.RtsEnable = $true
$serial.ReadTimeout = 250
$serial.Open()
$buf = New-Object byte[] 4096
$list = New-Object System.Collections.Generic.List[byte]
$start = Get-Date

Write-Host "Probing for Stage 0x42 / Pair 1 status for 4 seconds..." -ForegroundColor Cyan

while (((Get-Date) - $start).TotalSeconds -lt 4) {
    try {
        $n = $serial.Read($buf, 0, $buf.Length)
        if ($n -gt 0) {
            for ($i = 0; $i -lt $n; $i++) { $list.Add($buf[$i]) }
        }
    } catch [System.TimeoutException] { }

    while ($list.Count -ge 6) {
        if ($list[0] -ne 0xC9 -or $list[1] -ne 0x71) {
            $list.RemoveAt(0)
            continue
        }
        $len = [int]$list[4] + 256 * [int]$list[5]
        if ($list.Count -lt 6 + $len) { break }
        $type = $list[3]
        $payload = $list.GetRange(6, $len).ToArray()

        if ($type -eq 3) {
            $src = $payload[2]
            $pair = $payload[3]
            $dist = [BitConverter]::ToInt32($payload, 8)
            $flags = [BitConverter]::ToUInt16($payload, 12)
            Write-Host ("[RANGE] Src={0} Pair={1} Dist={2}cm Flags=0x{3:X4}" -f $src, $pair, $dist, $flags)
        }
        elseif ($type -eq 5 -and $payload[4] -eq 0x42) {
            $src = $payload[2]
            $pair = $payload[3]
            $detail = [BitConverter]::ToInt32($payload, 12)
            Write-Host ("[DIAG 0x42] Src={0} Pair={1} Status={2} (0x{2:X2})" -f $src, $pair, $detail) -ForegroundColor Red
        }
        $list.RemoveRange(0, 6 + $len)
    }
}

$serial.Close()
$serial.Dispose()

