$path = "hardware/outputs/records/captures/2026-10-07_three_node_star_bringup/six_node_star_test_2026-10-09_124657.json"
$data = Get-Content $path -Raw | ConvertFrom-Json

Write-Output "=== TRANSITION AT GAP 1 (Around 14s) ==="
$data.star_range_blocks | Where-Object { $_.elapsed_s -ge 13.0 -and $_.elapsed_s -le 15.0 } | ForEach-Object {
    $b = $_
    $p0 = $b.pairs[0]
    $p1 = $b.pairs[1]
    Write-Output ("Elapsed: {0,6:F3}s | Block: {1,5} | P0: st={2,2} r={3,3}cm | P1: st={4,2} r={5,3}cm" -f $b.elapsed_s, $b.block_index, $p0.status, $p0.range_cm, $p1.status, $p1.range_cm)
}

Write-Output "`n=== TRANSITION AT GAP 2 (Around 65s) ==="
$data.star_range_blocks | Where-Object { $_.elapsed_s -ge 64.5 -and $_.elapsed_s -le 66.5 } | ForEach-Object {
    $b = $_
    $p0 = $b.pairs[0]
    $p1 = $b.pairs[1]
    Write-Output ("Elapsed: {0,6:F3}s | Block: {1,5} | P0: st={2,2} r={3,3}cm | P1: st={4,2} r={5,3}cm" -f $b.elapsed_s, $b.block_index, $p0.status, $p0.range_cm, $p1.status, $p1.range_cm)
}

