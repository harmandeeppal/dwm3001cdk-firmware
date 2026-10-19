# HEAD recovery backup before 4 KiB UWB-task pilot

## Purpose

This is a read-only recovery backup taken immediately before any possible
installation of the 4 KiB UWB-task pilot on DWM-01 HEAD.

## Board identity

| Item | Value |
|---|---|
| Role | HEAD / DWM-01 |
| J-Link serial | `760225805` |
| Target | nRF52833 XXAA |
| Debug connection | J9 / SWD at 4 MHz |
| Target voltage | 3.300 V |

## Duplicate-read verification

| Memory area | Bytes per read | Read A SHA-256 | Read B SHA-256 | Match |
|---|---:|---|---|---|
| Full flash, `0x00000000`–`0x0007FFFF` | 524,288 | `4D6E769C3D6D49E9D87C8F32AAFC2FBC670E3DDAADD1C3B9659A9D3C413327DD` | `4D6E769C3D6D49E9D87C8F32AAFC2FBC670E3DDAADD1C3B9659A9D3C413327DD` | Yes |
| UICR, `0x10001000`–`0x10001FFF` | 4,096 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` | Yes |

Both commands used J-Link `savebin` only. They did not program, erase, reset,
or change UICR, OTP, antenna delay, or calibration values.

## Files

- `head_760225805_flash_a_0x00000000_0x0007ffff.bin`
- `head_760225805_flash_b_0x00000000_0x0007ffff.bin`
- `head_760225805_uicr_a_0x10001000_0x10001fff.bin`
- `head_760225805_uicr_b_0x10001000_0x10001fff.bin`
- `read_only_backup_a.jlink`
- `read_only_backup_b.jlink`
