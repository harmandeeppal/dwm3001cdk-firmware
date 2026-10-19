param(
    [string]$Port = 'COM16',
    [int]$BaudRate = 115200,
    [ValidateRange(0, 3600)]
    [int]$DurationSeconds = 0,
    [ValidateRange(0, 120)]
    [int]$WarmupSeconds = 20,
    [ValidateRange(-1, 14)]
    [int]$Pair = -1,
    [ValidateRange(0, 10000)]
    [int]$KnownDistanceCm = 0,
    [string]$OutputDirectory = '.\hardware\outputs\records\acquisition\live_gateway_viewer'
)

$ErrorActionPreference = 'Stop'
if ($KnownDistanceCm -gt 0 -and $Pair -lt 0) {
    throw 'Set -Pair as well as -KnownDistanceCm when checking measurement error.'
}

$nodeNames = @('HEAD', 'PELVIS', 'LEFT-WRIST', 'RIGHT-WRIST', 'LEFT-ANKLE', 'RIGHT-ANKLE')
$pairNames = @(
    'HEAD <-> PELVIS', 'HEAD <-> LEFT-WRIST', 'HEAD <-> RIGHT-WRIST',
    'HEAD <-> LEFT-ANKLE', 'HEAD <-> RIGHT-ANKLE', 'PELVIS <-> LEFT-WRIST',
    'PELVIS <-> RIGHT-WRIST', 'PELVIS <-> LEFT-ANKLE', 'PELVIS <-> RIGHT-ANKLE',
    'LEFT-WRIST <-> RIGHT-WRIST', 'LEFT-WRIST <-> LEFT-ANKLE',
    'LEFT-WRIST <-> RIGHT-ANKLE', 'RIGHT-WRIST <-> LEFT-ANKLE',
    'RIGHT-WRIST <-> RIGHT-ANKLE', 'LEFT-ANKLE <-> RIGHT-ANKLE'
)
$diagnosticNames = @{
    0x20 = 'poll_tx_failed'; 0x21 = 'response_wait_failed';
    0x22 = 'response_bad_length'; 0x23 = 'response_wrong_source_or_type';
    0x25 = 'final_tx_failed'; 0x30 = 'responder_poll_not_received';
    0x32 = 'responder_response_tx_failed'; 0x33 = 'responder_final_not_received';
    0x40 = 'coordinator_result_timeout'; 0x41 = 'coordinator_command_failed';
    0x42 = 'responder_reported_invalid_range'; 0x43 = 'responder_range_calculation_failed';
    0x50 = 'factory_otp_ch5_prf64_read_only'
    0x60 = 'fira_prepare_measurement_sequence_failed';
    0x61 = 'uwbmac_set_promiscuous_mode_failed';
    0x62 = 'uwbmac_set_short_addr_failed'; 0x63 = 'fira_output_buffer_allocate_failed';
    0x64 = 'fira_helper_open_failed'; 0x65 = 'fira_helper_set_scheduler_failed';
    0x66 = 'fira_helper_init_session_failed';
    0x67 = 'fira_set_session_parameters_failed';
    0x68 = 'fira_helper_add_controlee_failed'; 0x69 = 'uwbmac_start_failed';
    0x6A = 'fira_helper_start_session_failed'
    0xD1 = 'direct_fira_uwb_mcps_init_failed';
    0xD2 = 'direct_fira_helper_open_failed';
    0xD3 = 'direct_fira_scheduler_failed';
    0xD4 = 'direct_fira_measurement_sequence_failed';
    0xD5 = 'direct_fira_session_init_failed';
    0xD6 = 'direct_fira_session_parameters_failed';
    0xD7 = 'direct_fira_add_controlee_failed';
    0xD8 = 'direct_fira_uwbmac_start_failed';
    0xD9 = 'direct_fira_session_start_failed'
    0xC0 = 'ram_only_persistence_guard';
    0xC1 = 'official_fira_uwbmac_ready';
    0xC2 = 'official_fira_scheduler_ready';
    0xC3 = 'official_fira_session_ready';
    0xC4 = 'direct_fira_uwbmac_started';
    0xC5 = 'direct_fira_session_started'
}

$nodes = @()
foreach ($name in $nodeNames) {
    $axisStats = @()
    foreach ($axisName in @('ax','ay','az','gx','gy','gz')) {
        $axisStats += [pscustomobject]@{ Name = $axisName; Count = 0L; Mean = 0.0; M2 = 0.0; Min = $null; Max = $null }
    }
    $nodes += [pscustomobject]@{
        Name = $name; Sequence = $null; PreviousSequence = $null; TimestampUs = $null
        RateHz = $null; Ax = $null; Ay = $null; Az = $null; Gx = $null; Gy = $null; Gz = $null
        Samples = 0L; SequenceGaps = 0L; Duplicates = 0L; ResetsOrOutOfOrder = 0L
        AxisStats = $axisStats; Seen = $null; Status = $null; StatusBaseline = $null
    }
}
$ranges = @{}
for ($pairIndex = 0; $pairIndex -lt 15; $pairIndex++) {
    $ranges[$pairIndex] = [pscustomobject]@{
        LastValidCm = $null; LastValidSeen = $null; LastEventSeen = $null
        Valid = 0L; Invalid = 0L; Values = (New-Object 'System.Collections.Generic.List[double]')
        Events = (New-Object 'System.Collections.Generic.List[object]')
    }
}
$diagnostics = @{}
$diagnosticEvents = New-Object 'System.Collections.Generic.List[object]'
$factoryOtp = @{}
$gateway = $null
$gatewayBaseline = $null
$buffer = New-Object 'System.Collections.Generic.List[byte]'
$sessionStarted = $null
$sessionDirectory = $null
$sessionSaved = $false

