# PELVIS official-Qorvo ICM-20948 60 Hz pilot

## Purpose

Install the tested shared BLE and raw ICM-20948 foundation on PELVIS, with
node identity `COMP971-MOTION-PELVIS` and node index 1. This is the foundation
for PELVIS later becoming the 15-pair UWB schedule coordinator and BLE gateway.

## Safety boundary

| Item | Value |
| --- | --- |
| Board | DWM-02-PELVIS only |
| J-Link serial | 760224787 |
| Current-state recovery backup | `hardware/backups/DWM3001CDK_760224787/2026-10-01_pre_comp971_icm_ble_pilot` |
| Image SHA-256 | `FE7D1BA3E312D4A85E7BFE9CCBBB6C699FBE4AB2D85AA1BA38C10846A5708581` |
| Not performed | Chip erase, UICR/OTP/calibration write, runtime flash storage write, dongle flash |

## Included

- Official Qorvo DWM3001CDK QM33 SDK foundation.
- Bluetooth discovery/reconnect using the existing gateway protocol.
- External ICM-20948: raw accelerometer, gyroscope and temperature at 60 Hz.
- PELVIS identity only: index 1 and `COMP971-MOTION-PELVIS`.

## Excluded deliberately

- UWB ranging, UWB calibration and the 15-pair mesh.
- Magnetometer and orientation calculation.

UWB is added next as a shared, official-Qorvo two-board stage for HEAD and
PELVIS. At the six-node stage, PELVIS coordinates the schedule and forwards
the results; it is not required to relay every radio exchange.

## Required acceptance

Verify J-Link programming by read-back comparison, then confirm PELVIS raw
ICM telemetry reaches the gateway at approximately 60 Hz.

## Installation result

The initial image installed correctly but carried a source defect: IMU batches
were labelled with node index 0 (HEAD) although PELVIS status packets correctly
used node index 1. The one-line source correction was rebuilt and reprogrammed.
J-Link updated one 4 KiB flash page and verified it normally. A final read-only
comparison checked all 167,332 defined Intel-HEX bytes with zero mismatches.

The live gateway session at
`hardware/outputs/records/acquisition/live_gateway_viewer/session_20261001_093509`
then showed PELVIS in its own row, `ICM ready`, at 58.8 Hz with zero observed
sequence gaps. The stale HEAD row was buffered data from the previously powered
HEAD; it stopped updating, consistent with HEAD being off.
