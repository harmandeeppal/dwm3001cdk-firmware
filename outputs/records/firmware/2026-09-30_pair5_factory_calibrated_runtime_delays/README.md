# Pair-5 factory-calibrated runtime-delay test — 30 September 2026

## Purpose

This is the next, controlled PELVIS--LEFT-WRIST test after the read-only
factory-OTP report succeeded. It keeps the successful pair-5 v3 timing
margins, IMU stream, BLE transport, and diagnostic reporting unchanged. The
only ranging change is that each board will use its own verified, factory
programmed channel-5/PRF-64 antenna delays at run time.

It does **not** write OTP. The values are compiled into ordinary application
flash and passed to the standard DW3000 driver at boot. The reader diagnostic
remains enabled so the viewer can continue to show the unchanged OTP words as
evidence.

## Verified factory values

The 30-second reader session `session_20260930_072521` reported:

| Board | OTP word | RX delay | TX delay | Previous fallback |
|---|---|---:|---:|---:|
| PELVIS `760224787` | `0x3FF03FF0` | 16368 | 16368 | 16390 |
| LEFT-WRIST `760225868` | `0x3FF43FF4` | 16372 | 16372 | 16390 |

Qorvo's OTP revision-1 map defines word `0x1A` as channel-5/PRF-64 RX
(upper 16 bits) and TX (lower 16 bits) antenna delay. The local DW3000 API
documents that the RX delay is subtracted from raw receive timestamps and the
TX delay is added to raw transmit timestamps. This build uses the reported
value at both call sites via `dwt_setrxantennadelay()` and
`dwt_settxantennadelay()`.

## Exact build differences from the installed reader image

| Item | Current read-only reader | This runtime-delay test |
|---|---|---|
| PELVIS runtime TX/RX delay | 16390 / 16390 fallback | 16368 / 16368 factory value |
| LEFT-WRIST runtime TX/RX delay | 16390 / 16390 fallback | 16372 / 16372 factory value |
| OTP action | Read only | Read only |
| Pair selection | Pair 5 only | Pair 5 only |
| Delayed TX margins | 5 ms response; 6 ms final | unchanged |
| IMU and BLE transport | unchanged | unchanged |

## Images

| Board | File | SHA-256 |
|---|---|---|
| PELVIS `760224787` | `DWM-02-PELVIS__pair5-factory-runtime-delays__760224787.hex` | `F37E50A58414FC88198ECD608103A5D822453CE1F5F38CD17D095785D9B50093` |
| LEFT-WRIST `760225868` | `DWM-03-LEFT-WRIST__pair5-factory-runtime-delays__760225868.hex` | `098B55A55EC08C3FAEB25817D9CFCA91DE9205B8948440A0891A6DA7124AC593` |

Both clean builds completed with NCS v3.4.0 for `nrf52833dk/nrf52833`.
The explicit PELVIS definitions were:

```text
-DUWB_TX_ANT_DLY=16368U -DUWB_RX_ANT_DLY=16368U
```

The explicit LEFT-WRIST definitions were:

```text
-DUWB_TX_ANT_DLY=16372U -DUWB_RX_ANT_DLY=16372U
```

## Controlled 100 cm result and decision

Both temporary images were installed only after duplicate full-flash and UICR
backups of the reader images. The controlled 60-second PELVIS--LEFT-WRIST
session `session_20260930_080054` was then collected at the user-stated
100 cm separation.

| Measure | Result |
|---|---:|
| PELVIS radio attempts | 2,754 |
| Valid / invalid radio attempts | 2,754 / 0 |
| Valid reports delivered to laptop | 119 (1.98/s) |
| Range minimum / median / mean / SD / maximum | 102 / 184 / 178.7 / 26.5 / 213 cm |
| Mean bias at stated 100 cm | +78.7 cm |
| New USB drops during viewer session | 0 |

This is a **failed accuracy test**, even though the UWB exchanges and BLE
delivery were reliable. The error is present throughout the session: the six
10-second windows had means from 175.0 to 185.6 cm. It is not one isolated
outlier and must not be averaged or filtered into an accepted result.

The local official `DWM3001C Data Sheet` maps OTP word `0x01A` to channel-5
PRF-64 RX/TX antenna delays, confirming that the read-only diagnostic used the
right address. However, the same data sheet specifically says that the
engineering-sample `DWM3001CDKE1.0` must **not** use the channel-5 OTP antenna
delay and must use the default value `16390`. Because these kits are identified
as DWM3001CDK and the temporary application of the OTP values made this
controlled result unacceptable, direct OTP application is now on hold. The
physical kit revision must be confirmed before treating the stored word as a
runtime calibration value.

Do not collect 50 cm or 75 cm data with these temporary images. The next
firmware action, after the kit revision is confirmed, is to restore the
read-only-reader image with the known-safe default `16390` and then investigate
the remaining range-calculation and radio-configuration issue against Qorvo's
reference implementation. Neither option writes OTP.

## Install record

PELVIS `760224787` was selected through J9 by registered J-Link serial. Its
current OTP-reader image was preserved first in the duplicate-matched backup
`hardware/backups/DWM3001CDK_760224787/2026-09-30_pre_factory_runtime_delay_test_backup/`.
SEGGER J-Link Commander v9.24a then programmed the PELVIS image and reported
`Program & Verify` success for one changed 8 KiB flash range, followed by
reset and run. OTP was not written.

LEFT-WRIST `760225868` was then selected through J9 by registered J-Link
serial. Its current OTP-reader image was preserved first in the
duplicate-matched backup
`hardware/backups/DWM3001CDK_760225868/2026-09-30_pre_factory_runtime_delay_test_backup/`.
SEGGER J-Link Commander v9.24a then programmed the LEFT-WRIST image and
reported `Program & Verify` success for one changed 4 KiB flash range,
followed by reset and run. OTP was not written.

Both matching runtime-delay images are now installed and verified. They are
temporary pair-5 test images, not six-node fleet firmware.

## Restore status

The PELVIS and LEFT-WRIST temporary runtime-delay images were superseded on
30 September 2026 after their controlled 100 cm test failed accuracy
acceptance. Each board received a new duplicate-matched backup, then was
restored to its archived reader/default-`16390` image. J-Link reported
Program & Verify success and reset/run for both restores. No OTP was written
on either board.
