# Pair-5 pre-recovery UWB transmit-status diagnostic — v2

## Purpose

The previous pair-only images read `SYS_STATUS` after the Qorvo driver had
already issued its transmit-recovery command. That read returned `0`, which
did not explain the failures. These v2 bench images save the UWB radio state
inside the driver, before recovery:

- `SYS_STATUS[31:24]` is stored in the top byte of the existing diagnostic
  `detail` field. This includes the delayed-transmit warning flag.
- `SYS_STATE_LO[23:0]` is stored in the lower three bytes.

The two values are packed into one 32-bit bit pattern. Treat `detail` as
unsigned when splitting it, even if the decoded JSON shows a negative number.
The firmware does not change ranging timing, the 2 ms response delay, or the
BLE transport. It is a temporary pair-5 diagnostic, not the final six-node
firmware.

## Build

| Setting | PELVIS | LEFT-WRIST |
|---|---|---|
| Registered J-Link serial | `760224787` | `760225868` |
| Mesh node index | 1 | 2 |
| BLE configuration | `ble_pelvis.conf` | `ble_left_wrist.conf` |
| Pair-only index | 5 | 5 |
| Target | `nrf52833dk/nrf52833` | `nrf52833dk/nrf52833` |
| SDK / Zephyr | nRF Connect SDK 3.4.0 / Zephyr 4.4.0 | same |
| FLASH / RAM used | 144,892 / 67,844 bytes | 147,876 / 68,164 bytes |

Both clean builds completed successfully. Compile commands were checked for
the matching node index, `COMP971_UWB_PAIR_ONLY_INDEX=5`, and
`COMP971_UWB_TX_STATUS_DIAGNOSTICS=1`.

The build used the same command for each board, changing its identity config,
node index, and output directory:

```powershell
west build --no-sysbuild -p always -b nrf52833dk/nrf52833 `
  -d <separate-build-directory> `
  hardware/firmware/archive/dwm3001cdk_legacy_proto/zephyr -- `
  -DEXTRA_CONF_FILE=ble_<pelvis-or-left_wrist>.conf `
  -DDTC_OVERLAY_FILE=nrf52833_dk.overlay `
  -DCMAKE_C_FLAGS=-DUWB_MESH_NODE_INDEX=<1-or-2> `
  -DCOMP971_UWB_PAIR_ONLY_INDEX=5 `
  -DCOMP971_UWB_TX_STATUS_DIAGNOSTICS=ON
```

## Archived images

| Image | Size | SHA-256 |
|---|---:|---|
| `DWM-02-PELVIS__pair5-pre-recovery-status-v2__nrf52833.hex` | 407,673 bytes | `3377C2F78BDFECA51891C82437C45EAB08D04B37E6AA169DB0981E2E79A3E972` |
| `DWM-03-LEFT-WRIST__pair5-pre-recovery-status-v2__nrf52833.hex` | 416,088 bytes | `6F328205F80CA1B27F3E4335E2B566D311B51BBCCF1AC4D5CEB6597E6F693808` |

## Install state and safe next step

The PELVIS v2 image is now installed on serial `760224787`. Before writing,
its full 512 KiB flash and UICR were each read twice; both duplicate reads
matched. The prior v1 image verified before the update. The v2 write,
verification, and reset/run completed successfully (nrfjprog exit code 0).
See the [pre-update PELVIS backup](../../../../backups/DWM3001CDK_760224787/2026-09-30_pre_txstatus_v2_backup/README.md).

LEFT-WRIST v2 is now installed on serial `760225868`. Before writing, its
complete 512 KiB flash and 4 KiB UICR were each read twice; both duplicate
pairs matched. The archived v1 image verified before the update. The v2 image
hash matched the archived SHA-256, and erase, write, verify, and reset/run
completed successfully (nrfjprog exit code 0). A separate verify after reset
also passed. See the [LEFT-WRIST backup](../../../../backups/DWM3001CDK_760225868/2026-09-30_pre_txstatus_v2_backup/README.md).
Both PELVIS and LEFT-WRIST now have the matching v2 bench diagnostic. The
other four wearables were not changed.

The LEFT-WRIST programming command was:

```powershell
nrfjprog --program hardware/outputs/records/firmware/2026-09-30_pair5_pre_recovery_tx_status_v2/DWM-03-LEFT-WRIST__pair5-pre-recovery-status-v2__nrf52833.hex --sectorerase --verify --reset --family NRF52 --snr 760225868
```

The first 60-second v2 capture is recorded at
`hardware/outputs/records/calibration/2026-09-30_pair5_pre_recovery_v2_50cm/`.
It contained only PELVIS telemetry: all 2,524 measured pair attempts were
invalid, and PELVIS logged 2,558 response-wait failures with `RXFTO` set.
LEFT-WRIST produced no BLE IMU or node-status records, so this run did not
capture a LEFT-WRIST delayed-transmit failure status. First diagnose why
LEFT-WRIST was absent. Once it reports again, repeat the 50 cm capture and
inspect delayed-TX status. Do not adjust the 2 ms delay based on this capture.

### Repeat after restarting LEFT-WRIST

The follow-up 60-second capture is recorded at
`../../calibration/2026-09-30_pair5_pre_recovery_v2_50cm_after_wrist_restart/README.md`.
After the user restarted LEFT-WRIST, its IMU stream returned at about 60 Hz.
For pair 5 at the user-reported 50 cm separation, PELVIS counted 3,317 attempts
over 58.1 seconds: 2,122 valid and 1,195 invalid (36.0% invalid). The 115
gateway range reports were all marked valid by firmware, but their median was
15 cm and sample SD was 22.49 cm, so they are not calibration-quality results.

The v2 instrumentation captured the missing responder failure status:
LEFT-WRIST logged 857 delayed response-TX failures (667 with `HPDWARN`, 190
with `TXERR`); PELVIS logged 188 failed final-TX events (167 with `HPDWARN`,
21 with `TXERR`). PELVIS also logged 1,038 response waits, 1,037 with `RXFTO`.
This strongly points to delayed-TX timing margin in both stages, but does not
yet explain inaccurate ranges among successful reports. The next test should
increase the pair-only delayed-TX margins and repeat at the same distance;
keep the six-node firmware unchanged until that test is evaluated. See Qorvo's
[DW3xxx API Guide](https://forum.qorvo.com/uploads/short-url/xD3TlXKvkujjdUaJWXv2b4E7GQN.pdf)
for the meaning of `HPDWARN`.

Split the next decoded `detail` value as follows:

```text
status_high_byte = (uint32(detail) >> 24) & 0xFF
state_low_24     = uint32(detail) & 0xFFFFFF
```

## Source and build folders

- Source: `hardware/firmware/archive/dwm3001cdk_legacy_proto/`
- PELVIS build: `C:/ncs/v3.4.0/comp971_workspace_20260930/tmp/comp971_pair5_txstatus_v2_pelvis_build/`
- LEFT-WRIST build: `C:/ncs/v3.4.0/comp971_workspace_20260930/tmp/comp971_pair5_txstatus_v2_left_wrist_build/`
- Earlier v1 images: `2026-09-30_pelvis_pair5_txstatus_diagnostic/` and
  `2026-09-30_left_wrist_pair5_txstatus_diagnostic/`
- Latest v1 test capture: `../../calibration/2026-09-30_pair5_txstatus_both_50cm/README.md`
