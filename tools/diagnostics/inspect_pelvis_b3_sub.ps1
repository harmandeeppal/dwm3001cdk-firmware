$path = "hardware/outputs/records/captures/2026-10-07_three_node_star_bringup/six_node_star_test_2026-10-09_111327.json"
$data = Get-Content $path -Raw | ConvertFrom-Json

Write-Output "=== PELVIS 0xB3 WITH SUB_CODE != 15 ==="
$specialB3 = $data.diagnostics | Where-Object { $_.source_node -eq 1 -and $_.stage_code -eq '0xB3' -and $_.sub_code -ne 15 }
$specialB3 | ForEach-Object {
    [PSCustomObject]@{
        elapsed_s = $_.elapsed_s
        sub_code = $_.sub_code
        detail_dec = $_.detail
        detail_hex = ("0x{0:X8}" -f [int64]$_.detail)
        ts_us = $_.timestamp_us
    }
} | Format-Table -AutoSize