function Get-AgeText($seen) {
    if ($null -eq $seen) { return 'waiting' }
    $seconds = ((Get-Date) - $seen).TotalSeconds
    if ($seconds -lt 2) { return ('live {0:N1}s' -f $seconds) }
    return ('stale {0:N1}s' -f $seconds)
}

function Get-RangeStatistics([double[]]$Values) {
    if ($Values.Count -eq 0) {
        return [ordered]@{ count = 0; min_cm = $null; median_cm = $null; mean_cm = $null; sd_cm = $null; max_cm = $null }
    }
    $sorted = @($Values | Sort-Object)
    $count = $sorted.Count
    if (($count % 2) -eq 1) { $median = $sorted[[int][math]::Floor($count / 2)] }
    else { $median = ($sorted[($count / 2) - 1] + $sorted[$count / 2]) / 2.0 }
    $mean = ($sorted | Measure-Object -Average).Average
    $sumSquares = 0.0
    foreach ($value in $sorted) { $sumSquares += [math]::Pow(($value - $mean), 2) }
    $sd = if ($count -gt 1) { [math]::Sqrt($sumSquares / ($count - 1)) } else { 0.0 }
    return [ordered]@{
        count = $count; min_cm = [math]::Round($sorted[0], 2); median_cm = [math]::Round($median, 2)
        mean_cm = [math]::Round($mean, 2); sd_cm = [math]::Round($sd, 2)
        max_cm = [math]::Round($sorted[$count - 1], 2)
    }
}

function Get-KnownDistanceMetrics([double[]]$Values, [double]$KnownCm) {
    if ($Values.Count -eq 0) { return $null }
    $errors = @($Values | ForEach-Object { [double]$_ - $KnownCm })
    $absoluteErrors = @($errors | ForEach-Object { [math]::Abs($_) })
    $withinFive = @($absoluteErrors | Where-Object { $_ -le 5 }).Count
    $withinTen = @($absoluteErrors | Where-Object { $_ -le 10 }).Count
    return [ordered]@{
        valid_samples = $Values.Count
        mean_bias_cm = [math]::Round(($errors | Measure-Object -Average).Average, 2)
        mean_absolute_error_cm = [math]::Round(($absoluteErrors | Measure-Object -Average).Average, 2)
        within_5cm_percent = [math]::Round(100.0 * $withinFive / $Values.Count, 2)
        within_10cm_percent = [math]::Round(100.0 * $withinTen / $Values.Count, 2)
    }
}

function Get-CounterDelta([uint32]$Current, [uint32]$Baseline) {
    if ($Current -ge $Baseline) { return [uint64]$Current - [uint64]$Baseline }
    # A board restart resets its local counters. Report the new-boot count
    # instead of a misleading negative delta.
    return [uint64]$Current
}

function Add-AxisValue($stat, [double]$value) {
    $stat.Count++
    if ($null -eq $stat.Min -or $value -lt $stat.Min) { $stat.Min = $value }
    if ($null -eq $stat.Max -or $value -gt $stat.Max) { $stat.Max = $value }
    $delta = $value - $stat.Mean
    $stat.Mean += $delta / $stat.Count
    $stat.M2 += $delta * ($value - $stat.Mean)
}

function Get-AxisSummary($stat) {
    if ($stat.Count -eq 0) { return [ordered]@{ count = 0; min = $null; mean = $null; sd = $null; max = $null } }
    $sd = if ($stat.Count -gt 1) { [math]::Sqrt($stat.M2 / ($stat.Count - 1)) } else { 0.0 }
    return [ordered]@{
        count = $stat.Count; min = [math]::Round($stat.Min, 2); mean = [math]::Round($stat.Mean, 2)
        sd = [math]::Round($sd, 2); max = [math]::Round($stat.Max, 2)
    }
}

