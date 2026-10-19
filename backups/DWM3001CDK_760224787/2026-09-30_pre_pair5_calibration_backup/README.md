# PELVIS pre pair-5 calibration backup

## Board and read

- Board: DWM-02-PELVIS
- Registered J-Link serial: `760224787`
- Read date: 2026-09-30
- Tool: SEGGER J-Link Commander V9.72, SWD at 4000 kHz
- The board was halted for the reads and reset/run afterward.
- Code flash (512 KiB) and UICR (4 KiB) were each read twice. Each pair of
  reads is byte-for-byte identical.
- FICR identity region (4 KiB) was read once for reference.
- J-Link reported `VTref=3.300V`; all `savebin` operations completed.

## SHA-256

| File | Size | SHA-256 |
|---|---:|---|
| `pelvis_code_flash_read1.bin` | 524288 | `0B83A976B3ED5DB78BC2EDCD7C2B3BB059C59D402133C2D914A24BC674BD4405` |
| `pelvis_code_flash_read2.bin` | 524288 | `0B83A976B3ED5DB78BC2EDCD7C2B3BB059C59D402133C2D914A24BC674BD4405` |
| `pelvis_uicr_read1.bin` | 4096 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |
| `pelvis_uicr_read2.bin` | 4096 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |
| `pelvis_ficr_identity.bin` | 4096 | `199C840A813AAC04F351683AE427EED55E5AE5D79DA7E84F8C46AED9BA8A893B` |

The readback files are retained as the recovery record. Do not erase or alter
this directory. The normal full-mesh source/build remains the preferred way
to restore normal operation after the pair-only experiment; the raw readback
is a fallback, not a substitute for a documented firmware build.

## Read command file

`read_pelvis_current_backup.jlink` contains the exact J-Link commands used.
