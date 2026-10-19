# Pair 5 v2 diagnostic capture after LEFT-WRIST restart — 30 September 2026

## Result

This is a 60-second capture on COM16 after the user restarted LEFT-WRIST. The
user reported PELVIS and LEFT-WRIST were 50 cm apart. Both boards had the
matching temporary pair-5 v2 diagnostic firmware described in the [build
record](../../firmware/2026-09-30_pair5_pre_recovery_tx_status_v2/README.md).
The recorder discarded 3 seconds of buffered data before recording. This is
diagnostic evidence, **not an accepted calibration**.

The restart restored LEFT-WRIST telemetry for this capture: both PELVIS and
LEFT-WRIST produced six-axis IMU data at about 60 Hz. The pair exchanged UWB
frames, but the ranging is still failing often and valid reported distances
are far from the stated 50 cm.

| Measure | Result |
|---|---:|
| Raw capture duration | 60 s |
| IMU records | 7,163: PELVIS 3,581; LEFT-WRIST 3,582 |
| IMU sequence gaps | PELVIS 26; LEFT-WRIST 3 |
| Other wearable IMU records | 0 |
| Pair-5 UWB reports received by gateway | 115, all marked valid by firmware |
| Reported-range rate | 1.92 reports/s over 59.9 s |
| Reported distances | median 15 cm; mean 26.54 cm; sample SD 22.49 cm; range 5–105 cm |
| Reports within 45–55 cm | 6/115 (5.2%) |
| PELVIS UWB attempt-counter change | 3,317 attempts in 58.1 s: 2,122 valid, 1,195 invalid (36.0% invalid) |
| PELVIS result timeouts / failed BLE UWB notifications | 0 / 0 additional |
| UWB diagnostic events | 2,267 |
| Gateway invalid frames | 0 |

The UWB report stream is rate-limited, so its 115 reports are **not** the total
number of radio attempts. The attempt and valid/invalid proportions above come
from the difference between the first and last PELVIS health-counter records;
their 58.1-second span is slightly shorter than the full capture. Firmware
`quality_flags = 1` means the range was accepted as non-negative, not that it
was accurate. The spread and 15 cm median show these values cannot be used as
50 cm calibration data.

## What the radio diagnostics show

The failures are on the UWB exchange path, not gateway frame decoding or BLE
notification delivery:

| Diagnostic | Count | What the captured status shows |
|---|---:|---|
| LEFT-WRIST `responder_response_tx_failed` | 857 | 667 had `HPDWARN` set; 190 had the driver's `TXERR` state |
| PELVIS `final_tx_failed` | 188 | 167 had `HPDWARN` set; 21 had `TXERR` |
| PELVIS `response_wait_failed` | 1,038 | 1,037 carried `RXFTO`, meaning the expected response was not received before the receive-frame timeout |
| LEFT-WRIST `responder_final_not_received` | 179 | The responder did not receive the final frame for those exchanges |

These events can describe different stages of the same exchange, so do not
divide 2,267 diagnostic events by 3,317 attempts to calculate a failure rate.
Use the health-counter delta (36.0% invalid attempts) for that rate.

The main new clue is the repeated `HPDWARN` on delayed transmissions by **both**
boards. The current pair firmware schedules the responder reply 2 ms after
poll reception and the PELVIS final frame 3 ms after response reception
([timing definitions in `main.c`](../../../../firmware/archive/dwm3001cdk_legacy_proto/src/main.c)).
Qorvo describes HPDWARN as a half-period warning for delayed TX/RX; its API
guide says such warnings relate to late invocation of delayed transmission or
reception ([Qorvo DW3xxx API Guide](https://forum.qorvo.com/uploads/short-url/xD3TlXKvkujjdUaJWXv2b4E7GQN.pdf)).
This is strong evidence that the delayed-TX timing margin needs investigation.
It does not, by itself, explain the large errors among ranges that were marked
valid, so those must be checked again after timing failures are reduced.

## Files

- Raw capture: `../../acquisition/gateway_raw_2026-09-30_171551.bin`
- Capture metadata: `../../acquisition/gateway_raw_2026-09-30_171551.json`
- Decoded records: `../../acquisition/gateway_raw_2026-09-30_171551_decoded/`
- Summary: `../../acquisition/gateway_raw_2026-09-30_171551_decoded/summary.json`
- UWB reports: `../../acquisition/gateway_raw_2026-09-30_171551_decoded/uwb.ndjson`
- UWB diagnostic events: `../../acquisition/gateway_raw_2026-09-30_171551_decoded/uwb_diag.ndjson`

Raw capture SHA-256: `08B39515B354C6A1F2D258DC2CC90FE9F4E130F7EB6F99E27C2F3560BB2C1D63`.

## Next step

Keep this as a pair-only diagnostic result; do not apply a range correction or
copy these temporary images to the other four boards. The evidence supports a
controlled firmware timing-margin test on PELVIS and LEFT-WRIST, because both
delayed-TX stages logged HPDWARN. Preserve the current backups and diagnostic
capture, change only pair-test timing, then repeat the same fixed 50 cm setup.
Check both the invalid-attempt rate and range spread before resuming
calibration. The valid-range error may have an additional cause and is not
considered solved by this capture.
