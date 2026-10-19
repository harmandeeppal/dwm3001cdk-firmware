# Final BLE, UWB, and time-synchronisation plan

Status: combined six-board BLE + UWB functional verification completed on 28 September 2026. This is the selected architecture for the six-position wearable system. Range accuracy, loss/latency limits, and cross-device clock mapping remain validation work.

## Decision

The system uses separate radio paths for separate jobs:

```text
Each of six DWM3001CDK boards
  external ICM-20948 (60 Hz, accelerometer + gyroscope)
      └── BLE notification ──> nRF52840 Dongle ──USB──> laptop

Six DWM3001CDK UWB radios
  scheduled 15-pair distance measurements
      └── UWB result frames ──> DWM-02-PELVIS
                                      └── BLE notification ──> nRF52840 Dongle
```

The laptop is the only fusion location. PELVIS is the UWB coordinator and range-matrix collector; it is **not** the relay for the other five IMU streams.

## Roles

| Component | Final role |
|---|---|
| DWM-01 HEAD through DWM-06 RIGHT-ANKLE | BLE peripherals; each sends its own six-axis IMU samples to the dongle. |
| DWM-02 PELVIS | UWB coordinator; BLE peripheral; sends its own IMU samples and every collected UWB range result to the dongle. |
| nRF52840 Dongle | BLE central for the six wearable boards; USB bridge to the laptop. |
| Laptop | Records packets, converts each source clock to a shared clock, checks loss, and performs sensor fusion. |

