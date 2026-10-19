# HEAD official-Qorvo ICM-20948 60 Hz pilot

## Purpose

This is the second custom COMP971 application step above the official Qorvo
QM33 SDK: retain the proven Bluetooth discovery/reconnect behaviour and add
raw external ICM-20948 accelerometer and gyroscope telemetry from HEAD.

## Exact board and safety boundary

| Item | Value |
| --- | --- |
| Board to program | `DWM-01-HEAD` only |
| J-Link serial | `760225805` |
| Recovery backup | `hardware/backups/DWM3001CDK_760225805/2026-10-01_pre_comp971_official_port_pilot` |
| Install method | Normal J-Link `loadfile`, then read-only flash comparison |
| Not performed | Chip erase, UICR write, OTP write, antenna/calibration write, runtime flash storage write, dongle firmware change |

## What this image adds

| Area | Behaviour |
| --- | --- |
| I2C pins | SDA P0.26 (J10 pin 3), SCL P0.23 (J10 pin 5), using TWIM1 at 100 kHz |
| Power expectation | The external ICM is powered from the regulated 3.3 V J2 point and GND J10 pin 6; it is not powered from J10 pins 2 or 4 |
| Sensor detection | Tries I2C addresses `0x69`, then `0x68`; expects ICM-20948 `WHO_AM_I = 0xEA` |
| Sensor mode | ±2 g accelerometer and ±250 dps gyroscope |
| Samples | Six-axis acceleration/gyro plus raw temperature at an average 60 samples/second |
| Bluetooth transport | Six samples in each notification: about 10 notifications/second, while preserving each sample's sequence number and timestamp |
| Not included | Magnetometer, orientation calculation, UWB ranging, 15-pair mesh, calibration write |

If the ICM is disconnected or not detected, HEAD still advertises and can
connect to the gateway. It retries detection every two seconds instead of
blocking or repeatedly restarting the board.

## Build evidence

Source project: `hardware/firmware/qorvo_qm33_motion_node/official_sdk_project`

Target: `DWM3001CDK-COMP971-FreeRTOS`

| Item | Value |
| --- | --- |
| Flash use | 169,744 / 516,096 bytes (32.89%) |
| RAM use | 54,308 / 114,688 bytes (47.35%) |
| HEX SHA-256 | `39BEF402EB98E00EAB34E4040458F9516309B62DAEF10E3F8062B7CA884380A0` |
| Build result | Completed successfully before installation |

## Required acceptance test after installation

1. Leave HEAD powered and the nRF52840 dongle connected.
2. Run the live gateway viewer for a short session.
3. Confirm a live HEAD IMU row at approximately 60 Hz and non-zero changing raw
   acceleration/gyro values when the sensor is moved.
4. Confirm the gateway still receives HEAD status data after a dongle-only
   unplug/replug; do not restart HEAD for that check.

The post-install flash check will compare every defined Intel-HEX byte that was
read from HEAD. It does not treat undefined HEX gaps as data.

## Installation and initial acceptance result

Installed on 2026-10-01 on `DWM-01-HEAD` / J-Link serial `760225805` only.
J-Link reported normal program-and-verify success. The separate read-only
comparison checked 167,332 defined Intel-HEX bytes and found **zero
mismatches**.

The gateway session at `COM16` is saved in
`hardware/outputs/records/acquisition/live_gateway_viewer/session_20261001_084205`.
It received 708 consecutive raw HEAD ICM samples in 12.031 seconds. The
viewer displayed 58.8 Hz because its session clock began before the Bluetooth
connection and first six-sample notification; 708 samples equal 11.8 seconds
at the configured 60 Hz. There were zero malformed Bluetooth packets, zero
HEAD Bluetooth IMU-notification failures, and zero observed sequence gaps.

The gateway USB-drop counter increased by 9,086 near the beginning of that
viewer session, then stayed unchanged while HEAD samples continued without
gaps. This counter requires separate gateway investigation before a long
recording is accepted, but it did not prevent this short ICM acquisition and
transport acceptance test.

## Dongle-only reconnect result

Passed on 2026-10-01. The nRF52840 dongle was unplugged and replugged while
HEAD remained powered and untouched. After a 20-second drain period, the clean
12.012-second session at
`hardware/outputs/records/acquisition/live_gateway_viewer/session_20261001_090259`
received 738 HEAD samples, with zero observed sequence gaps, zero duplicate
sequences, zero HEAD Bluetooth IMU-notification failures, zero malformed
gateway packets, and zero new USB drops. This confirms the intended
plug-and-play reconnect behaviour for the current HEAD ICM pilot.
