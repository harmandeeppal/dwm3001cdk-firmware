# Official-Qorvo FiRa two-node UWB pilot

## Purpose

This is a controlled first UWB test between exactly two registered boards:

| Board | J-Link serial | UWB role | Short address | Result destination |
|---|---:|---|---:|---|
| PELVIS | 760224787 | Controller / initiator | 0 | BLE gateway, pair 0 |
| HEAD | 760225805 | Controlee / responder | 1 | PELVIS receives the UWB result |

Pair 0 means `HEAD <-> PELVIS`. PELVIS starts each official FiRa deferred
double-sided two-way-ranging exchange and therefore receives the result. HEAD
responds directly over UWB; PELVIS does not relay the radio exchange.

## Images

| File | SHA-256 | Build identity |
|---|---|---|
| `DWM-02-PELVIS__official-fira-controller__760224787.hex` | `E5CC5C3722F08948B44C531F7DA335784B7608688B2D4F24B690A99BE934190B` | node 1, `COMP971-MOTION-PELVIS`, UWB role 1 |
| `DWM-01-HEAD__official-fira-responder__760225805.hex` | `C70956BABF9DA80FCCFF7F56DD570240AE0236CC3B95B7DEAD507E4FFFD28125` | node 0, `COMP971-MOTION-HEAD`, UWB role 2 |
| `DWM-02-PELVIS__official-fira-controller-v2__760224787.hex` | `353B39189A9EF94166DA39EEE3A6B73509780D1ADC3BF2D4931EC67AD66CBDDB` | node 1, controller, corrected scheduler-start order |
| `DWM-01-HEAD__official-fira-responder-v2__760225805.hex` | `B5B24AD655158B789E84FBBF3D1970C850A27E02945CA281EC155C5BCCCAC8D6` | node 0, responder, corrected scheduler-start order |
| `DWM-02-PELVIS__official-fira-controller-v3__760224787.hex` | `1A2ADB606F421E4279215E843E0D7E83AE8629F0D7E3C6D760D85D1103922525` | node 1, controller, QANI `.rconfig` alignment and exact setup diagnostics |
| `DWM-01-HEAD__official-fira-responder-v3__760225805.hex` | `97882286BD7D46E5F907F1F4B6FA48512016F8BF231AC15D981B998AF413E253` | node 0, responder, QANI `.rconfig` alignment and exact setup diagnostics |

The first images are retained as traceable build records but are **not** the
acceptance images. Their UWB start notification occurred before the FreeRTOS
scheduler began, so the UWB task could miss it. Version 2 removes that signal:
the UWB task configures and starts its official FiRa session from inside the
running scheduler, and retries safely if setup fails. This changes only the
application start order; it does not change radio calibration, UICR, OTP, or
the factory antenna-delay values.

## What changed from the accepted IMU/BLE pilot

| Area | Earlier accepted pilot | This pilot |
|---|---|---|
| ICM-20948 | 60 Hz raw accelerometer/gyroscope batches | Unchanged |
| Bluetooth name and gateway service | Existing COMP971 service | Unchanged |
| UWB | Not started | Official Qorvo FiRa/UWB-MAC two-board session |
| UWB settings | None | Deferred DS-TWR, channel 9, SP3, code 11, one peer |
| UWB rate | None | One 200 ms FiRa block: nominally 5 attempts/second for this single pair |
| Laptop result | None | PELVIS sends pair-0 range records over its existing BLE link |

## Official code used

The application follows the lifecycle in Qorvo QM33 SDK 1.1.1:

- `Projects/FreeRTOS/QANI/Common/src/fira/fira_niq.c`
- `Src/Apps/Src/fira/fira_app.c`
- Qorvo FiRa/UWB-MAC library target `fira`

COMP971 adds only role/address selection and conversion of the official ranging
callback to the already deployed COMP971 BLE packet format. The low-level radio
driver, UWB-MAC, FiRa session API, and existing board calibration region remain
owned by the Qorvo SDK.

