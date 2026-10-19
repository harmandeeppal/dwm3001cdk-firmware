# UWB failure-diagnostics source build — 30 September 2026

## Result

The changed diagnostic code compiled for all six wearable identities and the
nRF52840 gateway. The decoder passed a synthetic framed diagnostic-record
test. All six wearable images and the gateway image have now been installed;
the wearable writes were flash-verified, and the gateway returned from DFU to
normal gateway mode. A first live capture has decoded diagnostic events from
the wearables; the gateway receive timestamps are zero and need investigation.

## Toolchain and configuration

- Nordic Connect SDK 3.4.0 / Zephyr 4.4.0
- PELVIS target: `nrf52833dk/nrf52833`, `UWB_MESH_NODE_INDEX=1`,
  `ble_pelvis.conf`, `nrf52833_dk.overlay`
- Gateway target: `nrf52840dongle/nrf52840`, gateway `prj.conf` and
  `app.overlay`
- Both builds used temporary output folders under the workspace `tmp/`.

## Firmware images

| Image | Size (bytes) | SHA-256 |
|---|---:|---|
| `DWM-01-HEAD__uwb-failure-diagnostics__nrf52833.hex` | 415,651 | `73CC41CBC52CE37B4F6779FFCCA34E9D8A6BE6C65DA64E628AD276C32F1A2B38` |
| `DWM-02-PELVIS__uwb-failure-diagnostics__nrf52833.hex` | 409,375 | `3B25298AD046951664757535F83D713A23FE335C121E2B69D8B2CAA061D14D49` |
| `DWM-03-LEFT-WRIST__uwb-failure-diagnostics__nrf52833.hex` | 415,712 | `ACFC64BE7DE0A40264F6DB3866B6E73779552A96DBDD7322103FAF42B577863D` |
| `DWM-04-RIGHT-WRIST__uwb-failure-diagnostics__nrf52833.hex` | 415,728 | `A8B48C4B4D0DEF1122DAF2EBDAC6A57A25354C9C415F7011946116C31F21B188` |
| `DWM-05-LEFT-ANKLE__uwb-failure-diagnostics__nrf52833.hex` | 415,712 | `46F824C28B79F41E7425D0FD3B2596A9AD0BD21531C48F4C7A8D31828E391571` |
| `DWM-06-RIGHT-ANKLE__uwb-failure-diagnostics__nrf52833.hex` | 415,704 | `799066C730FBD130981EF76A5A2DE35098B8AE228C659AB35FEC54ECA8B0B548` |
| `nRF52840-DONGLE__uwb-failure-diagnostics__gateway.hex` | 408,290 | `B00B4AB9525DE623B36BC1FD653362FFE73E0DB4645155EB5414CA9A23BDEE58` |

The six images use the same source and differ only in their configured
Bluetooth identity and mesh index (HEAD 0, PELVIS 1, LEFT-WRIST 2,
RIGHT-WRIST 3, LEFT-ANKLE 4, RIGHT-ANKLE 5). The images are archived here for
review.

## Checks performed

- All six identity-specific wearable firmware builds completed successfully.
- Gateway firmware build completed successfully (all 237 build steps).
- The decoder accepted a synthetic type-5 diagnostic frame and decoded node,
  pair, timeout stage, detail value, and gateway timestamp correctly.
- The diagnostic record is 16 bytes over BLE and 24 bytes in the gateway USB
  record after adding the gateway timestamp.

## Not yet checked

- Diagnostic loss under BLE congestion.
- Sustained diagnostic-event delivery and counts under a controlled failure
  test or BLE congestion.
- Whether a reported failure stage's root cause is RF interference, timing,
  packet loss, or a responder fault.

## Installation record

