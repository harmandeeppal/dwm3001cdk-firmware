param(
    [string]$Path = '.\hardware\outputs\records\captures\2026-10-07_three_node_star_bringup\six_node_star_test_2026-10-09_140700.json'
)

$content = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
$byNode = $content.diagnostics | Group-Object source_node
foreach ($g in $byNode) {
    Write-Host ("=== Source Node {0} ===" -f $g.Name)
    $byStage = $g.Group | Group-Object stage_name
    foreach ($s in $byStage) {
        $last = $s.Group[-1]
        Write-Host ("  {0,-30}: last={1,10} (hex=0x{2:X8})" -f $s.Name, $last.detail, [int64]$last.detail)
    }
}

