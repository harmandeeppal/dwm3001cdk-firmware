# Direct official FiRa task-runtime correction

## Why this build exists

The direct-FiRa v2 two-board test was run with PELVIS and HEAD on battery at a
stated 100 cm separation. BLE and both ICM-20948 streams were live, but:

| Observation | Result |
|---|---:|
| PELVIS UWB attempts | 0 |
| PELVIS valid ranges | 0 |
| PELVIS invalid ranges | 0 |
| UWB setup/failure events | 0 |
| Viewer node status | `ICM ready; UWB not started` on both boards |

This proves that no range exchange started. It is **not** an antenna-delay,
distance, J20, or UWB-radio-accuracy result.

## What the official SDK comparison found

Qorvo's DWM3001CDK QANI FiRa reference creates its FiRa task with:

| Item | Qorvo reference | COMP971 v2 |
|---|---|---|
| Task stack | 4,096 bytes | 1,536 bytes |
| Task wrapper | Qorvo `qthread_create` | raw dynamic `xTaskCreate` |
| Priority | Qorvo normal priority | FreeRTOS priority 2 |

The Qorvo source is
`C:\COMP971_QORVO_SDK_1_1_1\Projects\FreeRTOS\QANI\Common\src\fira\fira_niq.c`.
It defines `QANI_TASK_STACK_SIZE_BYTES` as 4096 and creates the task with
`qthread_create(..., QTHREAD_PRIORITY_NORMAL)`.

The v2 result is consistent with the direct-FiRa startup task failing to
complete in its smaller stack. The FreeRTOS configuration has stack-overflow
detection disabled, so this can leave no returned error record. This is a
strong, source-backed explanation, but it remains a hypothesis until v3 is
tested.

## v3 correction

The shared COMP971 source now creates the direct-FiRa task using the official
Qorvo task wrapper, Qorvo normal priority, and a dedicated 4,096-byte stack.
It also sends five non-failure progress records through the existing PELVIS BLE
telemetry path:

| Code | Meaning |
|---:|---|
| `C1` | FiRa task entered |
| `C2` | FiRa setup entered |
| `C3` | FiRa setup completed |
| `C4` | UWB-MAC started |
| `C5` | FiRa session started |

The live viewer identifies these as setup progress, not failures. Codes
`D1`–`D9` remain actual startup-failure records.

No radio session setting, antenna delay, OTP setting, UICR value, calibration
value, flash-storage policy, BLE packet format, ICM behaviour, or gateway
firmware is changed.

## Build verification

| Board | Role | Build path | SHA-256 | Build result |
|---|---|---|---|---|
| PELVIS `760224787` | controller | `C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_PELVIS_v3\DWM3001CDK-COMP971-FreeRTOS.hex` | `2795C7BC9E46FE23E374F2463713133D47AB44F582A15CE7E99D640E62F6FEAB` | passed |
| HEAD `760225805` | responder | `C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_HEAD_v3\DWM3001CDK-COMP971-FreeRTOS.hex` | `56918ED8A87CD54E088F05E3C5322B4478EFAA6E62AE648FD6F9C7BE24F53F88` | passed |

Both builds fit within the DWM3001CDK target memory budget: PELVIS uses
70,120 B of 112 KiB RAM and 338,712 B of 504 KiB application flash; HEAD uses
70,120 B RAM and 337,608 B application flash. The 4 KB FiRa stack is allocated
from RAM at startup, not flash.

## Flash and test boundary

Both v3 images are programmed and verified. No further flash action is needed
before the controlled two-board test. The images were programmed through J9
using normal application programming only; no chip erase, UICR write, OTP
write, antenna-delay write, calibration write, or runtime-flash write was
issued.

### PELVIS v3 programming record — complete

- Board: DWM-02 PELVIS; J-Link serial `760224787`.
- Image SHA-256 verified before programming:
  `2795C7BC9E46FE23E374F2463713133D47AB44F582A15CE7E99D640E62F6FEAB`.
- J-Link result: **Program & Verify succeeded**.
- Affected normal application flash: one range totalling `229,376` bytes at
  `0x00000000`.
- No chip erase command, UICR write, OTP write, antenna-delay write,
  calibration write, NVM write, or runtime-flash write was issued.

### HEAD v3 programming record — complete

- Board: DWM-01 HEAD; J-Link serial `760225805`.
- Image SHA-256 verified before programming:
  `56918ED8A87CD54E088F05E3C5322B4478EFAA6E62AE648FD6F9C7BE24F53F88`.
- J-Link result: **Program & Verify succeeded**.
- Affected normal application flash: one range totalling `225,280` bytes at
  `0x00000000`.
- No chip erase command, UICR write, OTP write, antenna-delay write,
  calibration write, NVM write, or runtime-flash write was issued.

After both are programmed, test them on battery at 100 cm with the existing
nRF52840 dongle and gateway viewer. The first acceptance check is `C1` through
`C5`, followed by a non-zero PELVIS attempt count and range records for pair 0
(`HEAD <-> PELVIS`).
