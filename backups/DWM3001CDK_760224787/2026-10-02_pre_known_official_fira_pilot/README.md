# PELVIS pre-known-official-FiRa-pilot backup attempt

## Status

This directory is reserved for the complete read-only PELVIS backup before any later approved application flash.

On 2026-10-02, `nrfjprog --ids` detected J-Link serial `760224787`. Its installed SEGGER back-end then failed during a complete flash/UICR read, continuously producing invalid multi-gigabyte and multi-megabyte partial files. Those invalid files were stopped and removed; they are not backups and must not be used.

The installed SEGGER J-Link Commander V9.72 then completed the read-only
backup correctly. The commands used only `savebin`; there was no reset, erase,
program, UICR write, OTP write, antenna-delay write, or calibration write.

| Region | Reads | SHA-256 | Result |
|---|---:|---|---|
| Application flash `0x00000000`--`0x0007FFFF` (512 KB) | 2 | `CBE03A4ADA42B65CC10DCEED505190CDE3EAD97137AE3BCCFC76EDC1ECAAADD2` | Identical |
| UICR `0x10001000`--`0x10001FFF` (4 KB) | 2 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` | Identical |

Files:

- `flash_read_1_0x00000000_0x0007ffff.bin`
- `flash_read_2_0x00000000_0x0007ffff.bin`
- `uicr_read_1_0x10001000_0x10001fff.bin`
- `uicr_read_2_0x10001000_0x10001fff.bin`

No erase, write, UICR modification, OTP modification, or calibration modification was performed.

## Valid current application evidence

The valid, earlier read-only application-region dump remains:

`hardware/outputs/records/diagnostics/2026-10-02_pelvis_qani_read_only_watch/pelvis_current_app_readonly_0x00000000_0x00054fff.bin`

SHA-256: `7DB033A50347081921531CE3800376159D2F973A9A525B5ADFFF930F8B488D40`

Its scope is `0x00000000`–`0x00054FFF`; it is a historical diagnostic subset
of the complete verified backup above.
