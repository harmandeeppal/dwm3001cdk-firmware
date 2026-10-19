param(
    [string]$Port = 'COM16',
    [int]$DurationSeconds = 5
)

$sp = [System.IO.Ports.SerialPort]::new($Port, 115200, 'None', 8, 'One')
$sp.DtrEnable = $true
$sp.RtsEnable = $true
$sp.ReadTimeout = 250

$nodeCounts = @{}
$statusReports = @{}
$rangeCounts = @{}

try {
    $sp.Open()
    Write-Host "Probing $Port for $DurationSeconds seconds..."
    $start = Get-Date
    $buf = New-Object System.Collections.Generic.List[byte]
    $readBuf = New-Object byte[] 4096

    while (((Get-Date) - $start).TotalSeconds -lt $DurationSeconds) {
        try {
            $n = $sp.Read($readBuf, 0, $readBuf.Length)
            if ($n -gt 0) {
                for ($k = 0; $k -lt $n; $k++) { $buf.Add($readBuf[$k]) }
            }
        } catch [System.TimeoutException] { }

        while ($buf.Count -ge 6) {
            if ($buf[0] -ne 0xC9 -or $buf[1] -ne 0x71) {
                $buf.RemoveAt(0)
                continue
            }
            $len = [int]$buf[4] + (256 * [int]$buf[5])
            if ($buf.Count -lt 6 + $len) { break }
            $type = $buf[3]
            $payload = $buf.GetRange(6, $len).ToArray()

            if ($type -eq 1 -and $len -ge 38) {
                $nodeIdx = [int]$payload[2]
                if (-not $nodeCounts.ContainsKey($nodeIdx)) { $nodeCounts[$nodeIdx] = 0 }
                $nodeCounts[$nodeIdx]++
            }
            elseif ($type -eq 3 -and $len -ge 24) {
                $pIdx = [int]$payload[3]
                $flags = [BitConverter]::ToUInt16($payload, 12)
                $valid = ($flags -band 1) -ne 0
                $key = "Pair_$pIdx"
                if (-not $rangeCounts.ContainsKey($key)) { $rangeCounts[$key] = @{ valid = 0; total = 0 } }
                $rangeCounts[$key].total++
                if ($valid) { $rangeCounts[$key].valid++ }
            }
            elseif ($type -eq 4 -and $len -ge 52) {
                $nodeIdx = [int]$payload[2]
                $statusReports[$nodeIdx] = [ordered]@{
                    node = $nodeIdx
                    flags = [int]$payload[3]
                    acquired = [BitConverter]::ToUInt32($payload, 8)
                    queue_drop = [BitConverter]::ToUInt32($payload, 16)
                    ble_fail = [BitConverter]::ToUInt32($payload, 28)
                    uwb_attempts = [BitConverter]::ToUInt32($payload, 36)
                    uwb_valid = [BitConverter]::ToUInt32($payload, 40)
                    uwb_invalid = [BitConverter]::ToUInt32($payload, 44)
                }
            }
            $buf.RemoveRange(0, 6 + $len)
        }
    }
} finally {
    if ($sp.IsOpen) { $sp.Close() }
    $sp.Dispose()
}

Write-Host "`n--- IMU Counts ---"
$nodeCounts.GetEnumerator() | Sort-Object Key | ForEach-Object {
    Write-Host ("Node {0}: {1} samples" -f $_.Key, $_.Value)
}

Write-Host "`n--- Range Counts ---"
$rangeCounts.GetEnumerator() | Sort-Object Key | ForEach-Object {
    Write-Host ("{0}: valid={1}, total={2}" -f $_.Key, $_.Value.valid, $_.Value.total)
}

Write-Host "`n--- Node Status Reports ---"
$statusReports.GetEnumerator() | Sort-Object Key | ForEach-Object {
    Write-Host ("Node {0}: acquired={1}, queue_drop={2}, ble_fail={3}, uwb_att={4}, uwb_val={5}" -f $_.Key, $_.Value.acquired, $_.Value.queue_drop, $_.Value.ble_fail, $_.Value.uwb_attempts, $_.Value.uwb_valid)
}
