# UWB task-stack remediation pilot

## Status

Built and static safety-checked on 3 October 2026. **PELVIS installed and
HEAD installed and independently verified.**

## Evidence from the BLE-first battery test

The 50 cm PELVIS–HEAD battery test delivered zero gateway packets. PELVIS was
then inspected through J9 without changing flash. Its processor fault status
contained a forced fault (`HFSR = 0x40000000`) and the divide-by-zero UsageFault
flag (`CFSR = 0x01000000`). This is a processor fault, not a Bluetooth range,
UWB distance, antenna-delay, or calibration result.

## Cause addressed

The installed BLE-first pilot called the complete official FiRa setup from the
existing status task, which has a 256-word (1 KiB) stack. Qorvo's own FiRa
reference uses a dedicated 4,096-byte task stack. The COMP971 source now uses
the same 4 KiB size in a dedicated `comp971uwb` task. BLE status and the 60 Hz
IMU task remain independent.

The UWB task waits 45 seconds after boot before beginning the UWB setup. This
deliberate diagnostic delay gives the BLE gateway enough time to show ongoing
BLE/IMU telemetry before UWB begins. It does not change UWB radio settings.

## Safety boundary

- No UICR, OTP, antenna delay, radio calibration, or runtime flash value is
  written by the application.
- The RAM-only `qflash_write` guard remains linked.
- Static symbol inspection found the local `qflash_write` guard and found none
  of `nrf_nvmc_page_erase`, `nrf_nvmc_words_write`, `sd_flash_page_erase`, or
  `sd_flash_write` in either candidate.

## Candidate images

| Board | J-Link serial | HEX SHA-256 | RAM static use | Flash static use |
|---|---:|---|---:|---:|
| PELVIS | `760224787` | `A011B39A3FA8F7D22F4AA24B5E00669584FB40514491FC7E947F99354EB763E0` | 69,824 / 114,688 bytes (60.88%) | 337,792 / 516,096 bytes (65.45%) |
| HEAD | `760225805` | `A08839F405FA42A1EA4E9D4A74B0974F59D22CE40D11143CFB51572332951123` | 69,824 / 114,688 bytes (60.88%) | 336,640 / 516,096 bytes (65.23%) |

## PELVIS installation — complete

On 3 October 2026, the operator approved the PELVIS-only installation while
J9 was connected. A new, duplicate full-flash and UICR backup was first taken
at `../../../../backups/DWM3001CDK_760224787/2026-10-03_pre_uwb_task_stack_fix/`.
Both full-flash reads matched the already-recorded BLE-first image, and both
UICR reads matched.

J-Link then programmed the PELVIS candidate normally (`loadfile`), which
erases only the application pages required by that image. It did **not** run
J-Link's `EraseChip` command. A separate, read-only 512 KiB target readback
compared every 336,320 HEX-defined byte with the PELVIS candidate:

| Check | Result |
|---|---|
| J-Link serial | `760224787` |
| Defined-byte mismatches | `0` |
| UICR unchanged | `true` |
| Post-program full-flash SHA-256 | `70F56A02EB1D972580883259DE03E4CD2D64D744978AB0486743E07317681009` |
| Post-program UICR SHA-256 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |

No UICR, OTP, antenna-delay, calibration, or runtime-flash value was changed.

## HEAD installation — complete

On 3 October 2026, after the duplicate read-only recovery backup and explicit
operator approval, J-Link programmed the HEAD candidate normally (`loadfile`).
It erased only the application pages required by that image; no `EraseChip`
command was run. A separate read-only 512 KiB target readback then compared
every HEX-defined byte with the HEAD candidate.

| Check | Result |
|---|---|
| J-Link serial | `760225805` |
| Defined-byte mismatches | `0` of `335,168` |
| UICR unchanged | `true` |
| Post-program full-flash SHA-256 | `BCE1BD940E8F3161D2E96558B6FD2020FD82DE4FCAB746DB846FBE07E394DEAE` |
| Post-program UICR SHA-256 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |

No UICR, OTP, antenna-delay, calibration, or runtime-flash value was changed.

## Required next steps

1. Disconnect HEAD from J9. Put PELVIS and HEAD on battery at a measured
   100 cm separation, with the nRF52840 dongle connected to the laptop.
2. Wait 10 seconds, then run the live gateway viewer. Allow at least 45 seconds
   before expecting UWB startup. BLE and IMU should appear before that point.
