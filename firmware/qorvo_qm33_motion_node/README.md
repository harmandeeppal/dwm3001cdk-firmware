# COMP971 official-Qorvo motion-node project

## Status

This folder is the clean home for the final wearable application source that
will sit **above** the official Qorvo QM33 SDK. The first custom BLE and
ICM-20948 pilot is now installed on HEAD only. It sends raw six-axis ICM data
at 60 Hz through the existing gateway; it is not the final wearable firmware
and contains no UWB measurement.

The buildable pilot source is in
[`official_sdk_project`](official_sdk_project). Its protected installation and
read-back verification record is
[`../../outputs/records/firmware/2026-10-01_head_official_icm60_pilot/README.md`](../../outputs/records/firmware/2026-10-01_head_official_icm60_pilot/README.md).

## Boundary

| Keep from official Qorvo project | Add for COMP971 |
|---|---|
| DWM3001CDK board target and UWB low-level stack | ICM-20948 acquisition at 60 Hz |
| Qorvo radio configuration and board calibration path | Gateway-compatible BLE service and packet format |
| FreeRTOS and Nordic S113 BLE foundations | Two-node test, then six-node 15-pair scheduler |
| Qorvo build/linker/SoftDevice setup | Motion data queueing, health counters and recovery logic |

The starting reference is Qorvo's `QANI/DWM3001CDK` project.  QANI's Apple
Nearby Interaction task and QNIS BLE service will **not** be carried into the
final application.  They are replaced, not run alongside the COMP971 service.

## First implementation order

1. Create a battery-safe BLE heartbeat using the official FreeRTOS/SoftDevice
   base and the existing COMP971 gateway packet framing.
2. Enable one verified I2C/TWIM instance and read the ICM-20948 at 60 Hz.
3. Send IMU packets to the gateway; prove reconnect without board restart.
4. Add an official-stack two-board UWB exchange on channel 9.
5. Add the full 15-pair TDMA schedule only after the two-board result is
   repeatable.

Each stage builds before a board is touched.  Each first flash uses a named
board, duplicate matched flash/UICR backup, and J-Link verify.

## Node build identity

The IMU/BLE source is shared by all six wearables. CMake selects identity with
two explicit cache values; it does not use a COM port or USB connection as an
identity:

| Node | `COMP971_NODE_INDEX` | `COMP971_DEVICE_NAME` |
| --- | ---: | --- |
| HEAD | 0 | `COMP971-MOTION-HEAD` |
| PELVIS | 1 | `COMP971-MOTION-PELVIS` |
| LEFT-WRIST | 2 | `COMP971-MOTION-LEFT-WRIST` |
| RIGHT-WRIST | 3 | `COMP971-MOTION-RIGHT-WRIST` |
| LEFT-ANKLE | 4 | `COMP971-MOTION-LEFT-ANKLE` |
| RIGHT-ANKLE | 5 | `COMP971-MOTION-RIGHT-ANKLE` |

The current installed HEAD build remains unchanged. These settings prepare the
same reviewed source for the next board; no other board is flashed by this
source change.

## Compatibility requirements already checked

| Requirement | Existing COMP971 gateway | Official QANI reference | Final custom action |
|---|---|---|---|
| Advertised name | `COMP971-MOTION-<position>` | DWM3001CDK/QANI name | advertise the COMP971 name so the gateway discovers the node |
| GATT service | COMP971 UUID ending `...2410` | Qorvo QNIS service | add the COMP971 service; do not reuse QNIS |
| Telemetry characteristic | notify, COMP971 UUID ending `...2410` | QNIS characteristic | add a compatible notification characteristic |
| Gateway liveness | one-byte heartbeat write | NI control messages | add the existing one-byte heartbeat contract |
| Full IMU batch | 206-byte payload; ATT MTU at least 209 | max MTU 100; data length 27 | increase Nordic BLE settings before 9-sample batches |

The first BLE *heartbeat* build will use only short status packets. The
larger MTU/data-length change will be included and verified before the 60 Hz
IMU stream is enabled. This avoids a connection that appears healthy while
silently dropping large telemetry packets.

The preserved portable packet definitions are in
[`include/comp971_telemetry_protocol.h`](include/comp971_telemetry_protocol.h).
The exact first-build test is
[`docs/FIRST_BUILD_ACCEPTANCE.md`](docs/FIRST_BUILD_ACCEPTANCE.md).
The later 15-pair implementation and timing rule are in
[`docs/UWB_15_PAIR_MESH_DESIGN.md`](docs/UWB_15_PAIR_MESH_DESIGN.md).

## Important current fact

The reference QANI configuration has `NRFX_TWIM_ENABLED`, `NRFX_TWIM0_ENABLED`
and `NRFX_TWIM1_ENABLED` set to `0`.  This is normal because QANI has no
external IMU.  Enabling I2C is an intentional first custom change, after pin
and peripheral-conflict review; it is not a missing radio calibration setting.
