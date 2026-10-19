# PELVIS read-only firmware-identity check

Date: 2 October 2026  
Board: PELVIS / J-Link serial `760224787`

## What was done

J-Link read application flash `0x00000000` through `0x00054FFF` only. It did
not program, erase, reset, or write UICR, OTP, antenna delay, calibration, or
runtime flash.

The result is `pelvis_current_app_readonly_0x00000000_0x00054fff.bin`.

## Result

| Item | Value |
|---|---|
| Current read-back SHA-256 | `7DB033A50347081921531CE3800376159D2F973A9A525B5ADFFF930F8B488D40` |
| Recorded v7 post-flash SHA-256 | `ABA0403970DD18F338A614D204D4E9555A565216292B9E1AC221040AC715BAF7` |
| Byte mismatches against recorded v7 read-back | 220,845 |
| First mismatch | `0x0001C004` |

The application currently on PELVIS is therefore not the recorded v7 image.
No range result from the current image can be treated as a v7 result.

The temporary GDB trace file `v7_start_trace.gdb` contains hardware-breakpoint
addresses recovered from the recorded v7 HEX. It is retained for audit only;
it must not be used against the current unknown image.