| Board | Registered serial | Result | Notes |
|---|---:|---|---|
| PELVIS | `760224787` | Programmed, verified, and reset successfully on 2026-09-30 | Existing full 512 KB flash backup is at `hardware/backups/DWM3001CDK_760224787/2026-08-30_preflash_backup/`. Programmed with `nrfjprog --program ... --sectorerase --verify --reset --family NRF52 --snr 760224787`. |
| HEAD | `760225805` | Programmed, verified, and reset successfully on 2026-09-30 | A fresh full 512 KB flash, UICR, and FICR readback is at `hardware/backups/DWM3001CDK_760225805/2026-09-30_pre_uwb_failure_diagnostics_backup/`. Backup was read using SEGGER J-Link Commander after `nrfjprog --readcode` stalled. |
| LEFT-WRIST | `760225868` | Programmed, verified, and reset successfully on 2026-09-30 | Existing flash and UICR backup is at `hardware/backups/DWM3001CDK_760225868/2026-09-27_preflash_backup/`. |
| RIGHT-WRIST | `760225748` | Programmed, verified, and reset successfully on 2026-09-30 | Existing flash and UICR backup is at `hardware/backups/DWM3001CDK_760225748/2026-09-27_preflash_backup/`. |
| LEFT-ANKLE | `760225018` | Programmed, verified, and reset successfully on 2026-09-30 | Existing full flash and UICR backup is at `hardware/backups/DWM3001CDK_760225018/2026-08-30_preflash_backup/`. |
| RIGHT-ANKLE | `760225934` | Programmed, verified, and reset successfully on 2026-09-30 | Existing flash and UICR backup is at `hardware/backups/DWM3001CDK_760225934/2026-09-27_preflash_backup/`. |
| nRF52840 Dongle gateway | DFU device `FA06ED375A53` | Programmed successfully on 2026-09-30; returned to normal gateway mode | Nordic USB DFU on COM14 reported `Device programmed`. `nrfutil device list` then showed `COMP971 Motion BLE Gateway` on COM15/COM16. |

Images written:

- PELVIS: `DWM-02-PELVIS__uwb-failure-diagnostics__nrf52833.hex`, SHA-256
  `3B25298AD046951664757535F83D713A23FE335C121E2B69D8B2CAA061D14D49`.
- HEAD: `DWM-01-HEAD__uwb-failure-diagnostics__nrf52833.hex`, SHA-256
  `73CC41CBC52CE37B4F6779FFCCA34E9D8A6BE6C65DA64E628AD276C32F1A2B38`.
- LEFT-WRIST: `DWM-03-LEFT-WRIST__uwb-failure-diagnostics__nrf52833.hex`,
  SHA-256 `ACFC64BE7DE0A40264F6DB3866B6E73779552A96DBDD7322103FAF42B577863D`.
- RIGHT-WRIST: `DWM-04-RIGHT-WRIST__uwb-failure-diagnostics__nrf52833.hex`,
  SHA-256 `A8B48C4B4D0DEF1122DAF2EBDAC6A57A25354C9C415F7011946116C31F21B188`.
- LEFT-ANKLE: `DWM-05-LEFT-ANKLE__uwb-failure-diagnostics__nrf52833.hex`,
  SHA-256 `46F824C28B79F41E7425D0FD3B2596A9AD0BD21531C48F4C7A8D31828E391571`.
- RIGHT-ANKLE: `DWM-06-RIGHT-ANKLE__uwb-failure-diagnostics__nrf52833.hex`,
  SHA-256 `799066C730FBD130981EF76A5A2DE35098B8AE228C659AB35FEC54ECA8B0B548`.

For all six boards, the tool reported `Programming image - Write successful`,
`Verify successful`, and `Applying system reset`. It also printed SEGGER
`JLinkARM.dll` warnings, but each operation returned exit code 0 and completed
its write, verify, and reset stages successfully. Live diagnostic delivery is
not yet confirmed.

Gateway DFU package: `nRF52840-DONGLE__uwb-failure-diagnostics__gateway_dfu_v6.zip`.
SHA-256: `D33A1793A95FA8ABE8768F0D7C7AF3C18FECE45EB029AB1E140B1AC5DD112C57`.
It is application version 6, hardware version 52, with no SoftDevice requirement
and CRC boot validation. The package is unsigned, matching the existing
signature-less PCA10059 DFU setup; this is acceptable for the current bench
prototype but is not a production-secure update method.

## First live diagnostic capture — 30 September 2026

Capture and decoded files are retained at:

- `hardware/outputs/records/acquisition/gateway_raw_2026-09-30_115726.bin`
- `hardware/outputs/records/acquisition/gateway_raw_2026-09-30_115726_decoded/`

