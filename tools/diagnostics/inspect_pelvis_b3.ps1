$path = "hardware/outputs/records/captures/2026-10-07_three_node_star_bringup/six_node_star_test_2026-10-09_111327.json"
$data = Get-Content $path -Raw | ConvertFrom-Json

Write-Output "=== PELVIS 0xB3 DIAGNOSTICS ONLY ==="
$pelvisB3 = $data.diagnostics | Where-Object { $_.source_node -eq 1 -and $_.stage_code -eq '0xB3' }
Write-Output "Count: $($pelvisB3.Count)"

$firstFew = $pelvisB3 | Select-Object -First 5
$lastFew = $pelvisB3 | Select-Object -Last 5

Write-Output "First 5 Pelvis 0xB3:"
$firstFew | ForEach-Object {
    [PSCustomObject]@{
        elapsed_s = $_.elapsed_s
        sub_code = $_.sub_code
        detail_dec = $_.detail
        detail_hex = ("0x{0:X8}" -f [int64]$_.detail)
        ts_us = $_.timestamp_us
    }
} | Format-Table -AutoSize

Write-Output "Last 5 Pelvis 0xB3:"
$lastFew | ForEach-Object {
    [PSCustomObject]@{
        elapsed_s = $_.elapsed_s
        sub_code = $_.sub_code
        detail_dec = $_.detail
        detail_hex = ("0x{0:X8}" -f [int64]$_.detail)
        ts_us = $_.timestamp_us
    }
} | Format-Table -AutoSize

Write-Output "Distinct sub_codes for Pelvis 0xB3:"
$pelvisB3 | Group-Object sub_code | Select-Object Name, Count | Format-Table -AutoSize
