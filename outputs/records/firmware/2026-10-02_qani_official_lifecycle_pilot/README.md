# QANI official-lifecycle two-board pilot

Date: 2 October 2026  
Scope: PELVIS (controller) to HEAD (responder), Channel 9, before any 15-pair work.

## Purpose

The previous direct-FiRa application task did not start UWB: both boards sent IMU data, but PELVIS reported zero UWB attempts. This pilot replaces that application-created UWB task with the Qorvo SDK's own QANI FiRa task and lifecycle.

The COMP971 code still supplies the wearable identity, IMU/BLE telemetry, and forwards a completed range to the gateway. Qorvo's QANI code owns UWB initialisation, session creation, scheduling, and ranging.

## Safety controls

No board was flashed while preparing this record.

The Qorvo DWM3001CDK platform reads factory radio values from OTP at startup. OTP is read-only. However, the SDK normally has an optional persistent configuration path. This build explicitly disables that path:

| Check | Verified result |
|---|---|
| CMake setting | `CONFIG_L1_CONFIG_VOLATILE=ON` |
| Compiler definition for Qorvo L1 configuration | `CONFIG_L1_CONFIG_VOLATILE` present |
| Persistent L1 source (`l1_config_custom.c`) | excluded: 0 compiled entries |
| Behaviour of `l1_config_store_key` in this build | update RAM, then return; no persistent commit |
| OTP | read only; no OTP write command |
| Antenna delay / calibration | no value changed or stored |
| J-Link operation planned | normal application `loadfile`, reset and run only; no chip erase command and no UICR/OTP operation |

This does not make the flash process itself reversible: writing a new application replaces the old application image. Existing recorded board backups remain the recovery route. It does mean the running pilot does not repeatedly write configuration or calibration values to flash.

## Source change

| Area | Previous v4 | This QANI pilot |
|---|---|---|
| UWB task owner | COMP971-created direct task | Qorvo QANI task (`CreateQaniTask`) |
| UWB start | direct application call | Qorvo `StartUWB` signal |
| FiRa session setup | application path that did not run | Qorvo QANI scheduler/session lifecycle |
| Radio values | Qorvo factory settings | same Qorvo factory settings, RAM only |
| BLE / IMU | COMP971 | unchanged |

Relevant files:

- `hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_two_node.c`
- `hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/fira/CMakeLists.txt`
- `hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/DWM3001CDK/project_COMP971.cmake`

## Built images

The directory names are retained from the trusted build configuration; the SHA-256 value identifies the exact QANI image.

| Board | J-Link serial | Role | Image | SHA-256 | RAM | Flash |
|---|---:|---|---|---|---:|---:|
| PELVIS | 760224787 | controller / initiator | `C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_PELVIS_v3\DWM3001CDK-COMP971-FreeRTOS.hex` | `4ABF7F88EBA41F656045405AD412DEF706B6632C71D7A749F529A49654BA1AB3` | 70,224 B / 112 KiB | 342,872 B / 504 KiB |
| HEAD | 760225805 | responder | `C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_HEAD_v3\DWM3001CDK-COMP971-FreeRTOS.hex` | `66CBF2C6966BACC36D2E59A9A44E7E3E1FD24978B97DCAF4B79F4FAEC04FCF58` | 70,224 B / 112 KiB | 341,992 B / 504 KiB |

## Test sequence after explicit flash approvals

1. Flash PELVIS by J9.
2. Flash HEAD by J9.
3. Put both on battery, 100 cm apart, with the nRF52840 gateway dongle connected.
4. Run the one live viewer on COM16.
5. Success requires a non-zero PELVIS attempt count and valid pair 0 (`HEAD <-> PELVIS`) ranges. IMU data alone is not proof of UWB success.

## Status

**Superseded — do not flash this particular pilot image.** This record is for
the early prepared build only. Later QANI-derived images v5, v6 and v7 were
in fact programmed to PELVIS and HEAD; their programming and read-back records
are in `../2026-10-01_official_fira_two_node_pilot/` and
`../2026-10-02_official_qani_positive_add_controlee_fix/`.

The later source audit established that QANI is Qorvo's iPhone Nearby
Interaction example, not Qorvo's two-DWM3001CDK reference application. It
also found that the installed SDK's `fira_niq.c` has COMP971-specific changes.
It is therefore not the clean reference for the next firmware revision. The
image documented here must not be flashed again.
