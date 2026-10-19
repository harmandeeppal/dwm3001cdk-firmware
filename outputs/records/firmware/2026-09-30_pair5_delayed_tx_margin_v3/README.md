# Pair-5 delayed-transmit margin test — v3 build

## Purpose and scope

The 30 September pair-only diagnostic capture at a user-reported 50 cm found
`HPDWARN` on delayed transmissions from both PELVIS and LEFT-WRIST. This test
build gives both scheduled sends more preparation time:

| Setting | Current v2 pair test | v3 test build |
|---|---:|---:|
| LEFT-WRIST response after POLL | 2,000 UUS (about 2 ms) | 5,000 UUS (about 5 ms) |
| PELVIS FINAL after RESPONSE | 3,000 UUS (about 3 ms) | 6,000 UUS (about 6 ms) |

Everything else remains the same: pair index 5 only (PELVIS–LEFT-WRIST),
pre-recovery UWB status capture enabled, ICM/BLE settings unchanged. These are
temporary diagnostic images, not fleet firmware or a calibration release.
Because both delayed-send stages showed `HPDWARN`, this test increases both
margins together. A better error rate would support the timing hypothesis, but
would not by itself prove the cause of every inaccurate range.

## Build identity

| Image | Board role | Registered J-Link serial | Node index | Config | FLASH / RAM used |
|---|---|---:|---:|---|---:|
| `DWM-02-PELVIS__pair5-delayed-tx-margin-v3__nrf52833.hex` | Coordinator | `760224787` | 1 | `ble_pelvis.conf` | 144,892 / 67,844 bytes |
| `DWM-03-LEFT-WRIST__pair5-delayed-tx-margin-v3__nrf52833.hex` | Pair responder | `760225868` | 2 | `ble_left_wrist.conf` | 147,892 / 68,164 bytes |

Both clean builds used nRF Connect SDK 3.4.0, Zephyr 4.4.0, target
`nrf52833dk/nrf52833`, pair-only index 5 and status diagnostics enabled. The
compile commands were checked and contain the expected node identity and both
timing overrides.

The timing override definitions were:

```text
-DUWB_RESP_DELAY_UUS=5000U
-DMESH_FINAL_DELAY_UUS=6000U
```

The ordinary firmware defaults remain unchanged at 2,000 and 3,000 UUS.

The compiled commands did not define `UWB_TX_ANT_DLY` or `UWB_RX_ANT_DLY`.
Therefore these images use the `main.c` fallback value `16390` for both
antenna-delay settings on both boards. They do not load per-device antenna
delay values from the registry or read them from OTP before setting the
transceiver registers. This is now a calibration item: the Qorvo DWM3001CDK
guide describes known-distance antenna-delay calibration and gives a `16390`
default for named E1.0 engineering samples, while a Qorvo support response
states that DWM3001CDK calibrated values can be stored in OTP. Check each
board's revision and OTP data before changing these settings; do not assume
the old PELVIS–LEFT-ANKLE value applies to LEFT-WRIST.

## Archived images

| Image | Size | SHA-256 |
|---|---:|---|
| `DWM-02-PELVIS__pair5-delayed-tx-margin-v3__nrf52833.hex` | 407,673 bytes | `E45B78B3E18DF8CBC67AD916CAA74DB7F2DE2474244C5BE953647BF128856BD7` |
| `DWM-03-LEFT-WRIST__pair5-delayed-tx-margin-v3__nrf52833.hex` | 416,117 bytes | `A5D52B7D1F84309EF8B722A6AA3A6B1E2AEA83CA6A86A1CA5EAA010520E57670` |

The build completed successfully. Zephyr emitted the existing deprecated-
boilerplate and compiler warnings (including unused code in pair-only mode);
there were no build errors. The images are archived here and in their separate
build folders under `C:/ncs/v3.4.0/comp971_workspace_20260930/tmp/`.

## Install state and safe next step

**Both v3 images are installed and SEGGER-reported as verified.**
PELVIS was selected by registered J-Link serial `760224787`. Before writing,
SEGGER J-Link read its 512 KiB flash and UICR twice; each pair of reads matched
byte-for-byte. FICR identity was also saved. The backup is in
`hardware/backups/DWM3001CDK_760224787/2026-09-30_pre_pair5_delayed_tx_margin_v3_backup/`.
SEGGER then programmed the PELVIS v3 HEX and reported `Program & Verify` and
`O.K.` for the changed 4 KiB flash range. Image SHA-256:
`E45B78B3E18DF8CBC67AD916CAA74DB7F2DE2474244C5BE953647BF128856BD7`.
The programming script is `program_pelvis_pair5_delayed_tx_margin_v3.jlink`.

LEFT-WRIST was then selected by registered serial `760225868`. Its 512 KiB
flash and UICR were also read twice and matched byte-for-byte before writing.
That backup is in
`hardware/backups/DWM3001CDK_760225868/2026-09-30_pre_pair5_delayed_tx_margin_v3_backup/`.
SEGGER reported `Program & Verify` and `O.K.` for three flash ranges (135,168
bytes). LEFT-WRIST image SHA-256:
`A5D52B7D1F84309EF8B722A6AA3A6B1E2AEA83CA6A86A1CA5EAA010520E57670`.
The programming script is `program_left_wrist_pair5_delayed_tx_margin_v3.jlink`.
No gateway, dongle, or other wearable was changed.

Both nodes were reset and started by SEGGER after programming. The first
nominal 50 cm v3 capture is now recorded in
`../calibration/2026-09-30_pair5_delayed_tx_margin_v3_50cm/README.md`.
PELVIS counted 19 invalid attempts out of 2,754 (0.69%), compared with 36.0%
in the preceding v2 capture. However, the 118 ranges delivered to the laptop
had a median of 8 cm, with values from 1 to 98 cm. Thus the timing change
substantially improved the attempt-validity count in this run but did not fix
distance accuracy. Do not calibrate from these results or install these
temporary pair-only images on the other four boards. Next inspect the DS-TWR
timestamp calculation and antenna-delay settings before choosing another
focused test. The calculation review found no obvious missing timestamp; the
compile commands show both boards used fixed fallback antenna delays of
`16390`. The next check is to read the pair's stored antenna-delay calibration
data without writing to OTP. See the detailed [v3 capture analysis](../calibration/2026-09-30_pair5_delayed_tx_margin_v3_50cm/README.md).

## Source and build folders

- Source: `hardware/firmware/archive/dwm3001cdk_legacy_proto/`
- PELVIS clean build: `C:/ncs/v3.4.0/comp971_workspace_20260930/tmp/comp971_pair5_txstatus_v3_pelvis_build/`
- LEFT-WRIST clean build: `C:/ncs/v3.4.0/comp971_workspace_20260930/tmp/comp971_pair5_txstatus_v3_left_wrist_build/`
- Previous v2 build/install record: `../2026-09-30_pair5_pre_recovery_tx_status_v2/README.md`
- Previous v2 post-restart capture: `../../calibration/2026-09-30_pair5_pre_recovery_v2_50cm_after_wrist_restart/README.md`
- Qorvo reference for `HPDWARN`: [DW3xxx API Guide](https://forum.qorvo.com/uploads/short-url/xD3TlXKvkujjdUaJWXv2b4E7GQN.pdf)
