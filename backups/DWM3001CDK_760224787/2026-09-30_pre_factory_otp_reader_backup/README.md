# PELVIS backup before factory-OTP reader

- Board: DWM-02 PELVIS
- Registered J-Link serial: `760224787`
- Target: `NRF52833_XXAA`
- Read date: 30 September 2026
- Purpose: preserve the currently installed pair-5 v3 image before the
  temporary read-only factory-OTP reader is written.

SEGGER J-Link Commander v9.72 connected to the named probe at 3.300 V and
read the full 512 KiB internal flash and UICR twice. The duplicate reads match
byte-for-byte:

| Memory | Size | SHA-256 | Duplicate reads match |
|---|---:|---|---|
| Flash read 1 | 524,288 bytes | `CC7C98058AA75C9E2AB512F6DE000BC40881453B15B03794C311A63E9F4BE573` | Yes |
| Flash read 2 | 524,288 bytes | `CC7C98058AA75C9E2AB512F6DE000BC40881453B15B03794C311A63E9F4BE573` | Yes |
| UICR read 1 | 4,096 bytes | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` | Yes |
| UICR read 2 | 4,096 bytes | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` | Yes |
| FICR identity | 4,096 bytes | `199C840A813AAC04F351683AE427EED55E5AE5D79DA7E84F8C46AED9BA8A893B` | Saved |

No erase or program command was used for this backup. The board was reset and
allowed to run after the reads completed.

Files:

- `pelvis_flash_read1.bin`, `pelvis_flash_read2.bin`: complete internal flash.
- `pelvis_uicr_read1.bin`, `pelvis_uicr_read2.bin`: UICR.
- `pelvis_ficr_identity.bin`: factory information/configuration region.
- `read_pelvis_before_factory_otp_reader.jlink`: exact read-only script.
