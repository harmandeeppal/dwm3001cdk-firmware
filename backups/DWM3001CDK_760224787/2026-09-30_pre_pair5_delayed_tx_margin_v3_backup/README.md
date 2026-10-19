# PELVIS backup before pair-5 timing test v3

## Board and purpose

- Role: PELVIS (DWM-02)
- Registered J-Link serial: `760224787`
- MCU detected: `NRF52833_xxAA_REV1`
- SEGGER probe reference voltage: 3.300 V
- Read date: 30 September 2026
- Purpose: preserve current firmware and UICR before installing the temporary v3 diagnostic image.

## Backup verification

SEGGER J-Link Commander V9.24a read the full 512 KiB flash and UICR twice.
The duplicate copies match byte-for-byte by SHA-256:

| Memory | Size | SHA-256 | Duplicate reads match |
|---|---:|---|---|
| Flash read 1 | 524,288 bytes | `49FBAD988269FEAE7C7A2508C6BC6C384003250453D0FE5ACC1F96426D6754C0` | Yes |
| Flash read 2 | 524,288 bytes | `49FBAD988269FEAE7C7A2508C6BC6C384003250453D0FE5ACC1F96426D6754C0` | Yes |
| UICR read 1 | 4,096 bytes | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` | Yes |
| UICR read 2 | 4,096 bytes | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` | Yes |
| FICR identity | 4,096 bytes | `199C840A813AAC04F351683AE427EED55E5AE5D79DA7E84F8C46AED9BA8A893B` | Saved |

## Files

- `pelvis_flash_read1.bin`, `pelvis_flash_read2.bin`: full internal flash reads.
- `pelvis_uicr_read1.bin`, `pelvis_uicr_read2.bin`: UICR reads.
- `pelvis_ficr_identity.bin`: read-only factory identity/configuration area.
- `read_pelvis_before_pair5_v3.jlink`: exact SEGGER read script used.

No erase or programming was performed during backup. The image installed after
this backup is recorded in
`hardware/outputs/records/firmware/2026-09-30_pair5_delayed_tx_margin_v3/README.md`.
