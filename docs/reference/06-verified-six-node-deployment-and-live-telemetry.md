# Verified six-node deployment and live telemetry

Status: functional six-board BLE + UWB deployment verified on 28–29 September 2026. This document records the exact named setup currently used for bench and wearable tests. It is not a calibration certificate and it does not claim centimetre-accurate UWB ranges.

## Verified automatic Bluetooth recovery — 29 September 2026

### What failed

When the nRF52840 Dongle was unplugged and restarted, PELVIS and LEFT-WRIST
previously stayed absent from the live viewer. A manual board reboot made a
board return immediately. This showed that the dongle scan worked, but a
normal board disconnection was not reliably restarting BLE advertising.

The original DWM code called `bt_le_adv_start()` once in its BLE
`disconnected()` callback and ignored the return value. A one-time attempt can
fail while the BLE controller is still completing the old connection.

### Implemented recovery design

The final design has two layers:

1. **Gateway heartbeat.** The nRF52840 gateway writes a one-byte heartbeat
   once per second to every connected DWM board over the custom BLE GATT
   service. A DWM board that still believes it is connected but receives no
   heartbeat for 10 seconds performs a normal software reboot. This is only a
   recovery path for a genuinely stuck connection.
2. **Advertising retry.** After a normal BLE disconnect, each DWM board calls
   `ensure_connectable_advertising()`. If `bt_le_adv_start()` is not yet ready,
   a Zephyr delayed-work item tries again every second until advertising is
   active. This is the recovery path used in the verified dongle-unplug test.

The relevant source is
`hardware/firmware/archive/dwm3001cdk_legacy_proto/src/ble_motion_transport.c`.
The gateway heartbeat implementation is in
`hardware/firmware/nrf52840_dongle_motion_gateway/src/main.c`.

This logic uses RAM timers and BLE controller state only. It does not write
runtime data to on-chip flash, and it does not change the UWB or ICM-20948
configuration.

### Installed board images

| Device | J-Link serial | Installed recovery image |
|---|---:|---|
| DWM-02-PELVIS | 760224787 | advertising retry + 10-second stale-link recovery |
| DWM-03-LEFT-WRIST | 760225868 | advertising retry + 10-second stale-link recovery |
| nRF52840 Dongle | n/a | gateway heartbeat v9 |

### Controlled recovery test and result

The test used PELVIS and LEFT-WRIST on battery, approximately 100 cm apart,
with the dongle as the only laptop USB data connection.

1. Confirmed both PELVIS and LEFT-WRIST were live at 60 Hz in the viewer.
2. Stopped the viewer so COM16 was free.
3. Unplugged only the nRF52840 Dongle for 12 seconds.
4. Replugged the dongle; neither DWM board was powered off, restarted, or
   reconnected through J9.
5. Waited 20 seconds, then reopened the viewer on COM16.

Result: both PELVIS and LEFT-WRIST returned automatically and were live at
60 Hz. UWB pair 5 (PELVIS ↔ LEFT-WRIST) also returned as a live result. The
other 14 pairs correctly showed `waiting` because their boards were powered
off. The displayed pair-5 value was a raw UWB result (132 cm in the successful
post-recovery screen), not a calibration value.

This proves automatic Bluetooth recovery for the tested two-board setup. It
does **not** prove UWB distance accuracy; controlled UWB calibration remains a
separate next step.

## Known PELVIS IMU limitation — 29 September 2026

During a stationary live test, PELVIS repeatedly reported accelerometer Z as
`32767`, the maximum signed 16-bit value. The reading remained `32767` after
the complete PELVIS board was rotated upside down and after a full battery
power cycle. LEFT-WRIST, running the same firmware, reported approximately
`+16500` on its gravity axis and responded normally to movement.

Conclusion: this is not normal gyro/accelerometer noise and cannot be repaired
by software calibration. It is currently treated as a PELVIS-specific ICM-20948
module or local wiring/data-path fault.

Until hardware isolation or replacement is performed:

