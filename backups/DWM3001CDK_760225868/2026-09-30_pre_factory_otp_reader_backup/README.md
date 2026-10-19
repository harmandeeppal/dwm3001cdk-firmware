# LEFT-WRIST backup before factory-OTP reader

- Board: DWM-03 LEFT-WRIST
- Registered J-Link serial: `760225868`
- Target: `NRF52833_XXAA`
- Read date: 30 September 2026
- Purpose: preserve the currently installed pair-5 v3 image before the
  temporary read-only factory-OTP reader is written.

SEGGER J-Link Commander v9.72 connected to the named probe at 3.300 V and
read the full 512 KiB internal flash and UICR twice. The duplicate reads match
byte-for-byte:

| Memory | Size | SHA-256 | Duplicate reads match |
|---|---:|---|---|
| Flash read 1 | 524,288 bytes | `92AAF2BFAD698459883AC47CCBA0BD9AA6B7A653E2FC95B6C9267A65D09566D2` | Yes |
| Flash read 2 | 524,288 bytes | `92AAF2BFAD698459883AC47CCBA0BD9AA6B7A653E2FC95B6C9267A65D09566D2` | Yes |
| UICR read 1 | 4,096 bytes | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` | Yes |
| UICR read 2 | 4,096 bytes | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` | Yes |
| FICR identity | 4,096 bytes | `33999BA266D84B2AF2B2D0D85C9BA56D92FFA6B5A1605EA20F00A99D57300362` | Saved |

No erase or program command was used for this backup. The board was reset and
allowed to run after the reads completed.

Files:

- `left_wrist_flash_read1.bin`, `left_wrist_flash_read2.bin`: complete internal flash.
- `left_wrist_uicr_read1.bin`, `left_wrist_uicr_read2.bin`: UICR.
- `left_wrist_ficr_identity.bin`: factory information/configuration region.
- `read_left_wrist_before_factory_otp_reader.jlink`: exact read-only script.
