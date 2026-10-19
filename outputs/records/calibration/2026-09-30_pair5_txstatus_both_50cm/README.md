# Pair-5 transmit-status capture with both nodes instrumented — 50 cm

## Result

This was a 60-second raw capture at stated 50 cm with PELVIS and LEFT-WRIST
on battery and the other wearables off. Both pair nodes had the temporary
transmit-status diagnostic firmware. All 114 UWB range reports were pair 5.

| Measure | Result |
|---|---:|
| Valid range reports | 114 in 59.65 s (~1.91/s) |
| Reported range median | 90 cm |
| Reported range min / max | 85 / 93 cm |
| PELVIS attempts | 3,327 |
| Valid / invalid attempts | 1,599 / 1,728 |
| Invalid attempt rate | 51.9% |
| PELVIS attempts per second | ~55.7/s (about 18 ms apart on average) |
| Valid distance reports reaching the gateway | ~1.91/s |
| Failed UWB BLE notifications | 0 |
| Invalid framed gateway records | 0 |

The median is 40 cm above the stated 50 cm. UWB accuracy is not accepted.
The 51.9% rate comes from PELVIS's internal attempt counters; it is not the
fraction of BLE reports that failed.

## Failure stages and captured status

| Stage | Events | Captured detail |
|---|---:|---:|
| `response_wait_failed` (PELVIS) | 1,303 | 131,184 |
| `responder_response_tx_failed` (LEFT-WRIST) | 855 | 0 |
| `final_tx_failed` (PELVIS) | 458 | 0 |
| `responder_final_not_received` (LEFT-WRIST) | 453 | -1 |

The failed transmit events on both boards now have status detail `0`, unlike
the earlier run where LEFT-WRIST reported `-1` because it was not yet
instrumented. In this firmware, `detail` is the raw low word of DW3110
`SYS_STATUS` read after `dwt_starttx()` returns an error. A zero value alone
does **not** identify the cause.

The Qorvo driver used by these v1 images checks `HPDWARN`; if that flag is clear, it also
checks whether `SYS_STATE_LO` equals `DW_SYS_STATE_TXERR`. It then turns the
transceiver off before returning. Since this application reads `SYS_STATUS`
after that return, the current capture does not preserve the pre-cleanup state
needed to distinguish those cases. See
`hardware/firmware/archive/dwm3001cdk_legacy_proto/lib/dwt_uwb_driver/dw3000/dw3000_device.c`
around the delayed-TX handling and `src/main.c` around
`MESH_TX_FAILURE_DETAIL()`.

These stage events can describe both ends of the same failed ranging exchange;
do not sum them as unique failed attempts. The zero BLE-notification failure
count and the radio-stage errors indicate the failure is before delivery of
the range result over BLE, not a Bluetooth notification drop.

## Files

- Raw capture: `../../acquisition/gateway_raw_2026-09-30_153826.bin`
- Metadata: `../../acquisition/gateway_raw_2026-09-30_153826.json`
- Decoded files: `../../acquisition/gateway_raw_2026-09-30_153826_decoded/`
- Diagnostic events: `../../acquisition/gateway_raw_2026-09-30_153826_decoded/uwb_diag.ndjson`
- Node counters: `../../acquisition/gateway_raw_2026-09-30_153826_decoded/node_status.ndjson`
- Summary: `../../acquisition/gateway_raw_2026-09-30_153826_decoded/summary.json`

## Next

Do not change the response delay based on this result. The v2 pair-only images
save `SYS_STATUS` and `SYS_STATE_LO` before the driver's error-recovery
command. Matching v2 images are now installed on PELVIS and LEFT-WRIST. Both
boards were backed up with duplicate-matched full-flash and UICR reads; their
previous v1 images verified before writing. Repeat the controlled pair-5 test
and decode the preserved status. See the [v2 firmware record](../../firmware/2026-09-30_pair5_pre_recovery_tx_status_v2/README.md),
[PELVIS backup](../../../../backups/DWM3001CDK_760224787/2026-09-30_pre_txstatus_v2_backup/README.md),
and [LEFT-WRIST backup](../../../../backups/DWM3001CDK_760225868/2026-09-30_pre_txstatus_v2_backup/README.md).
Do not change the response delay until that capture is reviewed.