- keep PELVIS available for UWB ranging and BLE UWB reporting;
- exclude PELVIS IMU samples from IMU calibration, sensor fusion, and model
  training;
- calibrate the other five ICM-20948 modules normally;
- do not apply an artificial offset to convert `32767` into a usable value.

## PELVIS ↔ LEFT-WRIST UWB controlled-distance results — 29 September 2026

The following captures used UWB pair 5 (PELVIS ↔ LEFT-WRIST), both boards on
battery and held still in a clear path. The same live viewer capture command
was used for each 60-second capture.

| Stated distance | Valid / invalid results | Median raw range | Standard deviation |
|---:|---:|---:|---:|
| 50 cm | 104 / 6 | 47 cm | 15.6 cm |
| 75 cm | 118 / 2 | 96 cm | 8.47 cm |
| 100 cm | 122 / 2 | 150 cm | 26.5 cm |

The raw values demonstrate that the UWB packet route and pair-5 ranging are
active, but they do **not** form a stable single offset or scale relationship.
No calibration coefficient has been calculated or installed. These data must
not be used as an accuracy claim. The next task is firmware/radio calculation
review before collecting more controlled-distance samples.

### Range-review outcome — 29 September 2026

The custom double-sided two-way-ranging timestamp calculation was compared
with Qorvo's reference pattern. It uses the same four timing intervals and the
same delayed-final timestamp construction (the programmed delayed TX time plus
the TX antenna delay). No unambiguous arithmetic error was found, so no
untested timestamp change was made.

The result is **not a normal antenna-delay calibration case**. Antenna delay
produces an approximately constant range offset. These three measurements have
both a changing error and unusually large spread. The current values therefore
indicate an unreliable radio measurement path at the test setup, rather than a
number that can safely be subtracted from every future range.

The DWM3001CDK contains a DWM3001C/DW3110 module. Qorvo confirms that
DWM3001C modules have factory calibration data in OTP, and that DWM3001CDK
range calibration data can be retrieved with its official CLI `GETOTP`
command. This factory calibration is an important secondary check, but cannot
by itself explain the non-constant errors above. We will not hand-write OTP or
guess an OTP memory-map interpretation. The safe next engineering step is a
temporary, read-only radio-quality
diagnostic, followed by a line-of-sight repeat using the same pair. It will
report measurement quality and will not alter OTP, firmware flash at runtime,
or any saved calibration setting.

