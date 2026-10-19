# HEAD official-Qorvo BLE pilot

## Purpose

This is the first custom COMP971 application built above the official Qorvo SDK
for the DWM3001CDK. It tests battery-capable Bluetooth connection to the
existing nRF52840 gateway before IMU or UWB work is added.

## Included

- Official Qorvo DWM3001CDK board and radio support.
- Official FreeRTOS and S113 SoftDevice build composition.
- Bluetooth advertisement name: `COMP971-MOTION-HEAD`.
- Existing COMP971 Bluetooth service UUIDs and telemetry characteristics.
- ATT MTU request of 247 bytes; the gateway requires at least 209 bytes.
- One status notification each second once the gateway subscribes.
- Advertising restarts after a gateway disconnect.

## Deliberately excluded

- ICM-20948 sampling.
- UWB ranging or the 15-pair mesh.
- Antenna-delay or any other calibration write.
- OTP, UICR, chip erase, and runtime flash storage writes.
- Any nRF52840 dongle firmware change.

## Build evidence

Source project: `hardware/firmware/qorvo_qm33_motion_node/official_sdk_project`

Target: `DWM3001CDK-COMP971-FreeRTOS`

| Item | Value |
| --- | --- |
| Flash use | 162,848 / 516,096 bytes (31.55%) |
| RAM use | 54,236 / 114,688 bytes (47.29%) |
| HEX SHA-256 | `5D4FEE0FCF0115D7EC54F9AEC0EA0A624331A558BDF1EC45657BDBEBD8E6493F` |
| BIN SHA-256 | `8FE97D07A7990F3884839EC6BBA05B2B28729440578F5A70F6CF4EC32318CE44` |
| ELF SHA-256 | `F5788DB4AE4534959BD249EE15AE1D70DB82A3E182D13837A33463F267C57BCE` |

## Discovery correction, build 2

The first live test showed no gateway packets. Source review identified the
cause: the gateway performs passive Bluetooth scanning, which cannot read a
device name placed only in the scan-response packet. Build 2 moves
`COMP971-MOTION-HEAD` into the main advertisement packet. The gateway uses that
name to decide to connect, and then discovers the same COMP971 GATT service as
before.

This changes no UWB, IMU, calibration, UICR, OTP, runtime flash-storage, or
dongle behaviour.

| Item | Build 2 value |
| --- | --- |
| HEX size | 451,253 bytes |
| BIN size | 163,304 bytes |
| HEX SHA-256 | `F39C800A17E3743A29FAF2736C4E36773E0A54A0753063F082D040C54C330EC0` |
| BIN SHA-256 | `B7BD02A564D7B18E6BB69AC92F81FACF1418693E7417BB478EAB78207E51E755` |

## Protected install target

- Board: `DWM-01-HEAD`.
- J-Link serial: `760225805`.
- Matching pre-install backup:
  `hardware/backups/DWM3001CDK_760225805/2026-10-01_pre_comp971_official_port_pilot`.
- The script programs the image, verifies the produced binary at flash address
  `0x00000000`, then resets and runs the board. It does not issue a chip erase
  command and does not write UICR, OTP, or calibration storage.

## Required acceptance check

1. Plug in the existing nRF52840 gateway dongle.
2. Confirm it receives a HEAD status message approximately once per second.
3. Unplug and replug only the dongle; HEAD should advertise and reconnect without
   a HEAD restart.

## Installation result

Installed on 2026-10-01 using J-Link serial `760225805` only. J-Link's normal
program-and-verify step completed successfully. A separate read-only post-flash
check then compared every defined byte in the HEX file against the HEAD flash:

| Check | Result |
| --- | --- |
| Defined HEX bytes checked | 160,436 |
| Mismatches | 0 |
| Reset and run after check | Completed |

The raw `verifybin` check is not used as install evidence because a BIN file
does not preserve the address gaps represented by an Intel-HEX image. The
range-aware HEX read-back check is the final verification method.

## Build 2 installation and initial gateway result

Build 2 was programmed on the same registered HEAD board only, using the same
protected pre-pilot backup. The normal J-Link program-and-verify stage completed
and the read-only range-aware check found zero mismatches across 160,404 defined
HEX bytes.

With the existing nRF52840 dongle on `COM16`, the gateway then discovered HEAD
and received valid HEAD status records. A 12.022-second viewer session reported
40 gateway packets, zero malformed packets, zero USB drops and zero IMU sequence
gaps. IMU and UWB rows were empty by design: this is a BLE-only pilot.

The saved gateway session is
`hardware/outputs/records/acquisition/live_gateway_viewer/session_20261001_074820`.
The remaining check is to unplug and replug the dongle without restarting HEAD,
then confirm it reconnects.

## Dongle-only reconnect result

Passed on 2026-10-01. The nRF52840 dongle was unplugged and replugged while
HEAD remained powered and untouched. The new gateway session received 23 valid
packets in 12 seconds, with zero malformed packets, zero USB drops and zero IMU
sequence gaps. HEAD health status was present again. This completes the
BLE-only pilot acceptance; it does not validate IMU or UWB.

Reconnect session:
`hardware/outputs/records/acquisition/live_gateway_viewer/session_20261001_080617`.
