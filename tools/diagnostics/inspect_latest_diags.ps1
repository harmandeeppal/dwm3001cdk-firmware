$path = "hardware/outputs/records/captures/2026-10-07_three_node_star_bringup/six_node_star_test_2026-10-09_124455.json"
$data = Get-Content $path -Raw | ConvertFrom-Json

Write-Output "=== PELVIS DIAGNOSTICS IN 124455 CAPTURE ==="
$pelvisDiags = $data.diagnostics | Where-Object { $_.source_node -eq 1 }
Write-Output "Total Pelvis diagnostics count: $($pelvisDiags.Count)"
$pelvisDiags | ForEach-Object {
    [PSCustomObject]@{
        elapsed_s = $_.elapsed_s
        stage_code = $_.stage_code
        stage_name = $_.stage_name
        sub_code = $_.sub_code
        detail_dec = $_.detail
        detail_hex = ("0x{0:X8}" -f [int64]$_.detail)
        ts_us = $_.timestamp_us
    }
} | Format-Table -AutoSize

