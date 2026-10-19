# COMP971 wearable hardware system guide

**Status:** engineering baseline, 29 September 2026  
**Use this document for:** architecture, wiring, operation, validation, and data collection decisions. For the live state of a particular board, read [01_CURRENT_SYSTEM_STATUS.md](01_CURRENT_SYSTEM_STATUS.md).

## Table of contents

1. [Purpose and boundaries](#1-purpose-and-boundaries)
2. [Hardware inventory and identity](#2-hardware-inventory-and-identity)
3. [Physical setup](#3-physical-setup)
4. [System architecture](#4-system-architecture)
5. [Data rates, packets, and synchronisation](#5-data-rates-packets-and-synchronisation)
6. [Data quality and model-input policy](#6-data-quality-and-model-input-policy)
7. [Firmware and folder authority](#7-firmware-and-folder-authority)
8. [Safe operating procedure](#8-safe-operating-procedure)
9. [Live viewer and captures](#9-live-viewer-and-captures)
10. [Calibration and acceptance rules](#10-calibration-and-acceptance-rules)
11. [Known limitations and troubleshooting](#11-known-limitations-and-troubleshooting)
12. [Flash safety](#12-flash-safety)
13. [Official technical sources](#13-official-technical-sources)

## 1. Purpose and boundaries

The project is a six-position human-motion system. Each DWM3001CDK has an external ICM-20948 IMU. The system must collect six-axis IMU data and all 15 unique body-to-body UWB distances.

This guide does not claim that every component is calibrated. A live packet or an LED means that a subsystem is powered or communicating; it does **not** by itself prove measurement accuracy.

The magnetometer is not used. The motion pipeline uses accelerometer and gyroscope data only.

## 2. Hardware inventory and identity

| Node | Position | J-Link serial | Intended role |
|---|---|---:|---|
| DWM-01 | Head | 760225805 | Wearable mesh node |
| DWM-02 | Pelvis | 760224787 | Wearable mesh coordinator and UWB collector |
| DWM-03 | Left wrist | 760225868 | Wearable mesh node |
| DWM-04 | Right wrist | 760225748 | Wearable mesh node |
| DWM-05 | Left ankle | 760225018 | Wearable mesh node |
| DWM-06 | Right ankle | 760225934 | Wearable mesh node |

The nRF52840 USB Dongle is the laptop gateway. Its COM number may change every time Windows reconnects it. The J-Link serial above is the permanent board identity. The machine-readable authority is [`../config/device_registry.json`](../config/device_registry.json).

## 3. Physical setup

### 3.1 External ICM-20948

The Adafruit ICM-20948 STEMMA QT/Qwiic connection is I2C. The intended DWM J10 wiring is:

| ICM lead | DWM J10 connection |
|---|---|
| Red / VIN | 3.3 V supply only |
| Black / GND | Pin 6, GND |
| Blue / SDA | Pin 3, SDA |
| Yellow / SCL | Pin 5, SCL |

Do **not** attach a 5 V J10 supply pin directly to the ICM's 3.3 V I2C power path unless an appropriate regulator board is specifically installed. The ICM red LED confirms it has power; it does not confirm I2C data is valid.

The DWM J2 power-selection jumper is part of the DWM's own power arrangement. It must remain in its normal documented position. Do not make or alter a solder bridge at J2 as a way to split power to the IMU.

### 3.2 Normal power and connections

- Use J9 USB only when flashing, debugging, or powering one board from the laptop.
- Use the battery for wearable operation.
- Plug the nRF52840 dongle into the laptop before starting a live viewer.
- Let the boards boot before opening the viewer. If the dongle changes COM port, use the currently listed port, not an old remembered COM number.

## 4. System architecture

### 4.1 Final data route

```text
Six ICM-20948 sensors --I2C--> six DWM3001CDK nodes
                                     |
                                     +-- BLE IMU telemetry --> nRF52840 dongle --> laptop

Six DWM3001CDK UWB radios -- scheduled DS-TWR mesh --> Pelvis
                                                    |
                                                    +-- BLE UWB results --> nRF52840 dongle --> laptop
```

Each wearable sends its own IMU data directly over BLE to the dongle. UWB is used for ranging, not for carrying continuous IMU streams. The Pelvis coordinator collects the UWB range results and sends them to the same gateway. This reduces UWB airtime and makes the laptop the single place where telemetry is combined and time-aligned.

### 4.2 UWB mesh

Six nodes have 15 unique unordered pairs:

| Pair index | Pair | Pair index | Pair |
|---:|---|---:|---|
| 0 | Head - Pelvis | 8 | Pelvis - Right ankle |
| 1 | Head - Left wrist | 9 | Left wrist - Right wrist |
| 2 | Head - Right wrist | 10 | Left wrist - Left ankle |
| 3 | Head - Left ankle | 11 | Left wrist - Right ankle |
| 4 | Head - Right ankle | 12 | Right wrist - Left ankle |
| 5 | Pelvis - Left wrist | 13 | Right wrist - Right ankle |
| 6 | Pelvis - Right wrist | 14 | Left ankle - Right ankle |
| 7 | Pelvis - Left ankle |  |  |

Every radio must be both a transmitter and receiver within the mesh schedule; the Pelvis is a coordinator, not a mandatory physical relay for an ankle-to-wrist distance. In the current firmware, PELVIS sends a command for one pair, waits for that ranging exchange and its result, then moves to the next pair. This is a centralized, sequential schedule: the firmware does not intentionally run two ranging exchanges at once. It is collision control, but it is not a distributed token system or a set of fixed, shared-clock TDMA slots.

There are 15 exchanges per full sweep. The simple estimate of 1.5–3 ms per exchange would imply about 22.5–45 ms per sweep (roughly 22–44 full sweeps per second), but that estimate leaves out this application's command messages, guards, receive windows, result collection and retries. It is not a measured rate for this firmware. In this code, a ranging receive window can wait about 16 ms, and a missing result can keep PELVIS waiting for up to about 41 ms. A measured full-mesh run refreshed the matrix about 1.2 times per second. A separate pair-only run attempted about 56 ranges per second, but that is one pair, not 15 pairs. These rates are from different tests and must not be compared as if they were one run.

The current scheduler is therefore much slower than the ideal estimate. We should not shorten its waits blindly: if a reply arrives after the receiver has stopped listening, that range fails. The next improvement should use the captured radio status to find which wait or transmit stage is failing, then change one timing value at a time and measure both speed and failure rate.

## 5. Data rates, packets, and synchronisation

### 5.1 Rate policy

- **IMU:** 60 Hz per healthy node. This is the selected acquisition rate for the human-motion data set. Each sample carries a sequence number.
- **UWB:** lower than the IMU rate. One pair measurement is produced in its assigned time slot; 15 distances are not simultaneous. The current schedule is deliberately conservative while radio reliability is being validated.
- **Viewer:** refreshes approximately once per second. It displays the newest received data; it is not a 60 Hz plot.

IMU and UWB do not need the same sampling rate. They are matched later by timestamp/sequence number on the laptop. For motion analysis, stable, well-characterised UWB is more important than forcing a high UWB rate.

### 5.2 What “60 Hz” means in the viewer

`60.0 Hz` beside a node means the viewer has observed a sequence cadence consistent with 60 IMU samples each second. The accelerometer (`A`) and gyroscope (`G`) values shown are raw sensor registers, not orientation.

Gyroscope values change even when a board appears still because of sensor noise and bias; that is normal. Calibration and filtering occur before model training, not in the text viewer.

### 5.3 Packet policy

The laptop receives small BLE messages. IMU messages contain node identity, sequence, and raw six-axis data. UWB result messages contain pair index, range, and validity. Invalid UWB attempts are counted separately and must not overwrite the last valid distance.

In the current firmware, the gateway limits each valid pair's UWB report to at most 2 per second, and invalid reports to at most 1 per second. That is only a BLE display/report limit; the radios can make more ranging attempts than the laptop receives reports. For example, one pair-only capture recorded about 56 radio attempts per second but delivered about 1.9 valid reports per second. Do not treat the laptop report count as the radio ranging rate.

### 5.4 What ranges and the IMUs can tell us

The 15 distances describe the relative shape of the six sensor locations. Distances alone do not show where that shape points in the room: moving or rotating the whole shape leaves all its pair distances unchanged. A distance-only reconstruction can also have a mirror-image ambiguity. Known fixed reference points and a chosen coordinate frame can anchor the result, but the frame and its handedness must be defined explicitly.

The ICM-20948 is used here as a six-axis accelerometer-plus-gyroscope sensor; the magnetometer is intentionally not used. Gravity can help estimate tilt when motion is suitable, and the gyroscope tracks short-term rotation, but yaw (compass heading) can drift without an external heading reference. Sensor fusion may combine IMU motion with UWB geometry, but it cannot create a stable absolute room heading from these inputs alone. Orientation/fusion has not yet been calibrated or implemented in the live viewer.

## 6. Data quality and model-input policy

The live viewer and all calibration captures retain **raw** data. They must not
silently replace a bad UWB result with a filtered value. This is important now:
the current UWB accuracy problem could still be a radio configuration or
calibration issue, and smoothing would only hide it.

| Data | Keep during acquisition | Do not do now | Later model-input policy, after UWB acceptance |
|---|---|---|---|
| IMU | Raw six-axis samples at 60 Hz, sequence number, and sensor timestamp | Do not average every 50 ms. At 60 Hz this combines about three samples and adds delay. | Keep the raw stream. Use the sensor's configured anti-alias filter and, only if validation supports it, a light causal filter. |
| UWB | Raw range, valid/invalid state, pair, and arrival time | Do not take a simple 50-ms average. A single 200 cm outlier can corrupt a 100 cm average. | Reject invalid reports, preserve an outlier reason, then test a short causal median/Hampel filter and optional light causal EMA. Store raw and filtered values side by side. |

The model will run on a 60 Hz timeline, but UWB must **not** pretend to be 60
Hz. Between two UWB reports, the laptop may carry the latest accepted range
forward, but it must also carry its **age** and validity/quality flags. For
example, a 100 cm result held for 50 ms is one 50-ms-old UWB measurement, not
three new UWB measurements. The model feature plan is therefore: 15 distances,
15 valid/quality flags, and 15 measurement ages alongside the six 60-Hz IMU
streams.

The first filtering experiment, after raw ranging passes the known-distance
test, will compare these recorded alternatives without changing firmware:

1. raw valid UWB;
2. physical jump gate plus a causal three-sample median; and
3. that median followed by a light causal EMA.

The comparison must report error, outlier rate, and delay. A filter is accepted
only if it improves error without making the 150 ms motion-prediction target
too late.

## 7. Firmware and folder authority

### 7.1 Editable source

| Purpose | Location |
|---|---|
| Wearable Motion Node Firmware (Production) | `../firmware/qorvo_qm33_motion_node/` |
| Archived Phase 1 Prototype | `../firmware/archive/dwm3001cdk_legacy_proto/` |
| nRF52840 gateway firmware | `../firmware/nrf52840_dongle_motion_gateway/` |
| Hardware identity and profiles | `../config/` |
| Operational PowerShell tools | `../tools/` |

All wearable nodes must use the same shared source, BLE protocol, IMU rate, and UWB packet format. The only planned differences are their node identity and the Pelvis coordinator configuration.

### 7.2 Records and backups

- `../outputs/records/` holds current measurement evidence.
- `../outputs/archive/` holds prior diagnostics and superseded trial images. Nothing in this archive is an approved fleet release.
- `../backups/` contains the six protected pre-flash board readbacks. Do not write build output into this folder.

## 8. Safe operating procedure

1. Confirm all jumpers/cables are secure. For an external IMU, confirm the red ICM LED is illuminated.
2. Power the wearable boards from their batteries.
3. Plug in the nRF52840 dongle and find its current COM port:

   ```powershell
   Get-CimInstance Win32_SerialPort | Select-Object DeviceID, Name
   ```

4. Run the live viewer with the reported gateway COM port.
5. Do not use UWB readings for calibration until a controlled capture passes the acceptance checks in section 9.

When flashing, connect **only the named board** by J9, verify its J-Link serial first, and use a recorded image. Never use a historical diagnostic image as a fleet image.

## 9. Live viewer and captures

### 9.1 Unified live viewer

Use the binary Phase-A viewer, replacing `COM16` with the gateway's current COM port:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ".\hardware\tools\live_gateway_binary_viewer.ps1" -Port COM16
```

This one viewer shows live six-axis IMU readings, sample rates and sequence gaps; UWB latest/report-delivery data; a separate UWB min/median/mean/sample-SD/max table; wearable health counters; UWB failure stages; and gateway packet/USB health. It first drains old gateway records for 20 seconds, showing only a countdown. It then starts a clean recorded session and displays the metrics in separate aligned tables. The preparation period is not included in the statistics or files. Stop with `Ctrl+C` or use `-DurationSeconds 60` to save a 60-second recorded session automatically. Use `-WarmupSeconds 0` only to skip the preparation delay. Each completed run saves `summary.json` (including min/mean/SD/max for each raw IMU axis), received UWB reports as `uwb_reports.csv`, and UWB failure stages as `uwb_failure_diagnostics.csv` under `outputs/records/acquisition/live_gateway_viewer/`. Only one program can own the gateway serial port at a time.

For a measured-distance check, add the matching pair index and known distance. For example, pair 5 is PELVIS–LEFT-WRIST:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ".\hardware\tools\live_gateway_binary_viewer.ps1" -Port COM16 -DurationSeconds 60 -Pair 5 -KnownDistanceCm 100
```

The viewer reports mean bias, mean absolute error, and the share of valid samples within 5 cm and 10 cm of the stated distance. Those tolerances are descriptive metrics here, not automatic pass limits. Its valid/invalid report percentage counts UWB result messages received over BLE; compare the separate PELVIS attempt/invalid/timeout counters and failure-stage counts to assess actual ranging success.

### 9.2 Controlled UWB capture

For the Pelvis-Left-Wrist pair (pair 5), with the boards held still at exactly 100 cm for 45 seconds:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ".\hardware\tools\capture_uwb_calibration.ps1" -Port COM16 -Pair 5 -KnownDistanceCm 100 -DurationSeconds 45
```

The capture writes a dated JSON record under `outputs/records/calibration/`. Use the same command with the measured distance and the correct pair index for another pair.

## 10. Calibration and acceptance rules

### 10.1 UWB

UWB calibration starts only after range transport is reliable. For every pair selected for calibration:

1. Place the antenna centres at a measured distance, clear of hands, bodies, battery packs, and metal.
2. Capture at 50 cm, 75 cm, and 100 cm, plus repeat at least one point.
3. Record valid count, invalid count, median, and standard deviation.
4. Test a second physical pair. A correction that works only for one pair is not a fleet calibration.
5. Accept a correction only when it improves repeatability and distance error across the tested ranges and pairs.

The recent Pelvis-Left-Wrist captures did **not** pass these criteria:

| Stated distance | Valid / invalid | Median | Standard deviation |
|---:|---:|---:|---:|
| 50 cm | 104 / 6 | 47 cm | 15.6 cm |
| 75 cm | 118 / 2 | 96 cm | 8.47 cm |
| 100 cm | 122 / 2 | 150 cm | 26.5 cm |

Transport is working, but the non-constant error means a simple one-value antenna-delay adjustment is not justified. Repeat testing with another pair before changing radio timing or calibration values.

After matching standard firmware was restored to Head and Pelvis, a clean
Head–Pelvis 100 cm capture produced 72 valid readings, 5 invalid attempts, a
174 cm median, and an 8.81 cm standard deviation. This independently confirms
UWB frame exchange, while also confirming that accuracy remains unacceptable.

| Stated Head–Pelvis distance | Valid / invalid | Median | Standard deviation |
|---:|---:|---:|---:|
| 50 cm | 78 / 3 | 62 cm | 2.64 cm |
| 75 cm | 65 / 6 | 82 cm | 11.66 cm |
| 100 cm | 72 / 5 | 174 cm | 8.81 cm |

The raw records are `head_pelvis_*cm_2026-09-29_*.json` in
`../outputs/records/calibration/`. The non-constant error rules out applying a
single global offset. The next required evidence is the same curve from a
second physical pair.

### 10.1.1 Factory OTP caution for DWM3001CDK kits

The local official `DWM3001C Data Sheet` maps OTP word `0x01A` to the
channel-5 PRF-64 RX/TX antenna-delay values. Reading that word is safe; it does
not write OTP. It does **not** mean that its value is always appropriate to
apply in an application. The same data sheet says that the engineering-sample
`DWM3001CDKE1.0` kit must not use the channel-5 antenna delay in OTP and must
use default `16390` instead.

For this project, PELVIS and LEFT-WRIST reported valid values at `0x01A`, but a
controlled 100 cm test applying them at runtime produced a 184 cm median and
26.5 cm SD. This is rejected. Confirm the physical kit revision before using
those values, and never write a guessed calibration value to OTP. This remains
separate from the open range-calculation and radio-configuration investigation.

### 10.2 IMU

For each healthy node, IMU work must include:

1. A stationary bias capture for the gyro.
2. A six-face accelerometer calibration.
3. A fixed, recorded mounting orientation for each body position.
4. Validation with a known still and controlled-motion sequence.

Do not calibrate magnetometer data for this project because it is not used. Do not include invalid Pelvis IMU samples in training or fusion.

## 11. Known limitations and troubleshooting

| Symptom | Meaning | Action |
|---|---|---|
| Viewer says `waiting` for a node | No recent BLE packet from that node | Check power, ICM LED, board boot, dongle COM port, and that no other program owns the port. |
| `-1 cm` or `invalid just now` | A UWB attempt failed | Keep the last valid reading, count the failure, and test with spacing/orientation before changing firmware. |
| UWB is live but wrong | Radio transport works; calibration does not | Follow the controlled multi-distance, multi-pair procedure. |
| Dongle COM port is denied | Another program has the port | Stop the viewer/capture with `Ctrl+C`, then run only one tool. |
| A sensor value is fixed at `32767` | That axis is saturated and invalid | Stop using that sensor stream and diagnose the local ICM wiring or hardware. |

BLE automatic recovery has been demonstrated for Pelvis and Left-Wrist after a gateway unplug/replug. It remains a release acceptance requirement for the full six-node set.

## 12. Flash safety

The nRF52833 internal flash endurance is specified in the Nordic product documentation as a minimum of 10,000 erase/write cycles. Manual development flashing does not approach that limit in normal work. The practical risks are flashing the wrong board, using an incompatible test image, or continuously writing telemetry to flash at run time.

The firmware must retain live measurements in RAM and transmit them over BLE; it must not log high-rate IMU or UWB samples to internal flash. If persistent configuration is later required, write rarely and use a wear-levelled storage scheme.

Before any material firmware experiment:

1. Check the connected board's J-Link serial.
2. Confirm its protected image exists in `../backups/`.
3. Save the exact image, source revision, date, and test purpose under `../outputs/records/`.
4. Flash one named board only and verify it before touching another.

## 13. Official technical sources

- Qorvo: [DWM3001CDK development kit](https://www.qorvo.com/products/ek/DWM3001CDK)
- Qorvo: [DWM3001C module](https://www.qorvo.com/products/p/DWM3001C)
- Qorvo: [DW3xxx API Guide](https://forum.qorvo.com/uploads/short-url/xD3TlXKvkujjdUaJWXv2b4E7GQN.pdf) (delayed-transmit status and response-expected receive behavior)
- Qorvo: [DWM3001CDK OTP calibration guidance](https://forum.qorvo.com/t/how-to-reduce-distance-d-cm-error/20404) (factory calibration values and `GETOTP` reference)
- Qorvo: [DW3/QM33 SDK release](https://forum.qorvo.com/t/new-release-of-dw3000-and-qm33-sdk-dw3-qm33-sdk-v1-0-2/19160) (official SDK includes UCI/CLI calibration tools)
- Nordic Semiconductor: [nRF52833 product specification](https://docs.nordicsemi.com/bundle/ps_nrf52833/page/keyfeatures_html5.html)
- Adafruit: [ICM-20948 9-DoF IMU guide](https://learn.adafruit.com/adafruit-tdk-invensense-icm-20948-9-dof-imu)
- TDK InvenSense: [ICM-20948 datasheet](https://invensense.tdk.com/wp-content/uploads/2016/06/DS-000189-ICM-20948-v1.3.pdf) and [sensor-fusion overview](https://www.invensense.tdk.com/en-us/sensorfusion)

The Qorvo product documentation confirms that module calibration data is held in one-time-programmable memory. Do not overwrite or guess it. The current range fault is not proven to be an antenna-delay-only problem.