function Update-Imu([byte[]]$payload) {
    if ($payload.Length -ne 38 -or $payload[0] -ne 1 -or $payload[1] -ne 1) { return }
    $index = [int]$payload[2]
    if ($index -ge $nodes.Count) { return }
    $node = $nodes[$index]
    $sequence = [BitConverter]::ToUInt32($payload, 8)
    $timestamp = [BitConverter]::ToUInt32($payload, 12)
    if ($null -ne $node.Sequence) {
        # Unsigned subtraction handles the sample counter wrapping at 2^32.
        $delta = ([uint64]$sequence + 0x100000000L - [uint64]$node.Sequence) -band 0xFFFFFFFFL
        if ($delta -eq 0) { $node.Duplicates++ }
        elseif ($delta -lt 0x80000000L) { if ($delta -gt 1) { $node.SequenceGaps += ($delta - 1) } }
        else { $node.ResetsOrOutOfOrder++ }
    }
    if ($null -ne $node.Sequence -and $null -ne $node.TimestampUs) {
        $timeDelta = ([uint64]$timestamp + 0x100000000L - [uint64]$node.TimestampUs) -band 0xFFFFFFFFL
        $seqDelta = ([uint64]$sequence + 0x100000000L - [uint64]$node.Sequence) -band 0xFFFFFFFFL
        if ($timeDelta -gt 0 -and $timeDelta -lt 0x80000000L -and $seqDelta -gt 0 -and $seqDelta -lt 0x80000000L) {
            $node.RateHz = [math]::Round(($seqDelta * 1000000.0) / $timeDelta, 1)
        }
    }
    $node.PreviousSequence = $node.Sequence
    $node.Sequence = $sequence; $node.TimestampUs = $timestamp; $node.Samples++
    $node.Ax = [BitConverter]::ToInt16($payload, 16); $node.Ay = [BitConverter]::ToInt16($payload, 18)
    $node.Az = [BitConverter]::ToInt16($payload, 20); $node.Gx = [BitConverter]::ToInt16($payload, 22)
    $node.Gy = [BitConverter]::ToInt16($payload, 24); $node.Gz = [BitConverter]::ToInt16($payload, 26)
    $axisValues = @($node.Ax,$node.Ay,$node.Az,$node.Gx,$node.Gy,$node.Gz)
    for ($axis = 0; $axis -lt $axisValues.Count; $axis++) { Add-AxisValue $node.AxisStats[$axis] $axisValues[$axis] }
    $node.Seen = Get-Date
}

function Update-Uwb([byte[]]$payload) {
    if ($payload.Length -ne 24 -or $payload[0] -ne 1 -or $payload[1] -ne 3) { return }
    $pairIndex = [int]$payload[3]
    if ($pairIndex -ge 15) { return }
    $entry = $ranges[$pairIndex]
    $now = Get-Date
    $entry.LastEventSeen = $now
    $range = [BitConverter]::ToInt32($payload, 8)
    $flags = [BitConverter]::ToUInt16($payload, 12)
    $valid = ($range -ge 0) -and (($flags -band 1) -ne 0)
    $event = [pscustomobject]@{
        received_utc = $now.ToUniversalTime().ToString('o'); pair_index = $pairIndex
        pair = $pairNames[$pairIndex]; pelvis_timestamp_us = [BitConverter]::ToUInt32($payload, 4)
        range_cm = $range; flags = ('0x{0:X4}' -f $flags); valid = $valid
        gateway_receive_timestamp_us = [BitConverter]::ToUInt64($payload, 16)
    }
    $entry.Events.Add($event)
    if ($valid) {
        $entry.Valid++; $entry.Values.Add([double]$range)
        $entry.LastValidCm = $range; $entry.LastValidSeen = $now
    } else { $entry.Invalid++ }
}

function Update-NodeStatus([byte[]]$payload) {
    if ($payload.Length -ne 60 -or $payload[0] -ne 1 -or $payload[1] -ne 4) { return }
    $index = [int]$payload[2]
    if ($index -ge $nodes.Count) { return }
    $status = [pscustomobject]@{
        Flags = $payload[3]
        ImuAcquired = [BitConverter]::ToUInt32($payload, 8)
        ImuQueueDrops = [BitConverter]::ToUInt32($payload, 16)
        ImuNotifyFailures = [BitConverter]::ToUInt32($payload, 28)
        UwbNotifyFailures = [BitConverter]::ToUInt32($payload, 32)
        UwbAttempts = [BitConverter]::ToUInt32($payload, 36)
        UwbValid = [BitConverter]::ToUInt32($payload, 40)
        UwbInvalid = [BitConverter]::ToUInt32($payload, 44)
        UwbTimeouts = [BitConverter]::ToUInt32($payload, 48)
        Seen = Get-Date
    }
    if ($null -eq $nodes[$index].StatusBaseline) { $nodes[$index].StatusBaseline = $status }
    $nodes[$index].Status = $status
}

function Update-UwbDiagnostic([byte[]]$payload) {
    if ($payload.Length -ne 24 -or $payload[0] -ne 1 -or $payload[1] -ne 5) { return }
    $pairIndex = [int]$payload[3]
    if ($pairIndex -ge 15) { return }
    $stageCode = [int]$payload[4]
    $stage = if ($diagnosticNames.ContainsKey($stageCode)) { $diagnosticNames[$stageCode] } else { 'unknown_stage_0x{0:X2}' -f $stageCode }
    $nodeIndex = [int]$payload[2]
    # Keep the raw unsigned word as well as the legacy signed field.  The
    # factory word is a bit field, not a signed distance/error value.
    $detailUnsigned = [BitConverter]::ToUInt32($payload, 12)
    if ($stageCode -eq 0x50 -and $nodeIndex -lt $nodes.Count) {
        $factoryOtp[$nodeIndex] = [pscustomobject]@{
            Seen = Get-Date
            RawWord = $detailUnsigned
            RxDelay = (($detailUnsigned -shr 16) -band 0xFFFF)
            TxDelay = ($detailUnsigned -band 0xFFFF)
        }
    }
    $diagnosticEvents.Add([pscustomobject]@{
        received_utc = (Get-Date).ToUniversalTime().ToString('o'); node_index = $nodeIndex
        pair_index = $pairIndex; pair = $pairNames[$pairIndex]; stage_code = ('0x{0:X2}' -f $stageCode)
        stage = $stage; local_timestamp_us = [BitConverter]::ToUInt32($payload, 8)
        detail = [BitConverter]::ToInt32($payload, 12); detail_unsigned_hex = ('0x{0:X8}' -f $detailUnsigned)
    })
    if (-not $diagnostics.ContainsKey($pairIndex)) { $diagnostics[$pairIndex] = @{} }
    if (-not $diagnostics[$pairIndex].ContainsKey($stage)) { $diagnostics[$pairIndex][$stage] = 0L }
    $diagnostics[$pairIndex][$stage]++
}

