$serial = [System.IO.Ports.SerialPort]::new('COM16', 115200, 'None', 8, 'One')
$serial.DtrEnable = $true
$serial.RtsEnable = $true
$serial.ReadTimeout = 250
$serial.Open()
$buf = New-Object byte[] 4096
$list = New-Object System.Collections.Generic.List[byte]
$start = Get-Date
$seen = @{}

Write-Host "Reading Node Status from all nodes for 3 seconds..." -ForegroundColor Cyan

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

        if ($type -eq 4 -and $len -ge 52) {
            $nodeIdx = $payload[2]
            if (-not $seen.ContainsKey($nodeIdx)) {
                $seen[$nodeIdx] = $true
                $syncEpoch = [BitConverter]::ToUInt32($payload, 4)
                $imuAcq = [BitConverter]::ToUInt32($payload, 8)
                $imuQOk = [BitConverter]::ToUInt32($payload, 12)
                $imuQDrop = [BitConverter]::ToUInt32($payload, 16)
                $imuQHi = [BitConverter]::ToUInt32($payload, 20)
                $bleImuOk = [BitConverter]::ToUInt32($payload, 24)
                $bleImuFail = [BitConverter]::ToUInt32($payload, 28)
                $bleUwbFail = [BitConverter]::ToUInt32($payload, 32)
                $uwbAtt = [BitConverter]::ToUInt32($payload, 36)
                $uwbVal = [BitConverter]::ToUInt32($payload, 40)
                $uwbInval = [BitConverter]::ToUInt32($payload, 44)
                $uwbTo = [BitConverter]::ToUInt32($payload, 48)

                Write-Host ("--- NODE {0} STATUS ---" -f $nodeIdx) -ForegroundColor Yellow
                Write-Host ("  IMU acquired: {0} | Queue dropped: {1} | BLE notify ok: {2} | BLE notify fail: {3}" -f $imuAcq, $imuQDrop, $bleImuOk, $bleImuFail)
                Write-Host ("  UWB attempts: {0} | Valid: {1} | Invalid: {2} | Result timeout: {3}" -f $uwbAtt, $uwbVal, $uwbInval, $uwbTo)
            }
        }
        $list.RemoveRange(0, 6 + $len)
    }
}

$serial.Close()
$serial.Dispose()

