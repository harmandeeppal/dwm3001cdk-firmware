# UWB platform-before-BLE candidate

## Status

Built and static-checked on 3 October 2026. **Not installed on any board.**

## What the preceding 4 KiB task test proved

The PELVIS–HEAD battery test at the stated 100 cm position reported both
`ICM ready; UWB started`. This is the first direct-FiRa pilot that reached a
running UWB session without the earlier processor fault.

It did **not** produce a distance. PELVIS recorded one attempt, zero valid
results, and one invalid result. The direct-FiRa diagnostic reported status
`32` (`0x20`). In the supplied Qorvo SDK
`quwbs/fbs/defs.h`, this is `QUWBS_FBS_STATUS_RANGING_TX_FAILED`: the PELVIS
controller failed to transmit a UWB packet. It is not a 100 cm measurement,
an antenna-delay value, a calibration result, or a Bluetooth failure.

The same session carried 3,516 HEAD and 3,564 PELVIS IMU records, with zero
new gateway USB drops. BLE/IMU therefore remained alive while UWB began.

## Source correction under test

Qorvo's `Projects/FreeRTOS/QANI/Common/src/main.c` calls
`CreateQaniTask()` before its BLE startup. `CreateQaniTask()` calls
`fira_uwb_mcps_init()` before BLE because the UWB platform/L1 setup may need
to run at that point.

The preceding COMP971 pilot instead called this platform initialisation inside
the delayed UWB task, after BLE had started. The candidate separates the two
operations:

| Step | Previous installed 4 KiB pilot | Candidate |
|---|---|---|
| UWB platform, L1 and UWB-MAC preparation | 45 seconds after Bluetooth starts | Before Bluetooth starts, matching Qorvo QANI ordering |
| FiRa session creation and start | After 45 seconds | After 45 seconds, unchanged |
| IMU | 60 Hz BLE stream | Unchanged |
| Radio channel/session/address settings | Existing values | Unchanged |
| UICR, OTP, antenna delay, calibration | Not written | Not written |

This ordering is a plausible correction for a local `TX_FAILED` result, but
it is not accepted as a fix until the two boards are tested.

## Candidate images

| Board | Role | Image | SHA-256 | Static RAM (`data + bss`) | Static flash (`text + data`) |
|---|---|---|---|---:|---:|
| PELVIS `760224787` | controller | `C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_PELVIS_v5\DWM3001CDK-COMP971-FreeRTOS.hex` | `93416D2320AA1DF8D49688F8B9F2063085565119CC9451FD22AE6F75F3877645` | 69,828 bytes | 340,528 bytes |
| HEAD `760225805` | responder | `C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_HEAD_v5\DWM3001CDK-COMP971-FreeRTOS.hex` | `B9E443095296483DF847A3871346AA49AC30DFC13EE4A29D9DD2C198019A05E5` | 69,828 bytes | 339,392 bytes |

## Safety check

- Both images compile successfully from the checked-in COMP971 source.
- The only linked `qflash_write` implementation is the local RAM-only guard
  in `Common/src/comp971_qflash_ram_guard.c`; it increments a RAM counter and
  returns success without calling any erase or write API.
- `arm-none-eabi-nm` found no linked
  `nrf_nvmc_page_erase`, `nrf_nvmc_words_write`, `sd_flash_page_erase`, or
  `sd_flash_write` symbol in either executable.
- Neither candidate has been programmed. Any future normal J-Link `loadfile`
  will erase only the application pages required for the new image; it will
  not run a chip erase.

## Next protected test

1. Obtain explicit approval to install PELVIS only, verify every image byte
   by independent readback, and confirm UICR unchanged.
2. Repeat for HEAD only after PELVIS is verified.
3. Test the matched pair on battery at a controlled distance with the gateway.
