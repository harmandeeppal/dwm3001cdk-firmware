# HEAD pre-official-FiRa-RAM-guard backup

Date: 3 October 2026  
Board: HEAD / DWM-01 / J-Link serial `760225805`  
Status: complete, read-only, duplicate reads match.

This is the recovery point before any possible HEAD responder flash for the
official-FiRa RAM-guard two-board pilot. J-Link Commander identified the board
by serial `760225805` over J9. Its commands used only `connect` and `savebin`.
There was no reset, chip erase, program, UICR write, OTP write, antenna-delay
write, calibration write, or runtime flash operation.

| Region | Address range | Reads | SHA-256 | Result |
|---|---|---:|---|---|
| Application flash | `0x00000000`--`0x0007FFFF` (512 KiB) | 2 | `F11C520650617FCB813E6A989B687613FA58E6E43286D257637972F922848AB5` | identical |
| UICR | `0x10001000`--`0x10001FFF` (4 KiB) | 2 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` | identical |

Files:

- `flash_read_1_0x00000000_0x0007ffff.bin`
- `flash_read_2_0x00000000_0x0007ffff.bin`
- `uicr_read_1_0x10001000_0x10001fff.bin`
- `uicr_read_2_0x10001000_0x10001fff.bin`
- `read_only_backup.jlink`
