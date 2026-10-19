$path = "hardware/outputs/records/captures/2026-10-07_three_node_star_bringup/six_node_star_test_2026-10-09_111327.json"
$data = Get-Content $path -Raw | ConvertFrom-Json

Write-Output "=== PELVIS DIAGNOSTICS AROUND 90-100s ==="
$pelvisDiags = $data.diagnostics | Where-Object { $_.source_node -eq 1 -and $_.elapsed_s -ge 80 -and $_.elapsed_s -le 120 }
$pelvisDiags | ForEach-Object {
    [PSCustomObject]@{
        elapsed_s = $_.elapsed_s
        stage_code = $_.stage_code
        stage_name = $_.stage_name
        sub_code = $_.sub_code
        detail_dec = $_.detail
        detail_hex = ("0x{0:X8}" -f [int64]$_.detail)
    }
} | Format-Table -AutoSize

Write-Output "`n=== STAGE CODES EMITTED BY PELVIS OVER FULL RUN ==="
$data.diagnostics | Where-Object { $_.source_node -eq 1 } | Group-Object stage_code | Select-Object Name, Count | Format-Table -AutoSize
