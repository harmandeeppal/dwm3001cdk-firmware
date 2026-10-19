$path = "hardware/outputs/records/captures/2026-10-07_three_node_star_bringup/six_node_star_test_2026-10-09_124657.json"
$data = Get-Content $path -Raw | ConvertFrom-Json

Write-Output "=== CAPTURE BASIC STATS ==="
Write-Output "Total star range blocks: $($data.star_range_blocks.Count)"
$firstB = $data.star_range_blocks[0]
$lastB = $data.star_range_blocks[-1]
Write-Output "First block: elapsed=$($firstB.elapsed_s)s, block_index=$($firstB.block_index)"
Write-Output "Last block:  elapsed=$($lastB.elapsed_s)s, block_index=$($lastB.block_index)"

Write-Output "`n=== PELVIS DIAGNOSTICS ==="
$pelvisDiags = $data.diagnostics | Where-Object { $_.source_node -eq 1 }
Write-Output "Pelvis diagnostics count: $($pelvisDiags.Count)"
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

Write-Output "`n=== CHECKING FOR GAPS IN BLOCKS ==="
$blocks = $data.star_range_blocks
$gaps = @()
for ($i = 1; $i -lt $blocks.Count; $i++) {
    $dt = $blocks[$i].elapsed_s - $blocks[$i-1].elapsed_s
    $db = $blocks[$i].block_index - $blocks[$i-1].block_index
    if ($dt -gt 1.0 -or $db -gt 5) {
        $gaps += [PSCustomObject]@{
            prev_elapsed = $blocks[$i-1].elapsed_s
            curr_elapsed = $blocks[$i].elapsed_s
            delta_t_s = [math]::Round($dt, 3)
            prev_block = $blocks[$i-1].block_index
            curr_block = $blocks[$i].block_index
            delta_blocks = $db
        }
    }
}
if ($gaps.Count -gt 0) {
    Write-Output "Detected Gaps:"
    $gaps | Format-Table -AutoSize
} else {
    Write-Output "No gaps > 1.0s detected in star range blocks."
}

