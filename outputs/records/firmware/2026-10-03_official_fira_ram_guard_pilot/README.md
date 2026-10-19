# Official-FiRa RAM-guard two-board pilot

Date: 3 October 2026  
Status: build and static safety review complete; **not flashed**.

## Purpose

This is the next minimal PELVIS--HEAD battery UWB pilot. It uses Qorvo's
normal FiRa/UWB-MAC/DW3110 application lifecycle to establish one official
controller-to-responder range session. It is not the final six-node mesh.

## What is Qorvo-owned and what is COMP971-owned

| Layer | Owner | Change in this pilot |
|---|---|---|
| DW3110 radio, SPI, ranging timing, UWB-MAC, FiRa | Qorvo QM33 SDK 1.1.1 | None |
| Factory OTP reads and board calibration | Qorvo SDK / factory | None; no value is written or overridden |
| BLE transport and external ICM-20948 telemetry | COMP971 | Existing compatible telemetry only |
| PELVIS/HEAD fixed identities and result callback | COMP971 | Minimal two-board adapter |
| L1 configuration persistence | COMP971 safety boundary | RAM-only guard; no non-volatile write is allowed |

## Why the RAM-only guard is necessary

The SDK source has a `CONFIG_L1_CONFIG_VOLATILE` option which retains L1
configuration in RAM. However, the SDK's supplied `uwbstack_core` library is
already compiled, so defining that macro only in this application would not
change its internal persistence call. The package contains no volatile prebuilt
variant.

The local composition removes only the NRFX `qflash.c` source from the
application's QHAL target and replaces `qflash_write()` with
`Common/src/comp971_qflash_ram_guard.c`. It increments a RAM counter and
returns `QERR_SUCCESS`, matching the official volatile-mode success path.
It does not call NVMC, SoftDevice flash, erase, UICR, OTP, or calibration APIs.

## Exact candidate images

| Board | J-Link serial | Role | HEX SHA-256 | Build output |
|---|---:|---|---|---|
| PELVIS | 760224787 | controller/initiator | `84A5CC1CE3AB994024FC805781CE6A48735BB3053A8DB2EB533EF3A8FF51D6AF` | `C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_PELVIS_v3\DWM3001CDK-COMP971-FreeRTOS.hex` |
| HEAD | 760225805 | responder | `A0DF1C27DF6802153D0822CA81128282885A4AC539F7D775DCE254A71D46C93B` | `C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_HEAD_v3\DWM3001CDK-COMP971-FreeRTOS.hex` |

The historical `v3` folder names do not identify the image. The SHA-256
values above do.

## Static safety evidence

Both images compiled successfully.

| Board | RAM used | Flash used | Link-image evidence |
|---|---:|---:|---|
| PELVIS | 69,824 B / 112 KiB (60.88%) | 337,696 B / 504 KiB (65.43%) | `qflash_write()` only increments a RAM counter and returns; no page-erase/program API is linked |
| HEAD | 69,824 B / 112 KiB (60.88%) | 336,576 B / 504 KiB (65.22%) | same result |

The final ELF files have no `nrf_nvmc_page_erase`, `nrf_nvmc_words_write`,
`sd_flash_page_erase`, or `sd_flash_write` symbol. The application HEX files
contain normal application flash records only; they contain no UICR address.

## Controlled installation and test order

1. **PELVIS only:** make two fresh full-flash reads and two UICR reads from
   serial `760224787`; compare each duplicate pair. Do not chip erase.
2. Program only the named PELVIS HEX with J-Link `loadfile`, then perform an
   independent read-back comparison against the exact HEX.
3. Check that PELVIS still advertises and sends BLE/ICM telemetry. This proves
   the normal wearable application did not regress.
4. Repeat steps 1--3 for HEAD serial `760225805` only after PELVIS evidence
   is clean.
5. With both on battery, 100 cm apart, run the unified gateway viewer for
   60 seconds. Do not judge range accuracy until PELVIS reports non-zero UWB
   attempts and valid result callbacks.

## Expected first viewer evidence

For the first two-board range test, the gateway must show:

- PELVIS and HEAD IMU streams near 60 Hz;
- a PELVIS lifecycle diagnostic `C5` (session started);
- a PELVIS guard diagnostic `C0`; its count may be zero or higher, but it
  confirms the RAM-only audit path is available;
- `HEAD <-> PELVIS` UWB attempts greater than zero; and
- at least one valid or explicit invalid UWB result.

If attempts remain zero, stop. That is a session/control-path issue, not a
distance, antenna-delay, battery, Bluetooth, J9, or J20 problem.

## PELVIS installation — complete

PELVIS / DWM-02 / J-Link serial `760224787` was programmed only after the
fresh duplicate read-only backup in
`hardware/backups/DWM3001CDK_760224787/2026-10-03_pre_official_fira_ram_guard_pilot/`.

