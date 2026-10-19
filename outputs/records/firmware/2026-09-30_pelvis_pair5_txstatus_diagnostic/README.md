# PELVIS pair-5 transmit-status diagnostic build

## Purpose

This temporary test image is for the PELVIS–LEFT-WRIST UWB pair only. When a
delayed radio transmit fails to start, it saves the DW3110 status register in
the existing UWB diagnostic event. This should help distinguish a transmit
timing/status problem from a BLE delivery problem.

It does **not** change the UWB schedule or its 2 ms response delay. It is not
the final six-node firmware and must not be used for normal mesh operation.

## Build

- Target: `nrf52833dk/nrf52833` (DWM3001CDK nRF52833)
- Board identity: PELVIS, mesh node index 1
- Pair filter: pair index 5 (PELVIS–LEFT-WRIST)
- BLE config: `ble_pelvis.conf`
- Overlay: `nrf52833_dk.overlay`
- Diagnostic option: `COMP971_UWB_TX_STATUS_DIAGNOSTICS=ON`
- SDK: nRF Connect SDK 3.4.0 / Zephyr 4.4.0
- Build result: successful; 144,764 bytes reported FLASH use
- Source: `hardware/firmware/archive/dwm3001cdk_legacy_proto/`

## Image

| File | Size | SHA-256 |
|---|---:|---|
| `DWM-02-PELVIS__pair5-txstatus-diagnostic__nrf52833.hex` | 407,313 bytes | `61C031F92A5F4CCE835642DE80177F70322C0CC0500EEDBA514BB767D9B83E0A` |

## How to read the result

For the existing diagnostic event codes for failed delayed FINAL/RESPONSE
transmits, the `detail` field will contain the raw low 32 bits of DW3110
`SYS_STATUS` when `dwt_starttx()` reports failure. The event code still tells
which step failed. A status value is evidence to analyze, not by itself a
proof of the underlying cause. The firmware reports actual range results over
BLE as before.

## Safety and installation state

- The image has been built and archived. A fresh read-only backup of PELVIS
  `760224787` code flash and UICR is saved in
  `hardware/backups/DWM3001CDK_760224787/2026-09-30_pre_txstatus_diagnostic_backup/`.
  Both pairs of reads match, and all 144,744 programmed bytes matched the
  expected pair-only image before the update.
- On 2026-09-30, this image was programmed to PELVIS `760224787` with
  `nrfjprog --program --sectorerase --verify --reset --family NRF52 --snr
  760224787`. The tool reported write successful, verify successful, and
  reset/run. Command exit code: 0. SEGGER emitted its known JLinkARM.dll
  error -256 warnings, but the write and verify stages completed successfully.
- Only install it on PELVIS. LEFT-WRIST was not changed and remains on its
  previous shared transport/failure-stage diagnostic firmware.
- After the controlled capture, restore the normal PELVIS firmware before
  returning to the six-node setup.

The exact read commands are in the adjacent backup folder's
`read_pelvis_before_txstatus_diagnostic.jlink`. A controlled live capture is
recorded at
`hardware/outputs/records/calibration/2026-09-30_pair5_txstatus_diagnostic_50cm/`.
It contained 148 PELVIS final-TX failure events with raw status detail `0`.
The 1,036 more frequent LEFT-WRIST reply-TX failures still had detail `-1`,
because LEFT-WRIST did not yet have this diagnostic option at the time of that
first capture. LEFT-WRIST was subsequently backed up and received the matching
v1 diagnostic image.

That repeat is now captured at
`hardware/outputs/records/calibration/2026-09-30_pair5_txstatus_both_50cm/`.
Both PELVIS and LEFT-WRIST delayed-TX failure events reported raw
`SYS_STATUS` detail `0`. This is not enough to identify the error because the
application reads the register after the Qorvo driver has run its TX-off
recovery. The driver-level v2 images were then built to preserve the
pre-recovery values. PELVIS v2 was installed first, followed by LEFT-WRIST v2
after its fresh backup and old-image verification. See
`hardware/outputs/records/firmware/2026-09-30_pair5_pre_recovery_tx_status_v2/`.

The matching v2 images have since been installed on PELVIS `760224787` and
LEFT-WRIST `760225868`, after duplicate-matched flash/UICR backups and old-
image verification. Both writes and verifies succeeded. See the [v2 install
record](../2026-09-30_pair5_pre_recovery_tx_status_v2/README.md) and the
[PELVIS backup](../../../../backups/DWM3001CDK_760224787/2026-09-30_pre_txstatus_v2_backup/README.md)
and [LEFT-WRIST backup](../../../../backups/DWM3001CDK_760225868/2026-09-30_pre_txstatus_v2_backup/README.md).