function Update-GatewayStatus([byte[]]$payload) {
    if ($payload.Length -ne 24 -or $payload[0] -ne 1 -or $payload[1] -ne 128) { return }
    $script:gateway = [pscustomobject]@{
        Packets = [BitConverter]::ToUInt32($payload, 4); BadPackets = [BitConverter]::ToUInt32($payload, 8)
        SequenceGaps = [BitConverter]::ToUInt32($payload, 12); UsbDrops = [BitConverter]::ToUInt32($payload, 16)
        QueueHighWater = [BitConverter]::ToUInt32($payload, 20); Seen = Get-Date
    }
    # The first status received after the warm-up is the baseline.  This makes
    # the displayed USB and sequence counters describe this session only.
    if ($null -eq $script:gatewayBaseline) {
        $script:gatewayBaseline = [pscustomobject]@{
            Packets = $script:gateway.Packets; BadPackets = $script:gateway.BadPackets
            SequenceGaps = $script:gateway.SequenceGaps; UsbDrops = $script:gateway.UsbDrops
        }
    }
}

function Draw-Warmup([int]$SecondsRemaining) {
    Clear-Host
    Write-Host 'COMP971 live telemetry and quality viewer' -ForegroundColor Cyan
    Write-Host ''
    Write-Host 'Preparing a clean measurement session' -ForegroundColor Yellow
    Write-Host ("The gateway stream is being drained. Metrics and records start in {0} second(s)." -f $SecondsRemaining)
    Write-Host 'Nothing received during this preparation period will be included in the saved report.' -ForegroundColor DarkGray
}

function Parse-Frames {
    while ($buffer.Count -ge 6) {
        if ($buffer[0] -ne 0xC9 -or $buffer[1] -ne 0x71) { $buffer.RemoveAt(0); continue }
        $version = $buffer[2]; $type = $buffer[3]
        $length = [int]$buffer[4] + (256 * [int]$buffer[5])
        if ($version -ne 1 -or $length -gt 64) { $buffer.RemoveAt(0); continue }
        $frameLength = 6 + $length
        if ($buffer.Count -lt $frameLength) { break }
        $payload = [byte[]]::new($length)
        for ($i = 0; $i -lt $length; $i++) { $payload[$i] = $buffer[6 + $i] }
        $buffer.RemoveRange(0, $frameLength)
        switch ($type) {
            1 { Update-Imu $payload }
            3 { Update-Uwb $payload }
            4 { Update-NodeStatus $payload }
            5 { Update-UwbDiagnostic $payload }
            128 { Update-GatewayStatus $payload }
        }
    }
}

function Get-StatusDelta($status, $baseline, $property) {
    if ($null -eq $status -or $null -eq $baseline) { return '-' }
    return Get-CounterDelta $status.$property $baseline.$property
}

