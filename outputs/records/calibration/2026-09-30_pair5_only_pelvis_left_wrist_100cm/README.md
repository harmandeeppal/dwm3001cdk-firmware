# Pair-only PELVIS–LEFT-WRIST test at 100 cm

## Result

The temporary PELVIS firmware successfully measured only pair 5,
PELVIS–LEFT-WRIST. The stated antenna-to-antenna spacing was 100 cm.

| Measure | Result |
|---|---:|
| Valid range reports received by gateway | 112 |
| Reported time span | 59.4 s |
| Report rate | 1.89 reports/s |
| Median | 97 cm |
| Mean | 102.0 cm |
| Sample standard deviation | 13.2 cm |
| Minimum / maximum | 86 / 145 cm |
| Other UWB pairs in captured range records | 0 |

The median is 3 cm below the stated distance, but the readings still spread
widely. This is one distance and one run, so it is **not an accepted
calibration** and no correction has been applied.

## What the counters mean

The PELVIS node-status counters increased during the capture by:

- 3,245 UWB attempts: 1,915 valid and 1,330 invalid (41.0% invalid).
- 2 result timeouts.
- 0 failed UWB BLE notifications.
- 26 IMU queue drops.

The gateway received 112 range reports because the firmware limits valid
reports for each pair to at most 2 per second. Thus 112 is not the number of
radio attempts. The status counters show the radio exchange outcomes; the
gateway range file contains the rate-limited reports. The 2,534 diagnostic
events are stage messages from both sides of failed exchanges, and must not be
counted as 2,534 separate attempts.

The capture contained 3,573 PELVIS and 3,582 LEFT-WRIST IMU records. Their
sequence-gap counts were 26 and 18. This test image retains each board's BLE
IMU stream while skipping remote IMU polling over UWB.

## Files and repeatable capture method

- Raw gateway bytes: `gateway_raw_2026-09-30_140530.bin`
- Capture metadata: `gateway_raw_2026-09-30_140530.json`
- Decoded records: `gateway_raw_2026-09-30_140530_decoded/`
- Range records: `gateway_raw_2026-09-30_140530_decoded/uwb.ndjson`
- Summary and failure-stage events: `summary.json` and `uwb_diag.ndjson` in
  the decoded folder.

The current gateway uses framed binary transport. Its raw capture and decoder
were used for this result. The older `capture_uwb_calibration.ps1` reads a
legacy text format; it produced an empty JSON during this session and is not
evidence that the boards failed to range. That empty file is retained but
marked invalid below.

Repeat with the same firmware, battery power, clear path, antenna orientation,
and measured antenna-to-antenna spacing at 75 cm and 50 cm. Repeat 100 cm once
more. Compare median error, spread, report rate, and invalid-attempt rate
across runs before deciding whether a calibration correction is justified.

## Equipment and firmware state

- PELVIS J-Link serial `760224787`: pair-5-only temporary image installed and
  verified.
- LEFT-WRIST J-Link serial `760225868`: unchanged; battery-powered.
- Other wearable boards: off during this test.
- Gateway: COM16, framed binary protocol v1.
- No final/full-mesh firmware is claimed by this test image. Restore the
  normal PELVIS full-mesh build before resuming all-six-node operation.
