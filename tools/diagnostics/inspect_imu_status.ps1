$path = "hardware/outputs/records/captures/2026-10-07_three_node_star_bringup/six_node_star_test_2026-10-09_111327.json"
$data = Get-Content $path -Raw | ConvertFrom-Json

Write-Output "=== IMU COUNTS AND RATES ==="
$data.imu_counts | ConvertTo-Json

Write-Output "=== NODE STATUS LATEST ==="
$data.node_status_latest | ConvertTo-Json -Depth 4
