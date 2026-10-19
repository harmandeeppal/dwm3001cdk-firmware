# LEFT-WRIST pair-5 transmit-status diagnostic build

## Purpose

This temporary image adds the same delayed-transmit status logging as the
PELVIS diagnostic image, so LEFT-WRIST can report status when its UWB reply
transmit fails. Pair 5 remains the PELVIS–LEFT-WRIST test. The pair-only
schedule filter is checked only by the PELVIS coordinator; on LEFT-WRIST it
does not change its responder role.

This is a bench diagnostic, not final six-node firmware. It does not change
the UWB timing or the 2 ms response delay.

## Build

- Target: `nrf52833dk/nrf52833`
- Board identity: LEFT-WRIST, mesh node index 2
- Pair test index: 5 (PELVIS–LEFT-WRIST)
- BLE config: `ble_left_wrist.conf`
- Overlay: `nrf52833_dk.overlay`
- Diagnostic option: `COMP971_UWB_TX_STATUS_DIAGNOSTICS=ON`
- SDK: nRF Connect SDK 3.4.0 / Zephyr 4.4.0
- Build: successful, 215 steps; 147,748 bytes FLASH use
- Compile command verified `COMP971_UWB_PAIR_ONLY_INDEX=5`,
  `COMP971_UWB_TX_STATUS_DIAGNOSTICS=1`, and
  `UWB_MESH_NODE_INDEX=2`.

## Image

| File | Size | SHA-256 |
|---|---:|---|
| `DWM-03-LEFT-WRIST__pair5-txstatus-diagnostic__nrf52833.hex` | 415,728 bytes | `2EB1CD48A473D86FE835167D2E42AC81C1E41059AF8A9360626BF31C82C9052E` |

## Installation state

- On 2026-09-30, programmed to LEFT-WRIST serial `760225868` with
  `nrfjprog --program --sectorerase --verify --reset --family NRF52 --snr
  760225868`. The tool reported write successful, verify successful, and
  reset/run. Exit code: 0. It printed SEGGER JLinkARM.dll error -256 warnings,
  but the write and verify stages completed successfully.
- Before writing, a fresh 512 KiB flash and 4 KiB UICR readback was saved in
  `hardware/backups/DWM3001CDK_760225868/2026-09-30_pre_txstatus_diagnostic/`.
  Both pairs of reads matched; all 147,736 bytes of its prior firmware matched
  the archived LEFT-WRIST failure-stage HEX.
- PELVIS remains on the separate pair-5 diagnostic image. Other boards remain
  unchanged and should stay off for this test.
- Restore the normal LEFT-WRIST image after this bench test.

## Live test state

Both endpoints had v1 transmit-status diagnostic firmware before the v2 update:
PELVIS serial `760224787` and LEFT-WRIST serial `760225868`. Matching v2 images
are now installed and verified on both boards, after duplicate-matched
full-flash/UICR backups and checks of their previous images. The v1 controlled
50 cm capture is recorded at
`hardware/outputs/records/calibration/2026-09-30_pair5_txstatus_both_50cm/`.
LEFT-WRIST's 855 failed reply-transmit events all reported detail `0`. This
post-return value still does not distinguish the driver's HPDWARN and TXERR
failure paths. Matched driver-level v2 images preserve those values before
TX-off recovery. Both are installed; the next step is a new controlled 50 cm
capture. See
`../2026-09-30_pair5_pre_recovery_tx_status_v2/README.md`.

## Earlier capture before LEFT-WRIST was instrumented

The 60-second capture on 2026-09-30 counted 3,300 PELVIS ranging attempts:
1,991 valid and 1,309 invalid (39.7%). It contained 1,036
`responder_response_tx_failed` events from LEFT-WRIST. Those events carried
detail `-1` because LEFT-WRIST did not yet have transmit-status logging
enabled. The later both-node capture is linked above. The next driver-level
diagnostic images preserve the pre-recovery state; they are archived but not
yet installed. See
`../2026-09-30_pair5_pre_recovery_tx_status_v2/README.md`.