function Draw-Viewer {
    Clear-Host
    $elapsed = if ($null -eq $sessionStarted) { 0 } else { ((Get-Date) - $sessionStarted).TotalSeconds }
    Write-Host 'COMP971 live telemetry and quality viewer' -ForegroundColor Cyan
    Write-Host ("Gateway {0} at {1} baud | recorded session {2:N0}s | Refresh 1s | Ctrl+C stops and saves" -f $Port, $BaudRate, $elapsed)
    Write-Host 'IMU values are raw registers (not orientation). Range statistics cover valid reports since this viewer started.' -ForegroundColor DarkGray
    if ($DurationSeconds -gt 0) { Write-Host ("Auto-stop after {0}s." -f $DurationSeconds) -ForegroundColor DarkGray }
    Write-Host ''
    Write-Host 'IMU - raw sensor records (sequence gaps are missing IMU samples, not UWB failures)' -ForegroundColor Yellow
    Write-Host 'NODE         STATE         RATE Hz  SAMPLES   GAPS   GAP%  DUP  SEQUENCE   ACCEL [ X      Y      Z ]      GYRO [ X      Y      Z ]' -ForegroundColor DarkGray
    Write-Host '------------ ------------  -------  -------  -----  -----  ---  ---------  -----------------------  -----------------------' -ForegroundColor DarkGray
    foreach ($index in @(0, 2, 1, 3, 4, 5)) {
        $node = $nodes[$index]
        $rate = if ($null -eq $node.RateHz) { '   --  ' } else { '{0,7:N1}' -f $node.RateHz }
        $seq = if ($null -eq $node.Sequence) { '       --' } else { '{0,9}' -f $node.Sequence }
        $denominator = $node.Samples + $node.SequenceGaps
        $gapPct = if ($denominator -gt 0) { 100.0 * $node.SequenceGaps / $denominator } else { 0.0 }
        $axes = if ($null -eq $node.Sequence) { 'waiting for IMU' } else { '[{0,6} {1,6} {2,6}]  [{3,6} {4,6} {5,6}]' -f $node.Ax,$node.Ay,$node.Az,$node.Gx,$node.Gy,$node.Gz }
        Write-Host ('{0,-12} {1,-12} {2} {3,8} {4,6} {5,6:N2} {6,4} {7}   {8}' -f $node.Name,(Get-AgeText $node.Seen),$rate,$node.Samples,$node.SequenceGaps,$gapPct,$node.Duplicates,$seq,$axes)
    }
    Write-Host ''
    Write-Host 'UWB - latest result and received-report delivery' -ForegroundColor Yellow
    Write-Host 'ID  PAIR                            STATE          LAST cm   VALID  INVALID   FAIL%  RATE/s' -ForegroundColor DarkGray
    Write-Host '--  ------------------------------  -------------  -------  ------  -------  ------  ------' -ForegroundColor DarkGray
    for ($pairIndex = 0; $pairIndex -lt 15; $pairIndex++) {
        $entry = $ranges[$pairIndex]
        $stats = Get-RangeStatistics -Values ([double[]]$entry.Values.ToArray())
        $events = $entry.Valid + $entry.Invalid
        $failPct = if ($events -gt 0) { 100.0 * $entry.Invalid / $events } else { 0.0 }
        $rate = if ($elapsed -gt 0) { $events / $elapsed } else { 0.0 }
        $last = if ($null -eq $entry.LastValidCm) { '     --' } else { '{0,7:N0}' -f $entry.LastValidCm }
        Write-Host ('{0,2}  {1,-30}  {2,-13}  {3}  {4,6}  {5,7}  {6,6:N2}  {7,6:N2}' -f $pairIndex,$pairNames[$pairIndex],(Get-AgeText $entry.LastValidSeen),$last,$entry.Valid,$entry.Invalid,$failPct,$rate)
    }
    Write-Host ''
    Write-Host 'UWB - valid-range statistics in centimetres (only the valid reports above)' -ForegroundColor Yellow
    Write-Host 'ID  PAIR                            SAMPLES    MIN  MEDIAN    MEAN      SD    MAX' -ForegroundColor DarkGray
    Write-Host '--  ------------------------------  -------  -----  ------  ------  ------  -----' -ForegroundColor DarkGray
    for ($pairIndex = 0; $pairIndex -lt 15; $pairIndex++) {
        $entry = $ranges[$pairIndex]
        $stats = Get-RangeStatistics -Values ([double[]]$entry.Values.ToArray())
        if ($stats.count -eq 0) {
            Write-Host ('{0,2}  {1,-30}  {2,7}  {3,5}  {4,6}  {5,6}  {6,6}  {7,5}' -f $pairIndex,$pairNames[$pairIndex],'--','--','--','--','--','--')
        } else {
            Write-Host ('{0,2}  {1,-30}  {2,7}  {3,5:N0}  {4,6:N0}  {5,6:N1}  {6,6:N1}  {7,5:N0}' -f $pairIndex,$pairNames[$pairIndex],$stats.count,$stats.min_cm,$stats.median_cm,$stats.mean_cm,$stats.sd_cm,$stats.max_cm)
        }
    }
    Write-Host 'FAIL% is invalid UWB reports received by the gateway. Radio-attempt validity is shown in the PELVIS health table below.' -ForegroundColor DarkGray
    if ($Pair -ge 0 -and $KnownDistanceCm -gt 0) {
        $values = @($ranges[$Pair].Values.ToArray())
        $knownMetrics = Get-KnownDistanceMetrics -Values ([double[]]$values) -KnownCm $KnownDistanceCm
        if ($null -ne $knownMetrics) {
            Write-Host ''
            Write-Host 'Known-distance accuracy - valid reports only' -ForegroundColor Cyan
            Write-Host 'PAIR                            STATED cm  BIAS cm    MAE cm  WITHIN 5 cm  WITHIN 10 cm' -ForegroundColor DarkGray
            Write-Host '------------------------------  ---------  -------  --------  -----------  ------------' -ForegroundColor DarkGray
            Write-Host ('{0,-30}  {1,9:N1}  {2,7:N1}  {3,8:N1}  {4,10:N1}%  {5,11:N1}%' -f $pairNames[$Pair],$KnownDistanceCm,$knownMetrics.mean_bias_cm,$knownMetrics.mean_absolute_error_cm,$knownMetrics.within_5cm_percent,$knownMetrics.within_10cm_percent) -ForegroundColor Cyan
        } else { Write-Host ("Known-distance check: waiting for valid reports for pair {0}." -f $Pair) -ForegroundColor DarkGray }
    }
    Write-Host ''
    Write-Host 'ICM connection state reported by each wearable' -ForegroundColor Yellow
    foreach ($index in @(0, 2, 1, 3, 4, 5)) {
        $s = $nodes[$index].Status
        $state = if ($null -eq $s) { 'waiting for node status' }
                 elseif (($s.Flags -band 0x02) -ne 0) { 'ICM not detected over I2C' }
                 elseif (($s.Flags -band 0x01) -ne 0) {
                     if (($s.Flags -band 0x04) -ne 0) { 'ICM ready; UWB started' }
                     else { 'ICM ready; UWB not started' }
                 }
                 elseif (($s.Flags -band 0x04) -ne 0) { 'UWB started; ICM status unavailable' }
                 else { 'unknown status flag' }
        Write-Host ('{0,-12} {1}' -f $nodeNames[$index],$state)
    }
    Write-Host ''
    Write-Host 'Wearable health counter changes since first status in this session (board-reported totals)' -ForegroundColor Yellow
    Write-Host 'NODE         IMU QUEUE-DROP  IMU BLE-FAIL  UWB BLE-FAIL  UWB ATTEMPT  VALID  INVALID INVALID% TIMEOUT' -ForegroundColor DarkGray
    foreach ($index in @(0, 2, 1, 3, 4, 5)) {
        $node = $nodes[$index]; $s = $node.Status; $b = $node.StatusBaseline
        $row = if ($null -eq $s) { 'waiting for node health status' } else {
            $attemptDelta = Get-StatusDelta $s $b 'UwbAttempts'
            $invalidDelta = Get-StatusDelta $s $b 'UwbInvalid'
            $invalidPct = if ($attemptDelta -gt 0 -and $invalidDelta -isnot [string]) { 100.0 * $invalidDelta / $attemptDelta } else { 0.0 }
            '{0,14} {1,12} {2,12} {3,12} {4,7} {5,7} {6,8:N1}% {7,7}' -f `
                (Get-StatusDelta $s $b 'ImuQueueDrops'),(Get-StatusDelta $s $b 'ImuNotifyFailures'),
                (Get-StatusDelta $s $b 'UwbNotifyFailures'),$attemptDelta,
                (Get-StatusDelta $s $b 'UwbValid'),$invalidDelta,$invalidPct,
                (Get-StatusDelta $s $b 'UwbTimeouts')
        }
        Write-Host ('{0,-12} {1}' -f $node.Name,$row)
    }
    if ($factoryOtp.Count -gt 0) {
        Write-Host ''
        Write-Host 'Factory UWB calibration readout - channel 5, PRF 64, read only' -ForegroundColor Cyan
        Write-Host 'NODE         STATE          RAW OTP WORD  RX DELAY  TX DELAY' -ForegroundColor DarkGray
        Write-Host '-----------  -------------  ------------  --------  --------' -ForegroundColor DarkGray
        foreach ($index in @(0, 2, 1, 3, 4, 5)) {
            if ($factoryOtp.ContainsKey($index)) {
                $otp = $factoryOtp[$index]
                Write-Host ('{0,-11}  {1,-13}  0x{2:X8}  {3,8}  {4,8}' -f $nodeNames[$index],(Get-AgeText $otp.Seen),$otp.RawWord,$otp.RxDelay,$otp.TxDelay) -ForegroundColor Cyan
            }
        }
        Write-Host 'These are factory-programmed delay values. This reader does not change them or write to OTP.' -ForegroundColor DarkGray
    }
    $diagTotal = 0L
    foreach ($p in $diagnostics.Keys) { foreach ($stage in $diagnostics[$p].Keys) { $diagTotal += $diagnostics[$p][$stage] } }
    Write-Host ("UWB setup/failure events received: {0} (C1-C5 are setup progress; D1-D9 are failures; a factory-OTP read is configuration evidence, not a failure)." -f $diagTotal) -ForegroundColor DarkGray
    if ($null -eq $gateway) { Write-Host 'Gateway USB health: waiting' -ForegroundColor DarkGray }
    elseif ($null -eq $gatewayBaseline) {
        Write-Host 'Gateway USB health: waiting for the first fresh gateway-status record.' -ForegroundColor DarkGray
    } else {
        $packetDelta = [math]::Max(0, [int64]$gateway.Packets - [int64]$gatewayBaseline.Packets)
        $badDelta = [math]::Max(0, [int64]$gateway.BadPackets - [int64]$gatewayBaseline.BadPackets)
        $gapDelta = [math]::Max(0, [int64]$gateway.SequenceGaps - [int64]$gatewayBaseline.SequenceGaps)
        $usbDelta = [math]::Max(0, [int64]$gateway.UsbDrops - [int64]$gatewayBaseline.UsbDrops)
        Write-Host ("Gateway after old-buffer drain: packets +{0}; bad BLE packets +{1}; IMU sequence gaps +{2}; new USB drops +{3}" -f $packetDelta,$badDelta,$gapDelta,$usbDelta) -ForegroundColor DarkGray
        Write-Host ("Gateway totals since dongle restart: packets {0}; malformed {1}; IMU gaps {2}; USB drops {3}; USB queue high-water {4}" -f $gateway.Packets,$gateway.BadPackets,$gateway.SequenceGaps,$gateway.UsbDrops,$gateway.QueueHighWater) -ForegroundColor DarkGray
    }
    if ($null -ne $sessionDirectory) { Write-Host ("Session report will be saved on exit to: {0}" -f $sessionDirectory) -ForegroundColor DarkGray }
}

function Save-SessionReport {
    if ($sessionSaved -or $null -eq $sessionStarted -or $null -eq $sessionDirectory) { return }
    $elapsed = [math]::Max(0.001, ((Get-Date) - $sessionStarted).TotalSeconds)
    $pairSummary = @()
    $allEvents = New-Object 'System.Collections.Generic.List[object]'
    for ($pairIndex = 0; $pairIndex -lt 15; $pairIndex++) {
        $entry = $ranges[$pairIndex]
        $stats = Get-RangeStatistics -Values ([double[]]$entry.Values.ToArray())
        $events = $entry.Valid + $entry.Invalid
        $pairDiags = @{}
        if ($diagnostics.ContainsKey($pairIndex)) { $pairDiags = $diagnostics[$pairIndex] }
        $pairSummary += [ordered]@{
            pair_index = $pairIndex; pair = $pairNames[$pairIndex]; valid_reports = $entry.Valid
            invalid_reports = $entry.Invalid; report_success_percent = if ($events -gt 0) { [math]::Round(100.0 * $entry.Valid / $events, 2) } else { $null }
            reports_per_second = [math]::Round($events / $elapsed, 3); range_statistics = $stats
            failure_diagnostic_events_by_stage = $pairDiags
        }
        foreach ($event in $entry.Events) { $allEvents.Add($event) }
    }
    $nodeSummary = @()
    foreach ($node in $nodes) {
        $s = $node.Status; $b = $node.StatusBaseline
        $health = if ($null -eq $s) { $null } else {
            [ordered]@{
                imu_queue_drops = Get-CounterDelta $s.ImuQueueDrops $b.ImuQueueDrops
                imu_ble_notify_failures = Get-CounterDelta $s.ImuNotifyFailures $b.ImuNotifyFailures
                uwb_ble_notify_failures = Get-CounterDelta $s.UwbNotifyFailures $b.UwbNotifyFailures
                uwb_attempts = Get-CounterDelta $s.UwbAttempts $b.UwbAttempts
                uwb_valid = Get-CounterDelta $s.UwbValid $b.UwbValid
                uwb_invalid = Get-CounterDelta $s.UwbInvalid $b.UwbInvalid
                uwb_timeouts = Get-CounterDelta $s.UwbTimeouts $b.UwbTimeouts
                uwb_invalid_percent_of_attempts = if ((Get-CounterDelta $s.UwbAttempts $b.UwbAttempts) -gt 0) { [math]::Round(100.0 * (Get-CounterDelta $s.UwbInvalid $b.UwbInvalid) / (Get-CounterDelta $s.UwbAttempts $b.UwbAttempts), 2) } else { $null }
            }
        }
        $nodeSummary += [ordered]@{
            node = $node.Name; samples_received = $node.Samples; observed_rate_hz = $node.RateHz
            sequence_gaps_seen_by_viewer = $node.SequenceGaps; duplicate_sequences = $node.Duplicates
            sequence_gap_percent_between_observed_samples = if (($node.Samples + $node.SequenceGaps) -gt 0) { [math]::Round(100.0 * $node.SequenceGaps / ($node.Samples + $node.SequenceGaps), 3) } else { $null }
            resets_or_out_of_order = $node.ResetsOrOutOfOrder; last_sequence = $node.Sequence
            raw_register_statistics = [ordered]@{
                accelerometer_x = Get-AxisSummary $node.AxisStats[0]; accelerometer_y = Get-AxisSummary $node.AxisStats[1]
                accelerometer_z = Get-AxisSummary $node.AxisStats[2]; gyroscope_x = Get-AxisSummary $node.AxisStats[3]
                gyroscope_y = Get-AxisSummary $node.AxisStats[4]; gyroscope_z = Get-AxisSummary $node.AxisStats[5]
            }
            health_counter_changes = $health
        }
    }
    $gatewaySummary = $null
    if ($null -ne $gateway) {
        $gatewaySummary = [ordered]@{
            latest_totals_since_dongle_restart = [ordered]@{
                packets = $gateway.Packets; malformed_packets = $gateway.BadPackets; sequence_gaps = $gateway.SequenceGaps
                usb_drops = $gateway.UsbDrops; usb_queue_high_water = $gateway.QueueHighWater
            }
            viewer_window_deltas = if ($null -ne $gatewayBaseline) {
                [ordered]@{
                    packets = [math]::Max(0, [int64]$gateway.Packets - [int64]$gatewayBaseline.Packets)
                    malformed_packets = [math]::Max(0, [int64]$gateway.BadPackets - [int64]$gatewayBaseline.BadPackets)
                    sequence_gaps = [math]::Max(0, [int64]$gateway.SequenceGaps - [int64]$gatewayBaseline.SequenceGaps)
                    usb_drops = [math]::Max(0, [int64]$gateway.UsbDrops - [int64]$gatewayBaseline.UsbDrops)
                }
            } else { $null }
        }
    }
    $knownDistanceSummary = $null
    if ($Pair -ge 0 -and $KnownDistanceCm -gt 0) {
        $knownMetrics = Get-KnownDistanceMetrics -Values ([double[]]$ranges[$Pair].Values.ToArray()) -KnownCm $KnownDistanceCm
        $knownDistanceSummary = [ordered]@{
            pair_index = $Pair; pair = $pairNames[$Pair]; known_distance_cm = $KnownDistanceCm
            statistics_for_valid_samples = $knownMetrics
        }
    }
    $summary = [ordered]@{
        format = 'COMP971 live gateway viewer session summary v1'
        started_utc = $sessionStarted.ToUniversalTime().ToString('o'); ended_utc = (Get-Date).ToUniversalTime().ToString('o')
        duration_seconds = [math]::Round($elapsed, 3); gateway_port = $Port; gateway_baud = $BaudRate
        known_distance_check = $knownDistanceSummary
        note = 'Per-pair report success uses valid/invalid UWB result records received by the gateway; it is not the full radio-attempt success rate. Failure-stage events may repeat within an exchange and are not unique attempts. Use wearable health counters for attempt/invalid/timeout totals.'
        imu_nodes = $nodeSummary; uwb_pairs = $pairSummary; gateway_health = $gatewaySummary
    }
    $summary | ConvertTo-Json -Depth 9 | Set-Content -LiteralPath (Join-Path $sessionDirectory 'summary.json') -Encoding UTF8
    if ($allEvents.Count -gt 0) { $allEvents | Export-Csv -LiteralPath (Join-Path $sessionDirectory 'uwb_reports.csv') -NoTypeInformation -Encoding UTF8 }
    else { 'received_utc,pair_index,pair,pelvis_timestamp_us,range_cm,flags,valid,gateway_receive_timestamp_us' | Set-Content -LiteralPath (Join-Path $sessionDirectory 'uwb_reports.csv') -Encoding UTF8 }
    if ($diagnosticEvents.Count -gt 0) { $diagnosticEvents | Export-Csv -LiteralPath (Join-Path $sessionDirectory 'uwb_failure_diagnostics.csv') -NoTypeInformation -Encoding UTF8 }
    else { 'received_utc,node_index,pair_index,pair,stage_code,stage,local_timestamp_us,detail' | Set-Content -LiteralPath (Join-Path $sessionDirectory 'uwb_failure_diagnostics.csv') -Encoding UTF8 }
    $sessionSaved = $true
    Write-Host "`nSaved session summary: $(Join-Path $sessionDirectory 'summary.json')" -ForegroundColor Green
    Write-Host "Saved received UWB reports: $(Join-Path $sessionDirectory 'uwb_reports.csv')" -ForegroundColor Green
    Write-Host "Saved UWB failure-stage records: $(Join-Path $sessionDirectory 'uwb_failure_diagnostics.csv')" -ForegroundColor Green
}

$serial = [System.IO.Ports.SerialPort]::new($Port, $BaudRate, 'None', 8, 'One')
$serial.DtrEnable = $true; $serial.RtsEnable = $true; $serial.ReadTimeout = 100
try {
    $serial.Open()
    # Do not mix records that were buffered before the user started this run
    # with the measurement session.  Read and discard them for the requested
    # warm-up period, then begin a new clean recording window.
    $warmupStarted = Get-Date
    while (((Get-Date) - $warmupStarted).TotalSeconds -lt $WarmupSeconds) {
        $available = $serial.BytesToRead
        if ($available -gt 0) {
            $discard = [byte[]]::new($available)
            [void]$serial.Read($discard, 0, $available)
        } else { Start-Sleep -Milliseconds 25 }
        $remaining = [math]::Max(0, [math]::Ceiling($WarmupSeconds - ((Get-Date) - $warmupStarted).TotalSeconds))
        Draw-Warmup $remaining
        Start-Sleep -Milliseconds 200
    }
    $buffer.Clear()
    $sessionStarted = Get-Date
    $stamp = $sessionStarted.ToUniversalTime().ToString('yyyyMMdd_HHmmss')
    $sessionDirectory = Join-Path $OutputDirectory "session_$stamp"
    New-Item -ItemType Directory -Force -Path $sessionDirectory | Out-Null
    $nextDraw = Get-Date
    while ($true) {
        $available = $serial.BytesToRead
        if ($available -gt 0) {
            $chunk = [byte[]]::new($available)
            $read = $serial.Read($chunk, 0, $available)
            for ($i = 0; $i -lt $read; $i++) { $buffer.Add($chunk[$i]) }
            Parse-Frames
        } else { Start-Sleep -Milliseconds 25 }
        if ((Get-Date) -ge $nextDraw) { Draw-Viewer; $nextDraw = (Get-Date).AddSeconds(1) }
        if ($DurationSeconds -gt 0 -and ((Get-Date) - $sessionStarted).TotalSeconds -ge $DurationSeconds) { break }
    }
} finally {
    if ($serial.IsOpen) { $serial.Close() }
    Save-SessionReport
}
