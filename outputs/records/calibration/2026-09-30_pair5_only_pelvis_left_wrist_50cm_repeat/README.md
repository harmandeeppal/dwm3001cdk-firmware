# Pair-only PELVIS–LEFT-WRIST repeat test at 50 cm

## Result

This is the second 60-second binary capture at a stated antenna-to-antenna
distance of 50 cm, with the same pair-only PELVIS firmware and battery setup.

| Measure | Result |
|---|---:|
| Valid range reports received by gateway | 113 |
| Reported time span | 59.5 s |
| Report rate | 1.90 reports/s |
| Median | 65 cm |
| Mean | 64.82 cm |
| Sample standard deviation | 2.54 cm |
| Minimum / maximum | 61 / 71 cm |
| Other UWB pairs in captured range records | 0 |

The median was 15 cm above the stated distance. This run is close to the first
50 cm run (median 64 cm, SD 2.78 cm), but accuracy is not accepted: the
measurement has an offset at this distance and many radio attempts still
fail.

## Why attempts are invalid

PELVIS node-status counters changed by:

- 3,310 UWB attempts: 1,610 valid and 1,700 invalid (51.4% invalid).
- 3 result timeouts.
- 0 failed UWB BLE notifications.
- 25 IMU queue drops.
- The capture contains 3,573 PELVIS and 3,591 LEFT-WRIST IMU samples, with 26
  and 18 sequence gaps respectively.

There are 3,059 diagnostic stage events. These are messages from both ends
of the same radio exchanges and are not 3,059 separate attempts. The main
stages were:

| Diagnostic stage | Events | Meaning in this firmware |
|---|---:|---|
| `response_wait_failed` | 1,314 | PELVIS did not receive LEFT-WRIST's response in its receive window |
| `responder_response_tx_failed` | 925 | LEFT-WRIST's delayed response transmit did not start successfully |
| `final_tx_failed` | 405 | PELVIS's delayed final transmit did not start successfully |
| `responder_final_not_received` | 408 | LEFT-WRIST did not receive the final frame |
| Other | 7 | Poll not received, invalid responder range, and coordinator result timeouts |

This localizes most failures to the UWB exchange, not BLE transport. Source
inspection shows LEFT-WRIST schedules its response 2 ms after receiving the
poll. The Qorvo DW3xxx API Guide says delayed `dwt_starttx()` can abort and
return an error when the host calls it too late. That makes response timing a
strong suspect, but not a confirmed root cause: this firmware currently logs
a generic failure code and does not save the DW3110 status/error flags needed
to prove why each delayed transmit failed.

References: [Qorvo DW3xxx API Guide, `dwt_starttx`](https://forum.qorvo.com/uploads/short-url/xD3TlXKvkujjdUaJWXv2b4E7GQN.pdf),
and [`mesh_run_responder_slot` in the project firmware](../../../../firmware/archive/dwm3001cdk_legacy_proto/src/main.c).

## Files

- Raw gateway bytes: `gateway_raw_2026-09-30_143101.bin`
- Capture metadata: `gateway_raw_2026-09-30_143101.json`
- Decoded records: `gateway_raw_2026-09-30_143101_decoded/`
- Ranges: `gateway_raw_2026-09-30_143101_decoded/uwb.ndjson`
- Stage diagnostics: `gateway_raw_2026-09-30_143101_decoded/uwb_diag.ndjson`

## Next step

Before fitting a distance correction, make a diagnostic firmware revision that
records the actual `dwt_starttx()` return value and relevant DW3110 status
flags at the responder and initiator. Then test a longer response margin in a
controlled pair-only run. Do not change firmware or apply a calibration
correction until that targeted test is reviewed. Keep the normal full-mesh
PELVIS image ready for restoration before six-node use.
