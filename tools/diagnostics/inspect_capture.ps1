$path = "hardware/outputs/records/captures/2026-10-07_three_node_star_bringup/six_node_star_test_2026-10-09_111327.json"
$data = Get-Content $path -Raw | ConvertFrom-Json

Write-Output "=== CAPTURE SUMMARY ==="
Write-Output "Duration requested: $($data.duration_s) s"
Write-Output "Active duration: $($data.active_duration_s) s"
Write-Output "Star blocks count: $($data.star_range_blocks.Count)"

Write-Output "`n=== UWB SUMMARY ==="
$data.uwb_summary.PSObject.Properties | ForEach-Object {
    $pair = $_.Value
    Write-Output "Pair: $($pair.pair_name)"
    Write-Output "  Valid: $($pair.valid_count) / $($pair.total_attempts) (Yield: $($pair.true_yield_pct)%, Wire: $($pair.wire_success_pct)%)"
    Write-Output "  Rate: $($pair.rate_hz) Hz, Median dist: $($pair.median_cm) cm"
    Write-Output "  First range: $($pair.first_range_s) s"
    Write-Output "  Status breakdown: $($pair.status_breakdown | ConvertTo-Json -Compress)"
}

Write-Output "`n=== DIAGNOSTICS ==="
if ($data.diagnostics) {
    $data.diagnostics | ConvertTo-Json -Depth 5
} else {
    Write-Output "No diagnostics logged in capture."
}

Write-Output "`n=== BLOCK INDEX AND TIME SPAN ==="
if ($data.star_range_blocks.Count -gt 0) {
    $first = $data.star_range_blocks[0]
    $last = $data.star_range_blocks[-1]
    Write-Output "First block: elapsed_s=$($first.elapsed_s) block_index=$($first.block_index)"
    Write-Output "Last block:  elapsed_s=$($last.elapsed_s) block_index=$($last.block_index)"
}
