# Qorvo QANI startup-contract build

Date: 2 October 2026  
Status: PELVIS and HEAD programmed and read-back verified; awaiting the first two-board battery test.

## Purpose

This is one controlled diagnostic candidate for the PELVIS--HEAD startup
problem. It does not replace the planned official UCI-derived wearable path
for the final product.

The candidate keeps Qorvo's QANI FiRa/UWB-MAC implementation and adds the
one Qorvo initialization call that was missing from the COMP971 adapter:

```c
niq_init(StartUWB, StopUWB, crypto_init, crypto_deinit,
         crypto_get_random_vector);
CreateQaniTask();
```

Qorvo's own `Projects/FreeRTOS/QANI/Common/src/main.c` performs those calls in
that order. The COMP971 adapter already called `persistent_time_init()` and
`CreateQaniTask()`, but it did not register the NIQ callbacks first.

## Source change

Changed project-owned source only:

`hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_two_node.c`

The change adds `HAL_crypto.h` and `niq.h`, calls `niq_init(...)` immediately
after `persistent_time_init(0)`, and reports a distinct `0xD2` startup failure
if NIQ registration fails. It does not modify the installed Qorvo SDK,
antenna delay, OTP, UICR, calibration, or runtime flash.

## Official API checks

The SDK header for `fira_helper_add_controlee()` says `0` **or a positive
value** means success. Therefore the existing `result < QERR_SUCCESS` check
in the QANI adapter is correct. The SDK role enum also confirms controller
initiator is `1` and responder is `0`; the PELVIS/HEAD role configuration is
correct.

## Outputs

These are the active rebuilt output paths. The old `v3` folder name is a
historical folder name only; the hashes below identify this new build exactly.

| Board role | CMake settings | HEX SHA-256 | ELF SHA-256 |
|---|---|---|---|
| PELVIS | node `1`, role `1` controller, `COMP971-MOTION-PELVIS` | `F667585F4D0065BB4BA00FF78B61B9D4D7C2E588C4BFF082752BFF54FCB6AF02` | `C6DAF242A896E9AB5683592E97AC959BBB86E913D3BE8155689876E9E829C50E` |
| HEAD | node `0`, role `2` responder, `COMP971-MOTION-HEAD` | `F992C44874AF544E95677BE320B7A3C3F6D0C188FF2D3F76B1F0BA90DE0A91E1` | `7DDC5FD97E5B95A47070FD227315E3AA61CE5725D17CD592B084DCF04376FDF9` |

Build output locations:

- `C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_PELVIS_v3`
- `C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_HEAD_v3`

Both complete builds passed. PELVIS uses 343,064 B / 504 KB flash (66.47%) and
70,400 B / 112 KB RAM (61.38%). HEAD uses 342,184 B / 504 KB flash (66.30%).

## PELVIS program and read-back verification

PELVIS, J-Link serial `760224787`, was programmed on 2 October 2026 after the
complete verified pre-flash backup documented at
`hardware/backups/DWM3001CDK_760224787/2026-10-02_pre_known_official_fira_pilot/`.

J-Link `loadfile` programmed the normal application range only. The image
contains no UICR or OTP records. J-Link reported one affected flash range,
233,472 bytes, and completed its program-and-verify step successfully. The
board was then reset once and started.

A separate read-only post-flash read captured
`pelvis_760224787_niqinit_postflash_0x00000000_0x00054fff.bin`:

- Read-back SHA-256: `751371FF1622413FF3309091AC89C6B6961007B9DEEC27DEADDD4D06525CBD61`
- Hex data bytes compared: `342,224`
- Mismatches: `0`

No chip erase, UICR write, OTP write, antenna-delay write, calibration write,
or runtime flash write was performed.

## Required proof before the matching HEAD flash

HEAD full flash and UICR backup completed successfully at
`hardware/backups/DWM3001CDK_760225805/2026-10-02_pre_niq_startup_pilot/`.
The two 512 KB flash reads match; the two 4 KB UICR reads match.

1. Obtain explicit approval to program the named HEAD image only.
2. Read back the programmed application and compare it to the HEAD HEX.
3. At 100 cm on battery, confirm startup diagnostic `0xC5` and non-zero UWB
   attempts before judging range accuracy.

## HEAD program and read-back verification

HEAD, J-Link serial `760225805`, was programmed after the complete verified
pre-flash backup documented at
`hardware/backups/DWM3001CDK_760225805/2026-10-02_pre_niq_startup_pilot/`.

J-Link `loadfile` programmed the normal application range only. The image
contains no UICR or OTP records. J-Link reported one affected flash range,
233,472 bytes, and completed its program-and-verify step successfully. The
board was then reset once and started.

A separate read-only post-flash read captured
`head_760225805_niqinit_postflash_0x00000000_0x00054fff.bin`:

- Read-back SHA-256: `93C3F8D7F6F82649D13D0FB981E8559AD60E7947F121BCC245C04B20643A11C2`
- Hex data bytes compared: `341,344`
- Mismatches: `0`

No chip erase, UICR write, OTP write, antenna-delay write, calibration write,
or runtime flash write was performed.

## Next physical test

1. Disconnect HEAD from J9.
2. Power PELVIS and HEAD from their batteries.
3. Place their UWB antennas 100 cm apart, facing each other as consistently as
   the enclosures allow.
4. Plug in the nRF52840 dongle.
5. Wait 20 seconds, then use the live viewer to check for both IMU streams and
   non-zero PELVIS-to-HEAD UWB attempts.

This is a startup test first. The pass condition is the Qorvo startup stage
and non-zero UWB attempts; range accuracy is assessed only after that.
## Battery test result — not accepted

PELVIS and HEAD were run on battery at a stated 100 cm separation with the
nRF52840 gateway connected. This was the first test after both exact images
had been independently read back and matched to their Intel-HEX data.

| Observation | Result |
|---|---:|
| HEAD IMU stream | live, about 58.8 Hz |
| PELVIS IMU stream | live, about 58.8 Hz |
| Board status | `ICM ready; UWB started` on both boards |
| PELVIS UWB attempts | 0 |
| PELVIS valid ranges | 0 |
| PELVIS invalid ranges | 0 |
| UWB setup/failure diagnostics | 0 |

The saved clean 20-second gateway session is
`hardware/outputs/records/acquisition/live_gateway_viewer/session_20261002_092405/`.
Its `summary.json`, `uwb_reports.csv`, and `uwb_failure_diagnostics.csv` show
the same result.

### Interpretation

`UWB started` means the QANI code accepted the request to start a session. It
does **not** mean that a radio ranging transaction occurred. With zero
attempts on both boards, no UWB packet was scheduled. This is therefore not a
100 cm distance, battery, J20, BLE-reception, antenna-delay, or calibration
result.

The added `niq_init(...)` call made the application match Qorvo’s QANI startup
contract, but it did not make QANI schedule a first DWM3001CDK-to-DWM3001CDK
range. No further QANI timing or configuration patches will be made. The next
candidate will instead use the normal Qorvo FiRa/UWB-MAC application sequence
with explicit checking of the measurement-schedule setup and without any
runtime flash, UICR, OTP, or calibration write.
