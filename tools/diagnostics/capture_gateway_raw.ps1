param(
    [string]$Port = 'COM16',
    [ValidateRange(5, 7200)]
    [int]$DurationSeconds = 60,
    [ValidateRange(0, 30)]
    [int]$SettlingSeconds = 3,
    [string]$OutputDirectory = '.\hardware\outputs\records\acquisition'
)

$ErrorActionPreference = 'Stop'
$captureId = Get-Date -Format 'yyyy-MM-dd_HHmmss'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$dataPath = Join-Path $OutputDirectory ("gateway_raw_{0}.bin" -f $captureId)
$metadataPath = Join-Path $OutputDirectory ("gateway_raw_{0}.json" -f $captureId)
$serial = [System.IO.Ports.SerialPort]::new($Port, 115200, 'None', 8, 'One')
$serial.DtrEnable = $true
$serial.RtsEnable = $true
$serial.ReadTimeout = 250
$stream = [System.IO.File]::Open($dataPath, [System.IO.FileMode]::CreateNew)
$buffer = New-Object byte[] 4096
$start = $null

try {
    $serial.Open()
    if ($SettlingSeconds -gt 0) {
        Write-Host "Clearing records buffered before capture for $SettlingSeconds seconds..." -ForegroundColor DarkGray
        $settlingUntil = (Get-Date).AddSeconds($SettlingSeconds)
        while ((Get-Date) -lt $settlingUntil) {
            try {
                $count = $serial.Read($buffer, 0, $buffer.Length)
            } catch [System.TimeoutException] {
                # No bytes for this short interval is normal.
            }
        }
    }
    $start = Get-Date
    Write-Host "Recording raw binary gateway data from $Port for $DurationSeconds seconds. Do not run another viewer at the same time." -ForegroundColor Cyan
    while (((Get-Date) - $start).TotalSeconds -lt $DurationSeconds) {
        try {
            $count = $serial.Read($buffer, 0, $buffer.Length)
            if ($count -gt 0) { $stream.Write($buffer, 0, $count) }
        } catch [System.TimeoutException] {
            # A short timeout is normal when no packet arrives.
        }
    }
} finally {
    $stream.Dispose()
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}

$metadata = [ordered]@{
    capture_started_utc = $start.ToUniversalTime().ToString('o')
    duration_seconds = $DurationSeconds
    pre_capture_discard_seconds = $SettlingSeconds
    port = $Port
    format = 'COMP971 gateway USB framed binary protocol v1'
    raw_file = [System.IO.Path]::GetFileName($dataPath)
    raw_bytes = (Get-Item -LiteralPath $dataPath).Length
    decode_command = "python .\hardware\tools\decode_gateway_capture.py `"$dataPath`""
}
$metadata | ConvertTo-Json | Set-Content -LiteralPath $metadataPath -Encoding UTF8
Write-Host "Saved raw capture: $dataPath" -ForegroundColor Green
Write-Host "Next: python .\hardware\tools\decode_gateway_capture.py `"$dataPath`"" -ForegroundColor Yellow
