# PELVIS–LEFT-WRIST pair-5 v3 timing test — nominal 50 cm

## Result in plain words

The extra UWB reply time appears to have greatly reduced invalid ranging
attempts in this run. It did **not** fix distance accuracy: with the pair at a
user-reported 50 cm, the median reported value was only 8 cm and readings
ranged from 1 to 98 cm. This is a diagnostic test, not calibration data.

## Setup and firmware

- Date: 30 September 2026
- Pair: PELVIS (node 1, J-Link serial `760224787`) and LEFT-WRIST (node 2,
  J-Link serial `760225868`)
- Nominal distance: 50 cm, held at the same position used for the preceding
  test as instructed; the acquisition file does not independently record a
  ruler measurement.
- Both boards used the pair-5-only v3 diagnostic firmware. The wrist response
  delay is about 5 ms (previously about 2 ms); the PELVIS final-message delay
  is about 6 ms (previously about 3 ms).
- Other four wearables were off. The gateway was connected on COM16.
- Capture duration: 60 seconds, after discarding 3 seconds of buffered data.
- This test changed only the two pair-test node images. It did not change the
  gateway or the other four wearables.

## Main measurements

| Measure | v3 result |
|---|---:|
| Radio attempts counted by PELVIS health counters | 2,754 in 58.2 s (about 47.3/s) |
| Attempt results counted valid by PELVIS | 2,735 |
| Attempt results counted invalid by PELVIS | 19 (0.69%) |
| Result timeouts / UWB BLE notification failures | 0 / 0 additional |
| Valid range records delivered to the gateway | 118 in 60 s (1.97/s) |
| Reported range median / mean | 8 cm / 16.31 cm |
| Reported range sample SD / min–max | 19.04 cm / 1–98 cm |
| Reports within 45–55 cm | 5/118 (4.2%) |
| Reports within 40–60 cm | 8/118 (6.8%) |
| IMU records | 7,154: PELVIS 3,573; LEFT-WRIST 3,581 |
| IMU record rates | PELVIS 59.6 Hz; LEFT-WRIST 59.7 Hz |
| IMU sequence gaps | PELVIS 24; LEFT-WRIST 19 |
| New BLE IMU notification failures / USB drops / bad frames | 0 / 0 / 0 |

The 118 laptop range records are not the number of radio attempts: the
firmware limits valid BLE reports to about two per second per pair. The PELVIS
health counters are the source for the attempt failure rate. `quality_flags =
1` means firmware accepted a non-negative result; it does not mean the value
is accurate.

## Comparison with the previous v2 test

The preceding v2 run at the same nominal 50 cm reported 3,317 attempts in
58.1 s, with 1,195 invalid (36.0%). The v3 run reported 19 invalid of 2,754
(0.69%). That is a large improvement in the attempt-validity counter for
these two individual runs. It supports the idea that delayed-TX preparation
time was a major cause of invalid transactions, but is not by itself proof
that all invalids are solved.

Distance accuracy remains unacceptable. The earlier v2 median was 15 cm; the
v3 median is 8 cm, both far below 50 cm. The v3 run still has a 19.04 cm
spread and only five of 118 reports fell within 5 cm of the stated distance.
Do not apply an offset or use these readings for motion analysis.

The decoder found 35 diagnostic stage events: PELVIS reported 5
`response_wait_failed` and 14 `responder_reported_invalid_range` events;
LEFT-WRIST reported 2 `responder_final_not_received` and 14
`responder_range_calculation_failed` events. Stage events can refer to the
same underlying exchange, so these 35 events are not a separate failure-rate
denominator. The PELVIS health-counter delta gives the 0.69% attempt invalid
rate.

## IMU and gateway health

Both IMU streams stayed close to 60 Hz. The decoder counted 24 PELVIS and 19
LEFT-WRIST sequence gaps. Node health counters recorded 23 and 19 new queue
drops respectively, and no new BLE IMU/UWB notification failures. Gateway
health increased by 42 sequence gaps, reported zero bad packets, and showed
no new USB drops. The decoder's per-node gap sum is 43, one more than the
gateway counter delta; retain this small counter discrepancy for later review.

## Files

- Raw capture: `../../acquisition/gateway_raw_2026-09-30_180708.bin`
- Capture metadata: `../../acquisition/gateway_raw_2026-09-30_180708.json`
- Decoded records: `../../acquisition/gateway_raw_2026-09-30_180708_decoded/`
- Summary: `../../acquisition/gateway_raw_2026-09-30_180708_decoded/summary.json`
- Range records: `../../acquisition/gateway_raw_2026-09-30_180708_decoded/uwb.ndjson`
- Diagnostic events: `../../acquisition/gateway_raw_2026-09-30_180708_decoded/uwb_diag.ndjson`
- Firmware install and source/build details: `../../firmware/2026-09-30_pair5_delayed_tx_margin_v3/README.md`

Raw capture SHA-256: `5F357CA6D2FEA1B01E63FA6BC23015DA6AA045F6A8CD773215B878798E93EC60`.

## Conclusion and next step

Keep this as a successful reduction in invalid attempts but a failed distance-
accuracy test. Do not calibrate from it. Before changing firmware timing
again, the firmware configuration needs one check.

The DS-TWR code uses the POLL, RESPONSE, and FINAL timestamps in its range
formula; a simple missing-timestamp error was not found in this review. But
the v3 compile commands do not define per-board `UWB_TX_ANT_DLY` or
`UWB_RX_ANT_DLY`. The source therefore uses its fallback value `16390` for
both boards, then explicitly programs those values into the transceiver. The
current registry's PELVIS value `16378` is historical for a PELVIS–LEFT-ANKLE
pair and marked pending final calibration; LEFT-WRIST has no stored value.
Neither value should be copied to this pair without measurement.

Qorvo's DWM3001CDK guide says to calibrate antenna delay with two boards at a
known distance; for named E1.0 engineering samples it says to use default
`16390`. A Qorvo support response also says DWM3001CDK calibrated values can
be stored in OTP. The exact board/module revision and OTP values have not yet
been checked here. The Qorvo guide also explains that antenna-delay settings
correct the time between the radio's digital timestamp and its antenna.
Sources: [DWM3001CDK Quick Start Guide, section 9](https://www.mouser.com/datasheet/2/412/Qovro_7_7_2022_DWM3001CDK_Quick_Start_Guide-2998998.pdf),
[Qorvo support response on DWM3001CDK OTP calibration](https://forum.qorvo.com/t/how-to-reduce-distance-d-cm-error/20404),
and [DW3xxx API Guide, antenna-delay functions](https://forum.qorvo.com/uploads/short-url/xD3TlXKvkujjdUaJWXv2b4E7GQN.pdf).

Next, read and record the two boards' actual antenna-delay calibration data
without writing to OTP. Confirming the board revision and these values should
come before changing the v3 delay settings or fitting a range correction. The
delay test did not establish final firmware for the six-node system.
