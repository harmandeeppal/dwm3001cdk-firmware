$path = "hardware/outputs/records/captures/2026-10-07_three_node_star_bringup/six_node_star_test_2026-10-09_111327.json"
$data = Get-Content $path -Raw | ConvertFrom-Json

Write-Output "=== PELVIS DIAGNOSTICS AROUND 90-100s WITH BLOCK SNAPSHOT ==="
$pelvisDiags = $data.diagnostics | Where-Object { $_.source_node -eq 1 -and $_.elapsed_s -ge 88 -and $_.elapsed_s -le 106 }
$pelvisDiags | ForEach-Object {
    [PSCustomObject]@{
        elapsed_s = $_.elapsed_s
        stage_code = $_.stage_code
        stage_name = $_.stage_name
        sub_code = $_.sub_code
        snap_block = $_.sub_code  # Note: in test script, sub_code was payload[5], which is byte 1 of snapshot_block!
        detail_dec = $_.detail
        detail_hex = ("0x{0:X8}" -f [int64]$_.detail)
        ts_us = $_.timestamp_us
    }
} | Format-Table -AutoSize
