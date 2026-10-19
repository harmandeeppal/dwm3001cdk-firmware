# Pair-only PELVIS–LEFT-WRIST test at 50 cm

## Result

The temporary PELVIS firmware measured only pair 5, PELVIS–LEFT-WRIST. The
stated antenna-to-antenna spacing was 50 cm.

| Measure | Result |
|---|---:|
| Valid range reports received by gateway | 114 |
| Reported time span | 59.9 s |
| Report rate | 1.90 reports/s |
| Median | 64 cm |
| Mean | 64.49 cm |
| Sample standard deviation | 2.78 cm |
| Minimum / maximum | 59 / 71 cm |
| Other UWB pairs in captured range records | 0 |

The median was 14 cm above the stated distance. This is one run at one
distance; it is **not an accepted calibration** and no correction has been
applied.

## Node counters and IMU

From the PELVIS node-status counters during the capture:

- 3,393 UWB attempts: 1,935 valid and 1,458 invalid (43.0% invalid).
- 0 result timeouts and 0 failed UWB BLE notifications.
- 27 IMU queue drops.
- The gateway received 114 reports in about 60 seconds because valid reports
  are limited to at most 2 Hz per pair. This is not the number of radio
  attempts.
- The capture contains 3,573 PELVIS and 3,582 LEFT-WRIST IMU records. Their
  sequence-gap counts were 28 and 18.

There were 2,602 diagnostic stage events. They are failure-stage messages
from both ends of exchanges, not a count of distinct attempts.

## Files

- Raw gateway bytes: `gateway_raw_2026-09-30_142225.bin`
- Capture metadata: `gateway_raw_2026-09-30_142225.json`
- Decoded records: `gateway_raw_2026-09-30_142225_decoded/`
- Range records: `gateway_raw_2026-09-30_142225_decoded/uwb.ndjson`
- Failure-stage records: `gateway_raw_2026-09-30_142225_decoded/uwb_diag.ndjson`

## Comparison and next step

| Stated distance | Median | Median error | Sample SD | Invalid attempts |
|---:|---:|---:|---:|---:|
| 50 cm | 64 cm | +14 cm | 2.78 cm | 43.0% |
| 50 cm, repeat | 65 cm | +15 cm | 2.54 cm | 51.4% |
| 75 cm | 89 cm | +14 cm | 1.03 cm | 32.5% |
| 100 cm, first run | 97 cm | -3 cm | 13.2 cm | 41.0% |
| 100 cm, repeat | 100 cm | 0 cm | 18.44 cm | 52.3% |

The two 50 cm runs agree closely on median (64 and 65 cm), but both have many
invalid radio attempts. The 75 cm result is still a single run. Do not fit a
correction until the UWB response-stage failures are investigated; see the
[50 cm repeat failure analysis](../2026-09-30_pair5_only_pelvis_left_wrist_50cm_repeat/README.md).
Restore the normal full-mesh PELVIS build before six-node operation.
