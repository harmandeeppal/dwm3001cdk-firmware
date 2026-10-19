$serial = [System.IO.Ports.SerialPort]::new('COM16', 115200, 'None', 8, 'One')
$serial.DtrEnable = $true
$serial.RtsEnable = $true
$serial.ReadTimeout = 250
$serial.Open()
$buf = New-Object byte[] 4096
$list = New-Object System.Collections.Generic.List[byte]
$start = Get-Date

Write-Host "Capturing Pair 1 raw bytes from COM16 for 4 seconds..." -ForegroundColor Cyan

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

        # Check if Pair 1
        if (($type -eq 3 -or $type -eq 5) -and $payload[3] -eq 1) {
            $hexStr = [BitConverter]::ToString($payload)
            Write-Host ("[TYPE {0} PAIR 1 RAW] {1}" -f $type, $hexStr) -ForegroundColor Yellow
        }
        $list.RemoveRange(0, 6 + $len)
    }
}

$serial.Close()
$serial.Dispose()