The 30-second capture from COM16 contained 924 decoded UWB failure-stage
events, 441 UWB range reports, 10,332 IMU records from all six node indices,
162 node-status records, and zero invalid frames. This confirms that the new
failure events passed over BLE to the gateway and then over USB to the laptop.
The six IMU counts were 1,620 / 1,791 / 1,791 / 1,548 / 1,791 / 1,791 for
HEAD, PELVIS, LEFT-WRIST, RIGHT-WRIST, LEFT-ANKLE, and RIGHT-ANKLE. The
RIGHT-WRIST sequence counter showed 261 gaps in this capture; the other nodes
showed 8, 18, 10, 10, and 11 gaps respectively.

Reported failure-stage counts were 304 `responder_response_tx_failed`, 300
`response_wait_failed`, 258 `coordinator_result_timeout`, 31
`final_tx_failed`, 30 `responder_final_not_received`, and 1
`responder_poll_not_received`. These identify where UWB attempts failed, but
do not by themselves prove the physical cause (for example, interference,
antenna orientation, or timing).

Important limitation: all 924 diagnostic records have
`gateway_receive_timestamp_us = 0`. IMU and UWB records in this and the
previous gateway capture also have zero gateway timestamps. Root cause was
identified: the gateway called Zephyr `k_cycle_get_64()`, but the built
`.config` did not enable `CONFIG_TIMER_HAS_64BIT_CYCLE_COUNTER`. Zephyr
documents that this API asserts and returns zero when that option is disabled
([system clock API documentation](https://docs.zephyrproject.org/latest/doxygen/html/group__clock__apis.html)).

### Gateway timestamp correction — v7 installed and checked

Only the nRF52840 gateway source was changed. Its receive timestamp now uses
`k_ticks_to_us_floor64(k_uptime_ticks())`, which converts Zephyr's 64-bit
uptime tick count to microseconds. This avoids relying on the disabled
64-bit hardware-cycle-counter option. The build uses 32,768 uptime ticks per
second (about 30.5 microseconds per tick).

- Wearable firmware: unchanged; do not reflash the six DWM3001CDKs for this fix.
- Build: successful with the nRF52840 dongle target; 145,172 bytes of flash
  used (13.9% of the configured 1,020 KB region).
- HEX: `nRF52840-DONGLE__uwb-failure-diagnostics__gateway_timestamp_v7.hex`
  (SHA-256 `43D61B95AD1006789121DA0B8C4F2B388B1D72691BB6CA5F84682D70C8C4A7DA`).
- DFU package: `nRF52840-DONGLE__uwb-failure-diagnostics__gateway_dfu_v7.zip`
  (SHA-256 `1A9412FC7B87A992A5AF838C4F7C928BDFE53C0DE3102FAC90E86471C4C8943A`).
- Package settings: application version 7, hardware version 52, no SoftDevice,
  CRC boot validation. It is unsigned, matching the existing bench DFU setup;
  it is not suitable for production.
- Installation: `nrfutil nrf5sdk-tools dfu usb-serial` reported
  `Device programmed.` on DFU COM14. The dongle returned to normal gateway
  mode on COM15/COM16.
- Verification capture: `gateway_raw_2026-09-30_122503.bin` (15 seconds),
  decoded to `gateway_raw_2026-09-30_122503_decoded/`. It contains 5,345 IMU,
  226 UWB, and 511 UWB-diagnostic records, with zero invalid frames.
- All decoded IMU, UWB, and UWB-diagnostic records have a non-zero
  `gateway_receive_timestamp_us`. No timestamp moved backwards within any
  stream. Several consecutive IMU samples share a timestamp because they were
  delivered in the same received batch; this is expected. UWB and diagnostic
  records have increasing receive timestamps in this capture.
- The six IMU streams have a median per-sample interval of about 16,663 us
  (about 60 Hz), using each sample's own `local_timestamp_us`. The wearable
  sends samples in BLE batches (up to nine samples per notification); the
  gateway takes one receive timestamp per BLE notification and copies it to
  each sample in that batch. So the repeated gateway timestamp is a batch
  arrival time, not the time each sensor sample was taken.
- This capture also had 226 UWB reports: 98 valid and 128 invalid. Nine of the
  15 pairs had no valid range in this 15-second window. The physical distances
  were not recorded, so this is a diagnostic snapshot only—not a distance
  calibration result.
- This verifies that the gateway receive-time field now works. It does not
  synchronize the six independent wearable clocks, fix missing UWB radio
  exchanges, or calibrate range accuracy. Continue to use node timestamps for
  sensor sampling times until a clock-synchronization design is implemented.
