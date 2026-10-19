# PELVIS read-only backup before BLE-first startup pilot

## Purpose

This is a safety backup of **PELVIS** (J-Link serial `760224787`) made before any
BLE-first pilot firmware is written. The backup operation only read memory; it
did not erase, write, reset, or start the board.

## Readback result

| Item | Result |
|---|---|
| Internal flash range | `0x00000000` through `0x0007FFFF` (512 KiB) |
| Flash read 1 SHA-256 | `1F23D95D79279D8962F556417A5635C87F58A73ADBF0D1A49F2EFCC1858D7DF3` |
| Flash read 2 SHA-256 | `1F23D95D79279D8962F556417A5635C87F58A73ADBF0D1A49F2EFCC1858D7DF3` |
| Two flash reads identical | Yes |
| UICR range | `0x10001000` through `0x10001FFF` (4 KiB) |
| UICR read 1 SHA-256 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |
| UICR read 2 SHA-256 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |
| Two UICR reads identical | Yes |
| Same as the verified 3 October RAM-guard post-flash image | Yes |

## Files

- `flash_read_1_0x00000000_0x0007ffff.bin` and `flash_read_2_...`: independent complete internal-flash reads.
- `uicr_read_1_...` and `uicr_read_2_...`: independent UICR reads.
- `read_only_backup.jlink`: the exact J-Link read-only command file.

## Next proposed write (not yet performed)

The BLE-first pilot image is built but **has not been flashed**. It starts BLE
and the normal IMU task first, waits two seconds, then starts the official UWB
startup from the existing status task. This lets the gateway show whether BLE
works even if UWB startup later stalls.

The candidate PELVIS HEX is:

`C:\\COMP971_QORVO_SDK_1_1_1\\BuildOutput\\COMP971\\FreeRTOS\\DWM3001CDK\\Debug_PELVIS_v3\\DWM3001CDK-COMP971-FreeRTOS.hex`

SHA-256:

`C6BAA64914121E3A87823DFBC5E413FBC0BFB6C961CAE1D0DAD2F6C33F1C9411`

This change does not write UICR, OTP, or antenna-delay values. A normal
application flash would replace only the application flash pages covered by
the verified HEX file; it is not a chip erase.
