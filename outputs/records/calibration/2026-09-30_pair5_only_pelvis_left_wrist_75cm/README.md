# Pair-only PELVIS–LEFT-WRIST test at 75 cm

## Result

The temporary PELVIS firmware measured only pair 5, PELVIS–LEFT-WRIST. The
stated antenna-to-antenna spacing was 75 cm.

| Measure | Result |
|---|---:|
| Valid range reports received by gateway | 116 |
| Reported time span | 59.8 s |
| Report rate | 1.94 reports/s |
| Median | 89 cm |
| Mean | 88.34 cm |
| Sample standard deviation | 1.03 cm |
| Minimum / maximum | 85 / 91 cm |
| Other UWB pairs in captured range records | 0 |

This run was very steady, but the median was 14 cm above the stated distance.
It is one run at one distance; it is **not an accepted calibration** and no
correction has been applied.

## Node counters and IMU

From the PELVIS node-status counters during the capture:

- 3,362 UWB attempts: 2,271 valid and 1,091 invalid (32.5% invalid).
- 0 result timeouts and 0 failed UWB BLE notifications.
- 28 IMU queue drops.
- Gateway received 116 reports in about 60 seconds because valid reports are
  limited to at most 2 Hz per pair; this is not the number of radio attempts.
- The capture contains 3,573 PELVIS and 3,582 LEFT-WRIST IMU records. Their
  sequence-gap counts were 27 and 19.

There were 2,126 diagnostic stage events, mainly `response_wait_failed`
(1,021) and `responder_response_tx_failed` (936). These are event messages
from the two ends of the exchanges, not a count of distinct attempts.

## Files

- Raw gateway bytes: `gateway_raw_2026-09-30_141424.bin`
- Capture metadata: `gateway_raw_2026-09-30_141424.json`
- Decoded records: `gateway_raw_2026-09-30_141424_decoded/`
- Range records: `gateway_raw_2026-09-30_141424_decoded/uwb.ndjson`
- Failure-stage records: `gateway_raw_2026-09-30_141424_decoded/uwb_diag.ndjson`

## Interpretation

Compared with the pair-only 100 cm run (median 97 cm, sample SD 13.2 cm),
this run's 75 cm median was 89 cm and SD 1.03 cm. The error changes with
distance, so a single fixed offset is not justified. Continue with a 50 cm
run and repeat the 100 cm run under the same conditions. Restore the normal
full-mesh PELVIS build before returning to six-node operation.
