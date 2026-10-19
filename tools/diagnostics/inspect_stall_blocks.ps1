$path = "hardware/outputs/records/captures/2026-10-07_three_node_star_bringup/six_node_star_test_2026-10-09_111327.json"
$data = Get-Content $path -Raw | ConvertFrom-Json

Write-Output "=== BLOCKS AROUND 90-93 SECONDS ==="
$blocksAroundStall = $data.star_range_blocks | Where-Object { $_.elapsed_s -ge 88.0 }
$blocksAroundStall | ForEach-Object {
    $b = $_
    $p0 = $b.pairs[0]
    $p1 = $b.pairs[1]
    Write-Output ("Elapsed: {0,6:F3}s | Block: {1,5} | P0(HEAD): st={2,2} r={3,3}cm | P1(L-WRIST): st={4,2} r={5,3}cm" -f $b.elapsed_s, $b.block_index, $p0.status, $p0.range_cm, $p1.status, $p1.range_cm)
}
