$ids = @(0x0CF7323C, 0x641B24BE, 0xE901480F, 0x289AA3B2, 0x04A5CFAA)
$names = @("HEAD", "LEFT-WRIST", "RIGHT-WRIST", "LEFT-ANKLE", "RIGHT-ANKLE")
for ($idx = 0; $idx -lt 5; $idx++) {
    $raw = $ids[$idx]
    $j1 = $raw % 1000
    Write-Host ("{0,-12}: ID=0x{1:X8} -> mod 1000 = {2} ms" -f $names[$idx], $raw, $j1)
}