## Safety boundaries

- Normal J-Link `loadfile` programming only; no chip erase command.
- No UICR write.
- No OTP write.
- No antenna-delay or calibration write.
- No runtime application flash logging.
- `CONFIG_CFG_MANAGER_VOLATILE_MODE` remains enabled; the session configuration
  is held in RAM only.
- The current PELVIS recovery point remains
  `hardware/backups/DWM3001CDK_760224787/2026-10-01_pre_comp971_icm_ble_pilot/`.

## Required acceptance test

1. Flash PELVIS while it is on J9, using serial `760224787`.
2. Flash HEAD while it is on J9, using serial `760225805`.
3. Power both from battery, place them 100 cm apart, and run the unified viewer
   with a 20-second warm-up.
4. Confirm PELVIS and HEAD remain live at 60 Hz IMU and that pair 0 reports
   valid UWB distances near 100 cm. The result is not considered calibrated
   until controlled captures at 50, 75, and 100 cm have been assessed.

## Scope limit

This is not the six-node, 15-pair mesh yet. It proves the official two-node
radio path, result forwarding, and coexistence with ICM/BLE first. The same
controller scheduling layer will later assign all 15 pair slots.

## Version 2 programming plan

1. Program PELVIS serial `760224787` with the v2 controller image using normal
   J-Link `loadfile` only.
2. Program HEAD serial `760225805` with the v2 responder image the same way.
3. Read back the application flash and compare every image-defined byte.
4. With both boards on battery at 100 cm, confirm status `ICM ready; UWB
   started` on both boards and observe pair 0 at the BLE gateway.

Neither v2 operation includes chip erase, UICR/OTP access, antenna calibration,
or a runtime flash write.

## Version 2 acceptance result and version 3 correction

The 2026-10-02 acceptance test at a stated 100 cm did not reach radio ranging.
PELVIS stayed live over BLE at 58.8 Hz, but the official FiRa session did not
start: `0` UWB attempts and repeated generic setup failures. Therefore this is
not a distance, battery, or antenna-calibration result.

Version 3 makes the pilot closer to Qorvo's QANI implementation:

- its session parameter block is in Qorvo's `.rconfig` RAM section;
- its single-pair settings match the QANI configuration, including a 100 ms
  block duration;
- it makes one setup attempt per boot, avoiding repeated allocation after a
  failed setup;
- it forwards the exact failed official API stage and Qorvo error code through
  the existing BLE diagnostics if setup still fails.

Version 3 does not write calibration, UICR, OTP, or runtime flash.

## Version 4 PELVIS diagnostic image

The v3 diagnostic was produced before the BLE gateway had subscribed, so its
single setup-failure event was not delivered. The temporary PELVIS v4 image
keeps that one stage/error result in RAM and sends it after BLE notifications
become available. It does not retry the FiRa setup, alter the radio parameters,
or write any runtime flash. HEAD remains on v3 for this diagnostic-only step.

| File | SHA-256 |
|---|---|
| `DWM-02-PELVIS__official-fira-controller-v4-diagnostic__760224787.hex` | `186C33F8E4211ABD10A3C8810A9497785FB07AB2433C55E8A40F47291133605C` |

## Programming record

### PELVIS controller - version 5 official-QANI lifecycle baseline

- Board / J-Link serial: `DWM-02-PELVIS` / `760224787`.
- Programmed: 2026-10-02 using `flash_pelvis_controller_v5_qani_baseline.jlink`.
- This replaces the direct FiRa startup task with Qorvo's own QANI
  `persistent_time_init`, `niq_init`, `CreateQaniTask`, and signalled FiRa
  lifecycle. COMP971 retains only fixed PELVIS identity/address selection and
  its 2-second post-scheduler start request.
- J-Link reported normal program-and-verify success; one application-flash
  range (`229,376` bytes) was affected, not a full-chip erase.
