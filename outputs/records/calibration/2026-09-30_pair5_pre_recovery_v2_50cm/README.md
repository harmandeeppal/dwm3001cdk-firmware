# Pair 5 v2 diagnostic capture — 30 September 2026

## Result

This 60-second capture was made on COM16 after installing matching pair-only
v2 diagnostic firmware on PELVIS and LEFT-WRIST. The capture command discarded
the first 3 seconds of buffered data. The previous instruction was to set the
pair 50 cm apart, but the distance and physical power state were not repeated
in the capture message, so treat 50 cm as the intended setup, not a separately
confirmed measurement.

**This is not a usable calibration capture.** It contains no valid pair-5
range. It shows that PELVIS repeatedly waited for a UWB response and timed
out. LEFT-WRIST produced no BLE IMU or node-status records in this capture.
That points to LEFT-WRIST not being active/connected for this test; it does not
prove the board is damaged or explain why it did not answer.

| Measure | Result |
|---|---:|
| Raw capture duration | 60 s |
| IMU records | 3,581, all from PELVIS (node 1) |
| LEFT-WRIST (node 2) IMU / status records | 0 / 0 |
| UWB report records | 60, all pair 5 and invalid (`-1 cm`) |
| PELVIS counter change | 2,524 attempts, 0 valid, 2,524 invalid |
| PELVIS result-collection timeouts | 0 additional |
| UWB diagnostic events | 2,558; all `response_wait_failed` at PELVIS, pair 5 |
| Status value in those events | `131184` = `0x00020070`; RX frame-wait timeout bit (`RXFTO`) is set |
| Gateway invalid frames | 0 |

The 60 displayed range records are not the total number of radio attempts:
the gateway limits invalid range reports. The PELVIS internal counter is the
better measure here. The status `RXFTO` means PELVIS reached its receive-frame
timeout without receiving a valid response. It identifies the point of
failure, not the reason the other board did not reply. Possible causes still
include LEFT-WRIST being unpowered/not running, not receiving PELVIS's
command, or a UWB response failure. No timing change is justified from this
capture alone.

The v2 firmware did not change ranging timing. Its programming and separate
post-reset verification succeeded on both boards. The captured status is a
receive-wait timeout, not a delayed-transmit failure report, so this capture
does not yet answer the original delayed-transmit question.

## Files

- Raw binary: `../../acquisition/gateway_raw_2026-09-30_170324.bin`
- Capture metadata: `../../acquisition/gateway_raw_2026-09-30_170324.json`
- Decoded records: `../../acquisition/gateway_raw_2026-09-30_170324_decoded/`
- Decoded summary: `../../acquisition/gateway_raw_2026-09-30_170324_decoded/summary.json`
- Pair-5 failure events: `../../acquisition/gateway_raw_2026-09-30_170324_decoded/uwb_diag.ndjson`

SHA-256 of the raw binary: `943AE931420F2E1AB0D34AE1CD5402F9D0B900997E47D9B7989BE9788974CDCC`.

## Next check

Before another distance capture, connect LEFT-WRIST to J9 so its boot and
connection can be checked directly. Keep PELVIS on battery and the other four
wearables off. Do not reflash either pair board or change the UWB delay unless
the next diagnostic evidence points to that change.

## References

- Firmware schedule and receive status handling: [`main.c`](../../../../firmware/archive/dwm3001cdk_legacy_proto/src/main.c)
- DW3110 status-bit definitions: [`dw3000_deca_regs.h`](../../../../firmware/archive/dwm3001cdk_legacy_proto/lib/dwt_uwb_driver/dw3000/dw3000_deca_regs.h)
- Qorvo support reference defining `RXFTO` as receive-frame-wait timeout: [Qorvo status-register discussion](https://forum.qorvo.com/t/dwm3000-status-register-values-error-codes/10422); [DW3xxx API Guide](https://forum.qorvo.com/uploads/short-url/xD3TlXKvkujjdUaJWXv2b4E7GQN.pdf)
