$path = "hardware/outputs/records/captures/2026-10-07_three_node_star_bringup/six_node_star_test_2026-10-09_124657.json"
$data = Get-Content $path -Raw | ConvertFrom-Json

Write-Output "=== BLOCKS SURROUNDING GAP 1 (14.048s -> 33.129s) ==="
$data.star_range_blocks | Where-Object { ($_.elapsed_s -ge 12.0 -and $_.elapsed_s -le 15.0) -or ($_.elapsed_s -ge 32.0 -and $_.elapsed_s -le 35.0) } | ForEach-Object {
    $b = $_
    $p0 = $b.pairs[0]
    $p1 = $b.pairs[1]
    Write-Output ("Elapsed: {0,6:F3}s | Block: {1,5} | P0(HEAD): st={2,2} r={3,3}cm | P1(L-WRIST): st={4,2} r={5,3}cm" -f $b.elapsed_s, $b.block_index, $p0.status, $p0.range_cm, $p1.status, $p1.range_cm)
}

Write-Output "`n=== BLOCKS SURROUNDING GAP 2 (65.588s -> 81.098s) ==="
$data.star_range_blocks | Where-Object { ($_.elapsed_s -ge 64.0 -and $_.elapsed_s -le 67.0) -or ($_.elapsed_s -ge 80.0 -and $_.elapsed_s -le 83.0) } | ForEach-Object {
    $b = $_
    $p0 = $b.pairs[0]
    $p1 = $b.pairs[1]
    Write-Output ("Elapsed: {0,6:F3}s | Block: {1,5} | P0(HEAD): st={2,2} r={3,3}cm | P1(L-WRIST): st={4,2} r={5,3}cm" -f $b.elapsed_s, $b.block_index, $p0.status, $p0.range_cm, $p1.status, $p1.range_cm)
}

Write-Output "`n=== PELVIS DIAGNOSTICS AROUND GAPS ==="
$data.diagnostics | Where-Object { $_.source_node -eq 1 -and (($_.elapsed_s -ge 10 -and $_.elapsed_s -le 40) -or ($_.elapsed_s -ge 60 -and $_.elapsed_s -le 90)) } | ForEach-Object {
    [PSCustomObject]@{
        elapsed_s = $_.elapsed_s
        stage_code = $_.stage_code
        stage_name = $_.stage_name
        sub_code = $_.sub_code
        detail_hex = ("0x{0:X8}" -f [int64]$_.detail)
        ts_us = $_.timestamp_us
    }
} | Format-Table -AutoSize