The DWM3001C integrates a DW3110 UWB transceiver and an nRF52833 BLE SoC. The nRF52833 supports 1 Mbps and 2 Mbps Bluetooth LE PHYs. The nRF52840 Dongle is a USB-connected BLE development device. These are hardware facts, not a claim that a six-node wearable system is automatically complete: [Qorvo DWM3001C](https://www.qorvo.com/products/p/DWM3001C), [Nordic nRF52833 specification](https://docs.nordicsemi.com/r/bundle/ps_nrf52833/page/keyfeatures_html5.html), [Nordic nRF52840 Dongle](https://docs.nordicsemi.com/r/bundle/ug_nrf52840_dongle/page/ug/nrf52840_dongle/intro.html).

## Why this design is selected

UWB range exchanges are timing-sensitive. Sending high-rate IMU traffic over the same UWB TDMA schedule reduces the time available for the 15 direct range pairs. BLE provides a dedicated telemetry path, while UWB remains dedicated to ranging and UWB clock synchronisation.

Six IMUs at 60 Hz require about 8 kB/s of raw six-axis application payload before BLE framing. Nordic documents 16 kbps per link for a configuration with up to eight 1 Mbps BLE connections and short connection events. That is enough capacity in principle for an approximately 10--12 kbps stream per wearable, but body motion, interference, retransmissions, and memory limits still need live validation. The final link parameters must be chosen from measured loss and latency, not assumed from a laboratory maximum: [Nordic BLE throughput guidance](https://docs.nordicsemi.com/r/bundle/sds_s132/page/sds/s1xx/ble_data_throughput/ble_data_throughput.html).

The all-to-all UWB matrix is custom firmware. Qorvo's supplied FiRa sessions are one-to-one or one-to-many rather than a turnkey many-to-many wearable mesh, and radio airtime becomes more constrained as devices are added. The project therefore retains a single scheduled UWB coordinator: [Qorvo multi-device guidance](https://forum.qorvo.com/t/dwm3001cdk-multiple-tags-and-battery-charging/19309), [Qorvo airtime/collision guidance](https://forum.qorvo.com/t/anchor-number-limit/23071).

## Timing and synchronisation

BLE transmission times will not be simultaneous. The central schedules each connection and packets can be delayed. Fusion must therefore use the **measurement time**, not the time that the laptop receives a packet.

1. Every ICM sample receives a node-local microsecond timestamp and a sequence number when it is read at 60 Hz.
2. PELVIS periodically performs a UWB two-way clock-synchronisation exchange with every remote wearable.
3. Each exchange estimates the remote clock offset, drift, and uncertainty relative to PELVIS. PELVIS sends this clock status to the laptop by BLE.
4. The laptop maps every source timestamp to a shared PELVIS/laptop timeline, detects missing sequence numbers, and resamples only for fusion.

This creates synchronised measurements; it does not try to make six BLE radios transmit at the same instant. The current working firmware includes source sequence numbers, but it does **not** yet include this full six-board clock mapping. No claim of perfectly synchronised six-board data may be made until the validation gates below pass.

## Packet and connection policy

- `60 Hz` is the IMU sampling rate for every board.
- Each BLE message carries up to three timestamped samples, normally sent every 50 ms. Batching reduces radio overhead while retaining individual sample times. The maximum added live-display latency is about 50 ms.
- Begin with `1 Mbps` BLE PHY for body-worn reliability. Test `2 Mbps` only if it demonstrably reduces loss/latency without reducing link robustness.
- Enable Data Length Extension and use a negotiated ATT MTU of at least 100 bytes before enabling three-sample batches.
- BLE notifications are used for live IMU and UWB telemetry. Sequence numbers expose loss; a short RAM queue permits a later batch to include samples that could not be sent immediately.
- PELVIS emits UWB reports with pair index, range, timestamp, validity, and quality flags. Missing/invalid ranges are recorded as such and are never silently held out as new measurements.
- Final deployments must use BLE bonding and LE Secure Connections. Initial bench tests may use an explicitly labelled temporary open test mode only.

## UWB-rate policy

IMU and UWB do not need the same rate. The first six-board target is a stable, timestamped **10 Hz full 15-pair UWB matrix**, with 60 Hz IMUs. We may raise the matrix rate only after all 15 links satisfy validity, loss, latency, and on-body range-error gates. This is deliberately conservative so that the first end-to-end, body-worn six-board validation does not overload the UWB TDMA schedule.

## Verified BLE bench result — 28 September 2026

All six DWM3001CDK boards were powered from battery and connected simultaneously to the nRF52840 Dongle. A fresh 30-second capture, after clearing earlier USB console text, received 60 or 61 gateway diagnostic messages from each board. The source sequence-number and source-timestamp calculation was **60.0 Hz** for every board, with no backwards sequence numbers.

| Node | Board | J-Link serial | Result |
|---:|---|---:|---|
| 0 | DWM-01-HEAD | 760225805 | BLE IMU received at 60.0 Hz |
| 1 | DWM-02-PELVIS | 760224787 | BLE IMU received at 60.0 Hz |
| 2 | DWM-03-LEFT-WRIST | 760225868 | BLE IMU received at 60.0 Hz |
| 3 | DWM-04-RIGHT-WRIST | 760225748 | BLE IMU received at 60.0 Hz |
| 4 | DWM-05-LEFT-ANKLE | 760225018 | BLE IMU received at 60.0 Hz |
| 5 | DWM-06-RIGHT-ANKLE | 760225934 | BLE IMU received at 60.0 Hz |

This verifies six named BLE links, the external ICM-20948 readings, source timestamps, and basic battery-powered operation. It does **not** yet verify a 10-minute loss test or cross-device clock mapping.

## Verified combined BLE + UWB result — 28 September 2026

The six named boards were flashed with the combined firmware. Each board sends its own ICM-20948 accelerometer and gyroscope samples directly to the nRF52840 Dongle by BLE. PELVIS runs the UWB coordinator and forwards each range result to the same dongle by BLE.

In a fresh 45-second test with all six DWM boards powered from their batteries and only the nRF52840 Dongle connected to the laptop:

- Gateway telemetry was received from all six IMU node indices, `0` through `5`.
- All 15 scheduled UWB pair indices, `0` through `14`, produced valid reports at least once.
- The route `UWB pair -> PELVIS -> BLE -> nRF52840 Dongle -> laptop` is therefore functionally verified for the complete six-board mesh.

This is a communications verification, not a range-accuracy result. Raw UWB values must still be calibrated and validated at known antenna-to-antenna distances, including on-body conditions.

## Delivery order and acceptance gates

1. Build and test a one-board BLE IMU peripheral and the nRF52840 Dongle central/USB bridge. **Completed for all six boards in the basic bench test.** A 10-minute stationary loss/latency test remains.
2. Add the remaining five peripherals. **Completed for basic six-board connectivity and 60 Hz source-rate verification.** Add final loss/latency logging before the 10-minute acceptance test.
3. Implement UWB clock synchronisation. Compare clocks before/after a 10-minute test and set an evidence-based cross-device alignment threshold.
4. Re-enable the full 15-pair UWB schedule with UWB IMU forwarding disabled. **Completed for functional six-board telemetry:** every pair has reached PELVIS and then the dongle in a battery-powered test. Measure matrix rate, loss, latency, and range stability next.
5. Perform walking and body-shadow tests, then antenna-delay calibration with known distances. Test 1 Mbps and 2 Mbps BLE PHYs and choose the measured better mode.
6. Only after these gates: enable encrypted/bonded recording and use the data for model training.

## What is needed from the user

Nothing needs to be flashed now. Keep the nRF52840 Dongle connected to the laptop and the six wearable boards powered for the next stage: controlled UWB range calibration and a longer stationary loss test.