- Post-program read-back: `341,120` image-defined HEX bytes compared; `0`
  mismatches. The read-back file is
  `pelvis_controller_v5_qani_postflash_0x00000000_0x00053fff.bin`.
- No UICR, OTP, antenna delay, calibration, or runtime flash data was written.

### PELVIS controller - version 6 official-QANI telemetry

- Board / J-Link serial: `DWM-02-PELVIS` / `760224787`.
- Programmed: 2026-10-02 using `flash_pelvis_controller_v6_qani_telemetry.jlink`.
- Keeps the v5 official QANI lifecycle. The only new behaviour is an
  application reporting hook from Qorvo's QANI callback to the existing BLE
  telemetry format. PELVIS forwards pair `0` (`HEAD <-> PELVIS`) to the laptop.
- SHA-256: `D7DFEDE933E6E2FCF675C96109987C41339B61172689DD0DCE599C11676E25DC`.
- J-Link program-and-verify succeeded. Post-program read-back: `342,136`
  image-defined HEX bytes compared; `0` mismatches. The read-back file is
  `pelvis_controller_v6_qani_postflash_0x00000000_0x000543ff.bin`.
- One application-flash range (`233,472` bytes) was affected. No full-chip
  erase, UICR, OTP, antenna delay, calibration, or runtime flash write occurred.

### HEAD responder - version 6 official-QANI telemetry

- Board / J-Link serial: `DWM-01-HEAD` / `760225805`.
- Programmed: 2026-10-02 using `flash_head_responder_v6_qani_telemetry.jlink`.
- It matches PELVIS v6 in QANI lifecycle and reporting support; only its fixed
  UWB role and short address differ (HEAD responder address `1`).
- SHA-256: `4618304B293395DD202309CBA0BF4862396DD74EB4599F2314FEDF01C590ECCC`.
- J-Link program-and-verify succeeded. Post-program read-back: `341,288`
  image-defined HEX bytes compared; `0` mismatches. The read-back file is
  `head_responder_v6_qani_postflash_0x00000000_0x000540ff.bin`.
- One application-flash range (`233,472` bytes) was affected. No full-chip
  erase, UICR, OTP, antenna delay, calibration, or runtime flash write occurred.

### HEAD responder - version 5 official-QANI lifecycle baseline

- Matching responder build completed on 2026-10-02; it uses the same QANI
  lifecycle, with only the fixed local role/address changed to HEAD responder
  (`short address 1`, destination PELVIS `0`).
- Image prepared but not yet programmed:
  `DWM-01-HEAD__official-qani-lifecycle-baseline-v5__760225805.hex`.
- SHA-256:
  `80109ECBCEC3CC7E006C31DC359D412F6387892A86AC71EA8E39B00F52EF7BB6`.

### PELVIS controller - version 4 diagnostic

- Board / J-Link serial: `DWM-02-PELVIS` / `760224787`.
- Programmed: 2026-10-02 using `flash_pelvis_controller_v4_diagnostic.jlink`.
- J-Link reported normal program-and-verify success; one application-flash
  range (`225,280` bytes) was affected, not a full-chip erase.
- Post-program read-back: `336,904` image-defined HEX bytes compared; `0`
  mismatches. The read-back file is
  `pelvis_controller_v4_diagnostic_postflash_0x00000000_0x00053fff.bin`.
- This temporary image changes only diagnostic delivery in RAM. It does not
  write UICR, OTP, antenna delay, calibration, or runtime flash data.
- Battery/dongle check, 2026-10-02: BLE and the ICM stream operated normally
  (`1,200` IMU samples at `58.8 Hz` in 20 seconds), but PELVIS reported UWB
  not started with zero attempts and no retained setup-stage message. This
  means the diagnostic did not get past the first blocking UWB setup operation;
  it is not evidence of a radio, distance, or antenna problem. The next step
  is a read-only SWD debugger inspection of the active PELVIS task.
