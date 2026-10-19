$serial = [System.IO.Ports.SerialPort]::new('COM16', 115200, 'None', 8, 'One')
$serial.DtrEnable = $true
$serial.RtsEnable = $true
$serial.ReadTimeout = 250
$serial.Open()
$buf = New-Object byte[] 4096
$list = New-Object System.Collections.Generic.List[byte]
$start = Get-Date

Write-Host "Capturing diagnostics from LEFT WRIST (Src=2) for 4 seconds..." -ForegroundColor Cyan

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

        if ($type -eq 5 -and $payload[2] -eq 2) {
            $src = $payload[2]
            $pair = $payload[3]
            $stg = $payload[4]
            $blk = [int]$payload[5] + 256 * [int]$payload[6]
            $ts = [BitConverter]::ToUInt32($payload, 8)
            $detail = [BitConverter]::ToInt32($payload, 12)
            Write-Host ("[LEFT WRIST DIAG] Stage=0x{0:X2} Blk={1} Detail={2} (0x{2:X8}) Ts={3}us" -f $stg, $blk, $detail, $ts) -ForegroundColor Magenta
        }
        $list.RemoveRange(0, 6 + $len)
    }
}

$serial.Close()
$serial.Dispose()
