# LEFT-WRIST backup before pair-5 timing test v3

## Board and purpose

- Role: LEFT-WRIST (DWM-03)
- Registered J-Link serial: `760225868`
- MCU detected: `NRF52833_xxAA_REV1`
- SEGGER probe reference voltage: 3.300 V
- Read date: 30 September 2026
- Purpose: preserve current firmware and UICR before installing the temporary v3 diagnostic image.

## Backup verification

SEGGER J-Link Commander V9.24a read the full 512 KiB flash and UICR twice.
The duplicate copies match byte-for-byte by SHA-256:

| Memory | Size | SHA-256 | Duplicate reads match |
|---|---:|---|---|
| Flash read 1 | 524,288 bytes | `68FDB568D64823C01AD35615C9777C06AC56B00DFD1AED2FC0117AD5BE5DB808` | Yes |
| Flash read 2 | 524,288 bytes | `68FDB568D64823C01AD35615C9777C06AC56B00DFD1AED2FC0117AD5BE5DB808` | Yes |
| UICR read 1 | 4,096 bytes | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` | Yes |
| UICR read 2 | 4,096 bytes | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` | Yes |
| FICR identity | 4,096 bytes | `33999BA266D84B2AF2B2D0D85C9BA56D92FFA6B5A1605EA20F00A99D57300362` | Saved |

## Files

- `left_wrist_flash_read1.bin`, `left_wrist_flash_read2.bin`: full internal flash reads.
- `left_wrist_uicr_read1.bin`, `left_wrist_uicr_read2.bin`: UICR reads.
- `left_wrist_ficr_identity.bin`: read-only factory identity/configuration area.
- `read_left_wrist_before_pair5_v3.jlink`: exact SEGGER read script used.

No erase or programming was performed during backup. The image installed after
this backup is recorded in
`hardware/outputs/records/firmware/2026-09-30_pair5_delayed_tx_margin_v3/README.md`.