- Read-only SWD inspection, 2026-10-02: PELVIS was stopped in the Nordic
  SoftDevice fault handler with fault ID `0x01` (a SoftDevice assertion) at
  program counter `0x00012214`. This was a software integration fault, not a
  UWB range failure. Review against Qorvo's unmodified QANI example found that
  the early COMP971 port had bypassed QANI's required startup sequence:
  `persistent_time_init(0)`, `niq_init(...)`, and QANI's own `CreateQaniTask()`
  / signal-driven start path. The direct-start pilot must be replaced with that
  official lifecycle before another distance test.

### PELVIS controller - version 3

- Board / J-Link serial: `DWM-02-PELVIS` / `760224787`.
- Programmed: 2026-10-02 using `flash_pelvis_controller_v3.jlink`.
- J-Link reported normal program-and-verify success; one application-flash
  range (`225,280` bytes) was affected, not a full-chip erase.
- Post-program read-back: `336,840` image-defined HEX bytes compared; `0`
  mismatches.

### HEAD responder - version 3

- Board / J-Link serial: `DWM-01-HEAD` / `760225805`.
- Programmed: 2026-10-02 using `flash_head_responder_v3.jlink`.
- J-Link reported normal program-and-verify success; one application-flash
  range (`225,280` bytes) was affected, not a full-chip erase.
- Post-program read-back: `336,120` image-defined HEX bytes compared; `0`
  mismatches.
- Neither v3 programming operation wrote UICR, OTP, antenna delay,
  calibration, or runtime flash data.

### PELVIS controller - version 2

- Board / J-Link serial: `DWM-02-PELVIS` / `760224787`.
- Programmed: 2026-10-02 using `flash_pelvis_controller_v2.jlink`.
- J-Link reported normal program-and-verify success.
- J-Link affected one application-flash range (`225,280` bytes). This was not
  a full-chip erase.
- Post-program read-back: `336,112` image-defined HEX bytes compared; `0`
  mismatches. The read-back file is
  `pelvis_controller_v2_postflash_0x00000000_0x00053fff.bin`.
- No UICR, OTP, antenna-delay, calibration, or runtime flash write was used.

### HEAD responder - version 2

- Board / J-Link serial: `DWM-01-HEAD` / `760225805`.
- Programmed: 2026-10-02 using `flash_head_responder_v2.jlink`.
- J-Link reported normal program-and-verify success.
- J-Link affected one application-flash range (`225,280` bytes). This was not
  a full-chip erase.
- Post-program read-back: `335,408` image-defined HEX bytes compared; `0`
  mismatches. The read-back file is
  `head_responder_v2_postflash_0x00000000_0x00053fff.bin`.
- No UICR, OTP, antenna-delay, calibration, or runtime flash write was used.

### PELVIS controller

- Board / J-Link serial: `DWM-02-PELVIS` / `760224787`.
- Programmed: 2026-10-01 using `flash_pelvis_controller.jlink`.
- J-Link reported normal program-and-verify success.
- J-Link affected one application-flash range (`225,280` bytes). This was not
  a full-chip erase.
- Post-program read-back: `336,176` image-defined HEX bytes compared; `0`
  mismatches. The read-back file is
  `pelvis_controller_postflash_0x00000000_0x00053fff.bin`.
- The two-board range result is still **not tested** until HEAD has the matching
  responder image and both boards run together.

### HEAD responder

- Board / J-Link serial: `DWM-01-HEAD` / `760225805`.
- Programmed: 2026-10-01 using `flash_head_responder.jlink`.
- J-Link reported normal program-and-verify success.
- J-Link affected one application-flash range (`225,280` bytes). This was not
  a full-chip erase.
- Post-program read-back: `335,472` image-defined HEX bytes compared; `0`
  mismatches. The read-back file is
  `head_responder_postflash_0x00000000_0x00053fff.bin`.
