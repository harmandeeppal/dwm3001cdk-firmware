param(
    [string]$Port = 'COM15',
    [int]$DurationSeconds = 20
)

function Open-GatewayPort {
    $candidate = New-Object System.IO.Ports.SerialPort $Port, 115200, 'None', 8, 'One'
    $candidate.ReadTimeout = 200
    $candidate.DtrEnable = $false
    $candidate.RtsEnable = $false
    $candidate.Open()
    return $candidate
}

$serial = $null
Write-Host "COMP971 gateway Bluetooth diagnostic: $Port for $DurationSeconds seconds"
Write-Host 'Press the dongle reset button once. The script will wait while COM16 restarts.'
$until = [DateTime]::UtcNow.AddSeconds($DurationSeconds)

try {
    while ([DateTime]::UtcNow -lt $until) {
        if ($null -eq $serial -or -not $serial.IsOpen) {
            try {
                if ($null -ne $serial) { $serial.Dispose() }
                $serial = Open-GatewayPort
                Write-Host "Connected to $Port."
            }
            catch {
                Start-Sleep -Milliseconds 250
                continue
            }
        }

        try {
            $text = $serial.ReadExisting()
            if ($text.Length -gt 0) { Write-Host -NoNewline $text }
        }
        catch {
            if ($serial.IsOpen) { $serial.Close() }
        }
        Start-Sleep -Milliseconds 100
    }
}
finally {
    if ($null -ne $serial) {
        if ($serial.IsOpen) { $serial.Close() }
        $serial.Dispose()
    }
}
