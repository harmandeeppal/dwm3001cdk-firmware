$serial = [System.IO.Ports.SerialPort]::new('COM16', 115200, 'None', 8, 'One')
$serial.DtrEnable = $true
$serial.RtsEnable = $true
$serial.ReadTimeout = 250
$serial.Open()
$buf = New-Object byte[] 4096
$list = New-Object System.Collections.Generic.List[byte]
$start = Get-Date
$types = @{}

Write-Host "Counting packet types on COM16 for 3 seconds..." -ForegroundColor Cyan

while (((Get-Date) - $start).TotalSeconds -lt 3) {
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
        $src = $payload[2]
        $k = "Type=0x{0:X2} Src={1}" -f $type, $src
        if ($types.ContainsKey($k)) {
            $types[$k]++
        } else {
            $types[$k] = 1
        }
        $list.RemoveRange(0, 6 + $len)
    }
}

$serial.Close()
$serial.Dispose()

Write-Host "Packet distribution:" -ForegroundColor Cyan
$types.GetEnumerator() | Sort-Object Name | ForEach-Object {
    Write-Host ("  {0} -> {1} packets" -f $_.Key, $_.Value)
}