J-Link selected the named serial and reported one normal application flash
range of 225,280 bytes. This required normal page erasure inside the affected
application range; it was **not** a chip erase. No UICR, OTP, antenna-delay,
or calibration address was included in the HEX file or programmed.

| Check | Result |
|---|---|
| Approved PELVIS HEX SHA-256 | `84A5CC1CE3AB994024FC805781CE6A48735BB3053A8DB2EB533EF3A8FF51D6AF` |
| J-Link program and verify | passed |
| Independent 512 KiB post-flash read SHA-256 | `1F23D95D79279D8962F556417A5635C87F58A73ADBF0D1A49F2EFCC1858D7DF3` |
| Defined Intel-HEX application bytes compared | 336,224 |
| Defined-byte mismatches | 0 |
| Pre/post UICR SHA-256 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |
| UICR changed | no |

The initial local comparison falsely reported a mismatch because the parser
did not process Intel-HEX type-02 segment-address records. The corrected
comparison processes both type-02 and type-04 addressing; it reports zero
mismatches. The saved full read-back and its J-Link script are alongside this
record.

PELVIS was reset and run after programming. HEAD remains untouched by this
installation.

## HEAD pre-flash backup — complete

HEAD / DWM-01 / J-Link serial `760225805` now has a fresh duplicate read-only
backup in `hardware/backups/DWM3001CDK_760225805/2026-10-03_pre_official_fira_ram_guard_pilot/`.
The two application-flash reads match SHA-256
`F11C520650617FCB813E6A989B687613FA58E6E43286D257637972F922848AB5`.
The two UICR reads match SHA-256
`B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1`.
HEAD remains unchanged pending its separate explicit flash approval.

## HEAD installation — complete

HEAD / DWM-01 / J-Link serial `760225805` was programmed only after the
fresh backup above. J-Link reported one normal application flash range of
225,280 bytes, then reset and ran the board. This was normal page erasure in
the application range, not a chip erase. The approved image has no UICR, OTP,
antenna-delay, or calibration address.

| Check | Result |
|---|---|
| Approved HEAD HEX SHA-256 | `A0DF1C27DF6802153D0822CA81128282885A4AC539F7D775DCE254A71D46C93B` |
| J-Link program and verify | passed |
| Independent 512 KiB post-flash read SHA-256 | `C5A0117BA22E02CDB843A9A5C5BC447CECA6748ABDEB152E4BD40092FE525502` |
| Defined Intel-HEX application bytes compared | 335,104 |
| Defined-byte mismatches | 0 |
| Pre/post UICR SHA-256 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |
| UICR changed | no |

Both members of the controlled PELVIS--HEAD RAM-guard pilot are now installed
and independently verified. The next step is the stated-distance battery test.

## First battery attempt — not accepted

With PELVIS and HEAD on battery at the stated 100 cm separation and the
gateway on COM16, the viewer received no Bluetooth packets during its clean
20-second buffer-drain period or its first recorded seconds. Therefore it
received no IMU, status, UWB-progress, or range data. This is a Bluetooth/app
startup observation; it is **not** a UWB distance result.

The installed pilot called `comp971_uwb_two_node_init()` before it started the
COMP971 BLE advertiser. That call can block or fail before BLE is available,
leaving the gateway with no diagnostic path. The exact internal cause is not
yet proven, but the startup order prevents us observing it.

## BLE-first startup candidate — built, not flashed

The next source-only candidate starts BLE and the scheduler first. The existing
status task waits two seconds, then initialises and starts the official UWB
path. It creates no additional FreeRTOS task: the SDK has a 4 KiB dynamic-task
heap, so adding a separate 4 KiB UWB task would risk preventing startup.
If UWB setup blocks, the independent BLE/IMU task can still report healthy
board data to the gateway.

| Board | HEX SHA-256 | RAM | Flash | Safety audit |
|---|---|---:|---:|---|
| PELVIS controller | `C6BAA64914121E3A87823DFBC5E413FBC0BFB6C961CAE1D0DAD2F6C33F1C9411` | 69,824 B / 112 KiB (60.88%) | 337,712 B / 504 KiB (65.44%) | RAM-only `qflash_write`; no program/erase API linked |
| HEAD responder | `AFB8187480D48DA9E788E0C14924962C0A6AB2E2117C62F0EC234185A1E6DD9F` | 69,824 B / 112 KiB (60.88%) | 336,576 B / 504 KiB (65.22%) | same result |

This candidate is not installed. Both boards need fresh duplicate read-only
backups before any later approved application flash.

## Non-goals

- No 15-pair mesh yet.
- No antenna-delay calibration, OTP, UICR, or persistent configuration write.
- No change to the nRF52840 dongle.
- No claim that FiRa will be the final many-to-many mesh scheduler.
