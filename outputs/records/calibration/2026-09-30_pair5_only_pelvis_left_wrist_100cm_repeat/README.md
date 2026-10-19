# Pair-only PELVIS–LEFT-WRIST repeat test at 100 cm

## Result

This is the second 60-second binary capture at a stated antenna-to-antenna
distance of 100 cm, using the same pair-only PELVIS image and setup.

| Measure | Result |
|---|---:|
| Valid range reports received by gateway | 113 |
| Reported time span | 59.3 s |
| Report rate | 1.91 reports/s |
| Median | 100 cm |
| Mean | 107.76 cm |
| Sample standard deviation | 18.44 cm |
| Minimum / maximum | 91 / 164 cm |
| Other UWB pairs in captured range records | 0 |

The median is on target, but the large spread and high readings mean this is
not a calibration pass.

## Node counters and IMU

From the PELVIS node-status counter changes during this capture:

- 3,347 UWB attempts: 1,597 valid and 1,750 invalid (52.3% invalid).
- 4 result timeouts and 0 failed UWB BLE notifications.
- 25 IMU queue drops.
- The capture contains 3,573 PELVIS and 3,582 LEFT-WRIST IMU records. Their
  sequence-gap counts were 25 and 19.
- There were 3,049 diagnostic-stage events. These are messages from both
  ends of exchanges, not a count of distinct attempts.

## Files

- Raw gateway bytes: `gateway_raw_2026-09-30_142614.bin`
- Capture metadata: `gateway_raw_2026-09-30_142614.json`
- Decoded records: `gateway_raw_2026-09-30_142614_decoded/`
- Range records: `gateway_raw_2026-09-30_142614_decoded/uwb.ndjson`
- Failure-stage records: `gateway_raw_2026-09-30_142614_decoded/uwb_diag.ndjson`

## Comparison with the first 100 cm run

| Run | Valid reports | Median | Mean | Sample SD | Min / max | Invalid attempts |
|---|---:|---:|---:|---:|---:|---:|
| First | 112 | 97 cm | 102.04 cm | 13.21 cm | 86 / 145 cm | 41.0% |
| Repeat | 113 | 100 cm | 107.76 cm | 18.44 cm | 91 / 164 cm | 52.3% |
| Combined | 225 | 98 cm | 104.92 cm | 16.27 cm | 86 / 164 cm | — |

The two medians differ by 3 cm, but both runs contain high outliers and many
invalid radio attempts. The close medians alone are not enough to accept the
measurement system. Across the single runs at 50 and 75 cm, the medians were
64 and 89 cm, both 14 cm above the stated distance. Repeat 50 and 75 cm under
the same geometry before deciding whether any calibration curve is justified.
