$path = "hardware/outputs/records/captures/2026-10-07_three_node_star_bringup/six_node_star_test_2026-10-09_111327.json"
$data = Get-Content $path -Raw | ConvertFrom-Json

Write-Output "=== PELVIS STATUS 0xEE (SESSION STATUS REASON) ==="
$pelvisEE = $data.diagnostics | Where-Object { $_.source_node -eq 1 -and $_.stage_code -eq '0xEE' }
Write-Output "Count: $($pelvisEE.Count)"
$pelvisEE | ForEach-Object {
    [PSCustomObject]@{
        elapsed_s = $_.elapsed_s
        sub_code = $_.sub_code
        detail = $_.detail
        ts_us = $_.timestamp_us
    }
} | Format-Table -AutoSize

Write-Output "=== PELVIS ALL DIAGNOSTIC STAGE CODES EMITTED ==="
$data.diagnostics | Where-Object { $_.source_node -eq 1 } | Group-Object stage_code | Select-Object Name, Count | Format-Table -AutoSize
