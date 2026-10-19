# PELVIS pre-official-FiRa-RAM-guard backup

Date: 3 October 2026  
Board: PELVIS / DWM-02 / J-Link serial `760224787`  
Status: complete, read-only, duplicate reads match.

## Scope

This is the recovery point immediately before any possible PELVIS flash of
the `2026-10-03_official_fira_ram_guard_pilot` image. It was made only after
J-Link Commander independently identified serial `760224787` on J9.

J-Link commands used only `connect` and `savebin`. No reset, chip erase,
application program, UICR write, OTP write, antenna-delay write, calibration
write, or runtime flash operation occurred.

| Region | Address range | Reads | SHA-256 | Result |
|---|---|---:|---|---|
| Application flash | `0x00000000`--`0x0007FFFF` (512 KiB) | 2 | `689F06CEEEE3D1D1D15838DBF387DD38C2E3517FE93BFCA22143B9A78AF6EED4` | identical |
| UICR | `0x10001000`--`0x10001FFF` (4 KiB) | 2 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` | identical |

Files:

- `flash_read_1_0x00000000_0x0007ffff.bin`
- `flash_read_2_0x00000000_0x0007ffff.bin`
- `uicr_read_1_0x10001000_0x10001fff.bin`
- `uicr_read_2_0x10001000_0x10001fff.bin`
- `read_only_backup.jlink`

## Configuration-control note

The 512 KiB flash backup differs from the valid 2 October pre-pilot backup:
211,124 bytes differ, beginning at `0x0001C004`. This confirms that the
currently installed application is different from that earlier recovery
snapshot. It does not identify the reason or imply hardware damage.

This 3 October backup is the recovery point for the next PELVIS-only test.