Sources: [Qorvo DWM3001CDK product page](https://www.qorvo.com/products/ek/DWM3001CDK),
[Qorvo confirmation of DWM3001C factory OTP calibration](https://forum.qorvo.com/t/dw3000-customer-otp-space/24226/2),
[Qorvo DWM3001CDK calibration guidance](https://forum.qorvo.com/t/how-to-reduce-distance-d-cm-error/20404).

## 1. Purpose and boundaries

This deployment provides two live data streams from the six wearable positions:

1. Each external ICM-20948 sends its own accelerometer and gyroscope samples directly to the laptop through Bluetooth LE (BLE).
2. The DWM3001CDK UWB radios execute a scheduled all-to-all mesh. PELVIS collects the 15 pair results and sends them to the laptop through BLE.

The nRF52840 Dongle is the only radio device connected to the laptop during normal operation. The laptop is the future location for logging, sensor fusion, orientation estimation, and the motion model.

## 2. Named hardware inventory

| Node index | Wearable name | DWM3001CDK J-Link serial | UWB role | BLE advertised name |
|---:|---|---:|---|---|
| 0 | DWM-01-HEAD | 760225805 | Scheduled range participant | `COMP971-MOTION-HEAD` |
| 1 | DWM-02-PELVIS | 760224787 | UWB coordinator and UWB-result collector | `COMP971-MOTION-PELVIS` |
| 2 | DWM-03-LEFT-WRIST | 760225868 | Scheduled range participant | `COMP971-MOTION-LEFT-WRIST` |
| 3 | DWM-04-RIGHT-WRIST | 760225748 | Scheduled range participant | `COMP971-MOTION-RIGHT-WRIST` |
| 4 | DWM-05-LEFT-ANKLE | 760225018 | Scheduled range participant | `COMP971-MOTION-LEFT-ANKLE` |
| 5 | DWM-06-RIGHT-ANKLE | 760225934 | Scheduled range participant | `COMP971-MOTION-RIGHT-ANKLE` |

Every DWM board has one external Adafruit ICM-20948 connected. The ICM magnetometer is not read or transmitted by this firmware.

## 3. Physical setup and power

### Normal live use

- All six DWM3001CDK boards run from their batteries.
- The nRF52840 Dongle is plugged into the laptop USB port.
- No DWM board needs to remain connected to J9 or USB after successful flashing and verification.
- J9 is used only to identify, flash, verify, or diagnose one named DWM board at a time.

### External ICM-20948 connection

The tested I2C signal wiring on each DWM board is:

| ICM-20948 lead | DWM3001CDK connection | Meaning |
|---|---|---|
| Red / VIN | J2 selected **3.3 V** rail | ICM board power |
| Black / GND | J10 pin 6 | Common ground |
| Blue / SDA | J10 pin 3 | I2C data |
| Yellow / SCL | J10 pin 5 | I2C clock |

The J2 power-selection link must be installed so that the selected J2 supply is the measured 3.3 V rail. The current boards use the soldered J2 link. Do not use J10 pin 2 or pin 4 as the ICM supply for this deployment: those are 5 V header pins, while the tested connection uses the 3.3 V J2 rail.

The firmware uses I2C0 at 100 kHz. Its nRF52833 pins are SDA P0.26 and SCL P0.23. A red LED on an ICM board confirms power only; live gateway IMU messages confirm both power and I2C data.

## 4. Firmware that is currently deployed

### DWM3001CDK combined firmware

Source directory: `hardware/firmware/archive/dwm3001cdk_legacy_proto/`.

Important source files:

| File | Responsibility |
|---|---|
| `src/main.c` | DW3110 initialisation, scheduled 15-pair mesh, PELVIS range collection, board node index |
| `src/icm20948_stream.c` | ICM-20948 initialisation and 60 Hz accel/gyro sampling |
| `src/ble_motion_transport.c` | BLE peripheral, reconnect advertising, IMU batches, PELVIS UWB reports |
| `src/ble_telemetry_protocol.h` | Common version-1 binary packet contract |
| `zephyr/nrf52833dk_nrf52833.overlay` | DW3110 SPI/control pins and external I2C pins |
| `zephyr/ble_*.conf` | Per-board BLE advertised names |

The six images were built from the same source using the corresponding `UWB_MESH_NODE_INDEX` value from 0 to 5 and `COMP971_DIRECT_BLE_IMU=1`.

`COMP971_DIRECT_BLE_IMU=1` is deliberate: it prevents high-rate IMU payloads from using the UWB schedule. UWB is reserved for distance measurement and PELVIS-to-laptop range reporting.

### ICM configuration

- Sampling: 60 Hz.
- Measurements: accelerometer, gyroscope, and temperature.
- Magnetometer: not read and not transmitted.
- Accelerometer setting: register value `0x00`, ±2 g full scale.
- Gyroscope setting: register value `0x00`, ±250 degrees/s full scale.
- Each sample has a per-board sequence number and a local microsecond timestamp.

### nRF52840 Dongle gateway

Source directory: `hardware/firmware/nrf52840_dongle_motion_gateway/`.

The dongle runs as a BLE central. It scans for the `COMP971-MOTION-` name prefix, connects to all six peripherals, subscribes to their telemetry characteristic, and writes gateway diagnostic data to USB serial. On this laptop the observed diagnostic console is COM16 at 115200 baud; Windows may assign a different COM number after reconnecting the dongle.

## 5. Architecture and data route

```text
HEAD / PELVIS / LEFT-WRIST / RIGHT-WRIST / LEFT-ANKLE / RIGHT-ANKLE
  ICM-20948 at each position
      -> local 60 Hz sample, sequence number, local timestamp
      -> BLE notification -> nRF52840 Dongle -> USB -> laptop

All six DWM3001CDK DW3110 UWB radios
      -> one scheduled range transaction for every body pair
      -> PELVIS stores the range result for the current pair
      -> BLE notification -> nRF52840 Dongle -> USB -> laptop
```

PELVIS is not a BLE relay for the five remote IMUs. Each remote board sends its own IMU data directly to the dongle. PELVIS is the UWB coordinator and the source of all UWB reports sent by BLE.

## 6. BLE packet contract

The custom service UUID is `6e2fd6a4-6a4c-4b64-b389-139718b02410`. The telemetry characteristic UUID is `6e2fd6a5-6a4c-4b64-b389-139718b02410`.

| Packet | Contents | Live policy |
|---|---|---|
| IMU batch, type `0x01` | Protocol version, node index, sample count, then one to three 22-byte samples | Sampled at 60 Hz; batches are considered every 50 ms |
| UWB range, type `0x03` | PELVIS source index, pair index, PELVIS timestamp, range in cm, validity flags | Valid pair reports are limited to 10 Hz per pair; invalid reports to 1 Hz per pair |

Each IMU sample contains: `sequence`, `local_timestamp_us`, raw signed 16-bit `ax ay az gx gy gz`, and raw signed 16-bit temperature. The timestamps wrap after about 71.6 minutes; future laptop logging must expand them using sequence continuity. The firmware currently has `sync_epoch = 0`; full cross-device clock mapping is not implemented yet.

## 7. UWB pair map

| Pair | Positions | Pair | Positions |
|---:|---|---:|---|
| 0 | HEAD ↔ PELVIS | 8 | PELVIS ↔ RIGHT-ANKLE |
| 1 | HEAD ↔ LEFT-WRIST | 9 | LEFT-WRIST ↔ RIGHT-WRIST |
| 2 | HEAD ↔ RIGHT-WRIST | 10 | LEFT-WRIST ↔ LEFT-ANKLE |
| 3 | HEAD ↔ LEFT-ANKLE | 11 | LEFT-WRIST ↔ RIGHT-ANKLE |
| 4 | HEAD ↔ RIGHT-ANKLE | 12 | RIGHT-WRIST ↔ LEFT-ANKLE |
| 5 | PELVIS ↔ LEFT-WRIST | 13 | RIGHT-WRIST ↔ RIGHT-ANKLE |
| 6 | PELVIS ↔ RIGHT-WRIST | 14 | LEFT-ANKLE ↔ RIGHT-ANKLE |
| 7 | PELVIS ↔ LEFT-ANKLE |  |  |

## 8. Verified result on 28 September 2026

The final test used all six boards powered by battery, with only the nRF52840 Dongle connected to the laptop.

- Every node index, 0 through 5, transmitted live IMU telemetry to the gateway.
- Every UWB pair index, 0 through 14, produced at least one valid range report during a fresh 45-second capture.
- This verifies the complete communications route for six IMUs and the full 15-pair UWB mesh.

The test does not establish the accuracy of the raw ranges. Earlier short-distance checks showed small and pair-dependent range offsets, which is why calibration remains the next hardware stage.

## 9. What is ready and what is not yet claimed

| Item | Status |
|---|---|
| Six named board identities | Verified |
| Six external ICM-20948 data paths | Verified |
| Six direct BLE IMU paths to dongle | Verified |
| PELVIS UWB collection and BLE forwarding | Verified |
| All 15 UWB pair reports | Functionally verified |
| Raw UWB distance accuracy | Not calibrated |
| Continuous 10 Hz full range matrix | Target; must be measured with logging |
| 10-minute loss/latency test | Not completed |
| Cross-device timestamp mapping | Not implemented |
| Orientation estimate | To be calculated on laptop from accel + gyro; no magnetometer |
| Encrypted/bonded BLE recording mode | Not enabled in the current bench firmware |

## 10. UWB transient-invalid diagnostic — 28 September 2026

A subsequent 45-second capture with the six boards in their current close bench layout counted valid and invalid UWB attempts. An invalid attempt is encoded as `range_cm = -1`; it is a failed transaction, not a measured distance of minus one centimetre.

| Pair | Positions | Valid | Invalid |
|---:|---|---:|---:|
| 0 | HEAD ↔ PELVIS | 86 | 10 |
| 1 | HEAD ↔ LEFT-WRIST | 177 | 0 |
| 2 | HEAD ↔ RIGHT-WRIST | 191 | 0 |
| 3 | HEAD ↔ LEFT-ANKLE | 159 | 1 |
| 4 | HEAD ↔ RIGHT-ANKLE | 59 | 17 |
| 5 | PELVIS ↔ LEFT-WRIST | 16 | 34 |
| 6 | PELVIS ↔ RIGHT-WRIST | 202 | 0 |
| 7 | PELVIS ↔ LEFT-ANKLE | 175 | 0 |
| 8 | PELVIS ↔ RIGHT-ANKLE | 178 | 0 |
| 9 | LEFT-WRIST ↔ RIGHT-WRIST | 73 | 11 |
| 10 | LEFT-WRIST ↔ LEFT-ANKLE | 159 | 1 |
| 11 | LEFT-WRIST ↔ RIGHT-ANKLE | 181 | 0 |
| 12 | RIGHT-WRIST ↔ LEFT-ANKLE | 91 | 7 |
| 13 | RIGHT-WRIST ↔ RIGHT-ANKLE | 205 | 0 |
| 14 | LEFT-ANKLE ↔ RIGHT-ANKLE | 84 | 11 |

Pair 4 and pair 5 require controlled testing first. This result does not prove a hardware fault in any board: each of the four involved boards succeeds in other pairs. It points first to the specific physical radio path (very short separation, antenna orientation, nearby battery/wiring, or body/bench obstruction). Calibration corrects a bias on valid measurements; it cannot turn a failed exchange into a valid one.

The live viewer retains the last valid range, shows a separate invalid-attempt counter, and derives the displayed IMU sample rate from consecutive sequence numbers and timestamps. This keeps a single failed UWB attempt from erasing the last useful value on screen.

## 11. UWB forwarding and rate decision

### Current diagnostic implementation

The current PELVIS firmware forwards each UWB pair result immediately as one separate BLE notification:

```text
one UWB pair completes -> PELVIS receives/stores that pair result -> one BLE range packet to dongle
```

The packet contains the pair number, PELVIS timestamp, range in centimetres, and validity flags. It is not a 15-distance batch. This is deliberately useful during diagnosis because a failed pair is visible immediately and independently.

The firmware rate limits a valid result to at most 10 BLE reports per second **per pair**, and an invalid result to at most one per second per pair. Therefore, 150 small valid BLE notifications per second is only a theoretical maximum (`15 pairs x 10`); it is not the measured current rate. If 150 notifications were evenly spaced, the average interval would be 6.7 ms, not 16.7 ms.

The 45-second current-layout diagnostic measured between 16 and 205 valid reports per pair, depending on the link. The best observed links were about 4.5 valid reports per second. These results do not establish a clean full-matrix rate because invalid links and close bench geometry distort the schedule.

### Final recommended implementation

After controlled calibration and UWB reliability work, PELVIS should collect one complete scheduled matrix epoch and send one compact BLE **UWB matrix** packet containing:

- a matrix timestamp,
- all 15 signed centimetre distances,
- a 15-bit validity mask, and
- optional per-pair age/quality fields.

This reduces Bluetooth overhead from up to 150 individual notifications per second to about 10 matrix packets per second at the initial 10 Hz matrix target. A compact representation of 15 signed 16-bit distances plus a timestamp and validity mask fits comfortably within the current 100-byte BLE MTU. The diagnostic per-pair forwarding is retained until calibration is complete.

### Higher UWB rate potential

The DW3110 hardware is capable of substantially faster individual range exchanges than the current firmware. Qorvo's DW3xxx API guide contains both SS-TWR and DS-TWR examples, and Qorvo technical guidance states that specialised low-overhead schedules can exceed 1000 individual ranges per second. That does **not** mean this project should run 1000 ranges/s or 60 full 15-pair matrices/s: a wearable all-pair mesh has 15 serial radio paths, BLE forwarding, body obstruction, timing guard margins, and a range-accuracy requirement. [Qorvo DW3xxx API Guide](https://forum.qorvo.com/uploads/short-url/xD3TlXKvkujjdUaJWXv2b4E7GQN.pdf), [Qorvo rate guidance](https://forum.qorvo.com/t/dw3000-settings/20801)

The staged rate plan is therefore:

1. Make all links reliable and calibrate ranges at controlled distances.
2. Measure a complete-matrix rate, loss, latency, and power at 5 Hz.
3. Repeat at 8 Hz and 10 Hz.
4. Keep the highest rate that satisfies the pre-agreed validity, error, and loss limits. Do not target 60 Hz UWB until those measurements show it is both possible and useful.

## 12. Safe next procedure

1. Use the live viewer to confirm the six IMU positions and 15 UWB pair values change while the boards move.
2. Place boards at controlled known antenna-to-antenna distances and collect repeated samples for each pair.
3. Estimate a per-board antenna-delay correction and validate at more than one distance.
4. Run a stationary 10-minute loss and latency test.
5. Add timestamp clock mapping and the laptop orientation/fusion pipeline before motion-model training.

## 13. Flash safety record

The DWM boards were flashed one board at a time only after their J-Link serial number was checked. Each operation used sector erase, program, verify, and reset. The output explicitly reported `Write successful` and `Verify successful` for every board. This development-time flashing does not write application telemetry into flash at runtime. Firmware flashing should still be kept deliberate; the existing original board backups remain the restoration point if needed.

## 14. Gateway Bluetooth recovery record — 28 September 2026

### Symptom

Both wearable boards can remain powered and actively ranging (D20 and D9 flash; ICM LED is on), while the laptop receives no `GATEWAY_IMU` or `GATEWAY_UWB` lines. This is a **gateway BLE connection/scan state issue**, not evidence that either wearable has lost power or that UWB has stopped.

### Confirmed recovery sequence

1. Do **not** press the nRF52840 Dongle RESET button; that enters Nordic DFU/bootloader mode (red LED fades/pulses).
2. Unplug the Dongle from USB, wait three seconds, then plug it back in. This restarts the installed gateway application.
3. Wait ten seconds for the application to initialise.
4. Restart PELVIS, wait five seconds, then restart the remote wearable. This causes each peripheral to advertise again and lets the gateway reconnect.
5. Verify the data path in COM16: both `GATEWAY_IMU node=...` lines and `GATEWAY_UWB pair=...` lines must reappear before attempting calibration.

### Important operating rule

Do not use board LEDs as a repeated diagnostic once their initial power state has been confirmed. If the known LEDs are active but COM16 is silent, treat the issue as gateway BLE recovery first. Record this result rather than repeatedly asking for the same LED observations.

### Current engineering status

Gateway firmware v7 adds a five-second stale-link watchdog. It disconnects a silent BLE connection and restarts scanning automatically. The firmware was compiled and installed through the Nordic DFU bootloader on 28 September 2026. A clean post-update 30-second PELVIS–LEFT-WRIST test received both IMU streams and 59 valid UWB pair-5 reports, with zero watchdog events. This verifies normal automatic connection after the one required post-update board restart; deliberate stale-link recovery still needs a longer unattended test.

## 15. Controlled PELVIS–LEFT-WRIST UWB test status — 28 September 2026

The boards were held at a stated 100 cm separation with only PELVIS and LEFT-WRIST powered. The latest double-sided ranging test produced 59 valid reports in 30 seconds. The observed values were 126 cm minimum, 183 cm median, and 190 cm maximum.

This confirms that the full path is working: LEFT-WRIST UWB responder -> PELVIS UWB coordinator -> PELVIS BLE notification -> nRF52840 gateway -> COM16 laptop stream. It does **not** yet establish calibrated accuracy. The approximately +83 cm median error is a systematic range bias; the low readings also show that outlier handling is still required. Do not save this as an antenna-delay correction until it has been measured at multiple known distances and validated after applying a correction.

### Controlled-distance results after gateway recovery

| Known separation | Valid / invalid (45 s) | Median raw range | Standard deviation | Status |
|---:|---:|---:|---:|---|
| 50 cm | 95 / 3 | 80 cm | 3.2 cm | Stable but +30 cm biased |
| 75 cm | 100 / 1 | 98 cm | 4.3 cm | Stable but +23 cm biased |
| 100 cm, repeat | 100 / 1 | 183 cm | 22.8 cm | Unstable and +83 cm biased |

The 100 cm point was repeated without restarting or rotating either board and produced the same approximately 183 cm middle value. The three points cannot be described by one safe constant or linear calibration. Calibration is therefore paused. The next engineering task is to compare the custom double-sided ranging exchange and timestamp handling against Qorvo's reference DS-TWR implementation, then repeat the three-point test. The correction must not be stored in firmware until that verification passes.

## 16. Range-accuracy corrective change queued — 28 September 2026

Desktop review found that the current double-sided-ranging image configures the DW3110 PHY with `dwt_configure()` but omits the separate `dwt_configuretxrf()` call. Qorvo documents that this second call explicitly sets the transmit pulse-generator delay and transmit power. For channel 5, the documented settings are `PGdly = 0x34` and TX power `0xfdfdfdfd`.

The source has been amended to apply those documented channel-5 values immediately after `dwt_configure()`. This does not write the DW3110 OTP, does not change any antenna-delay or calibration constant, and does not log data to flash at runtime. It is a pending radio-configuration fix: PELVIS and LEFT-WRIST must each receive a newly built image, then the same 50/75/100 cm controlled test must be repeated before judging whether it solved the range bias.

Sources: [Qorvo DW3xxx API Guide — TX RF configuration, Tables 6–7](https://forum.qorvo.com/uploads/short-url/xD3TlXKvkujjdUaJWXv2b4E7GQN.pdf), [Qorvo DW3000 example structure](https://github.com/foldedtoad/dwm3000/blob/master/examples/ex_01h_simple_tx_pdoa/simple_tx_pdoa.c).

### Controlled-capture command

Use `hardware/tools/capture_uwb_calibration.ps1` for the 45-second PELVIS-to-LEFT-WRIST captures. It reads only the gateway USB serial stream, stores raw valid samples and invalid-attempt count in a JSON evidence file, and reports minimum, median, mean, standard deviation, and maximum. It does not write to either DWM board. Stop `live_six_node_viewer.ps1` first because only one program can open COM16 at a time.

### First capture with the explicit TX-RF fix

At the stated 100 cm PELVIS-to-LEFT-WRIST separation, the 45-second capture recorded 89 valid ranges and 3 invalid attempts. Its median was 91 cm and its standard deviation was 17.1 cm. The 9 cm median error is much smaller than the prior approximately +83 cm error, but the 17.1 cm variation is still too large to accept a calibration correction. The next two controlled distances must be captured before deciding whether the underlying exchange is stable.

Evidence: `hardware/outputs/dwm3001cdk_motion_node/2026-09-28_pelvis_left_wrist_txrf_fix/pelvis_left_wrist_100cm_txrf_fix.json`.

### Gateway recovery limitation found during calibration

Closing the live viewer/capture and reopening COM16 exposed an intermittent state where the nRF52840 gateway USB console remained available but emitted no IMU or UWB lines. Replugging the Dongle and restarting PELVIS followed by LEFT-WRIST restored the two BLE links. This is not an acceptable normal-operation requirement. Gateway v7's watchdog has verified a clean initial connection but has **not** yet proven automatic recovery from this post-flash/restart state. Plug-and-play status therefore remains unverified, and this must be fixed and regression-tested separately from UWB calibration.

Gateway v8 has been built and packaged, pending DFU installation. It sets an explicit eight-second outgoing BLE connection-attempt timeout and has its watchdog cancel any matching pending attempt. Zephyr documents `bt_conn_disconnect()` as the supported cancellation method for a pending outgoing connection; the gateway then releases that connection reference and resumes scanning. The package is `C:\tmp\COMP971-nRF52840-gateway-auto-recovery-v8.zip` (SHA-256 `4903C405069689C11F25DA505EE5E1428AF156831A509AA2B73F32B23EA565A3`). This is a recovery improvement, not a claim that the behaviour has passed a live regression test.
# 17. Bluetooth recovery correction — v9 outcome and v10 automatic recovery (2026-09-28)

## Verified v9 result

v9 did not reconnect PELVIS automatically after a gateway restart.  A controlled
test showed that rebooting LEFT-WRIST made it appear immediately, proving that
gateway scanning and a fresh wearable boot work.  PELVIS remained stuck in a
dead BLE state, so the normal disconnect request alone is not reliable enough.

## v10 wearable correction

The two wearable images now use a 10-second gateway-heartbeat timeout.  If a
connected board receives no heartbeat for 10 seconds, it performs an automatic
software reboot.  That clears the stale BLE state and makes the board advertise
again.  This is a normal application reboot; it does not perform runtime flash
writes or change UWB/ICM settings.  The gateway firmware remains v9.

## Installed set

The v9 firmware set was installed and verified on 2026-09-28:

- DWM-02-PELVIS — J-Link 760224787
- DWM-03-LEFT-WRIST — J-Link 760225868
- nRF52840 Dongle — gateway package `COMP971-nRF52840-gateway-heartbeat-v9.zip`

The next required evidence is the recovery test below: both battery-powered boards
must reappear automatically after the dongle reboots, without unplugging or
restarting either board.

## What the v8 test proved

Gateway v8 was installed and tested while PELVIS and LEFT-WRIST remained powered.
After the dongle restarted, the live viewer received no telemetry after 30 seconds.
Therefore the gateway-only reconnect change was **not sufficient**: a wearable can
continue to believe that its old Bluetooth connection exists after the gateway has
restarted.  v8 is superseded; it must not be described as plug-and-play.

## v9 correction

v9 uses a small two-way Bluetooth liveness check:

- The gateway writes one 1-byte heartbeat to every subscribed wearable once per
  second.
- PELVIS and each wearable expect a heartbeat within seven seconds.
- If that heartbeat stops, the wearable deliberately drops the stale connection
  and starts advertising again.
- The gateway scan/reconnect logic then reconnects and re-subscribes.

This uses ordinary RAM timers and normal BLE GATT writes.  It does not save anything
to on-chip flash while the system is running.

## Prepared firmware set

The three matching firmware images have been built before any new hardware flash:

| Device | Firmware role | Build output |
|---|---|---|
| DWM-02-PELVIS (760224787) | UWB node + BLE heartbeat peripheral | `C:\\tmp\\comp971_ds_txrf_heartbeat_pelvis_build_20260928_v1\\zephyr\\zephyr\\zephyr.hex` |
| DWM-03-LEFT-WRIST (760225868) | UWB node + BLE heartbeat peripheral | `C:\\tmp\\comp971_ds_txrf_heartbeat_left_wrist_build_20260928_v1\\zephyr\\zephyr\\zephyr.hex` |
| nRF52840 Dongle | BLE gateway heartbeat central | `C:\\tmp\\COMP971-nRF52840-gateway-heartbeat-v9.zip` |

The PELVIS/LEFT-WRIST controlled-distance test remains pending until this recovery
test passes.  No calibration coefficient has been applied from the earlier noisy
100 cm capture.
