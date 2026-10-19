# LEFT-WRIST pre transmit-status diagnostic backup

This is the read-only backup of LEFT-WRIST, J-Link serial `760225868`, before
installing its pair-5 diagnostic image.

- Read date: 2026-09-30
- Tool: SEGGER J-Link Commander V9.72, SWD at 4000 kHz
- J-Link selected serial `760225868` and reported `VTref=3.300V`.
- Code flash (512 KiB) and UICR (4 KiB) were each read twice; both pairs are
  byte-for-byte identical. FICR identity was read once.
- The board was reset and started after reading.
- All 147,736 programmed bytes matched the archived LEFT-WRIST failure-stage
  diagnostic HEX, with zero mismatches.

## SHA-256

| File | Size | SHA-256 |
|---|---:|---|
| `left_wrist_flash_read1.bin` | 524288 | `F20206561823753DA56190376D59600855F26645B3522D11EF859B9CDD23216C` |
| `left_wrist_flash_read2.bin` | 524288 | `F20206561823753DA56190376D59600855F26645B3522D11EF859B9CDD23216C` |
| `left_wrist_uicr_read1.bin` | 4096 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |
| `left_wrist_uicr_read2.bin` | 4096 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |
| `left_wrist_ficr_identity.bin` | 4096 | `33999BA266D84B2AF2B2D0D85C9BA56D92FFA6B5A1605EA20F00A99D57300362` |

The exact read-only commands are in
`read_left_wrist_before_txstatus_diagnostic.jlink`. Keep these files unchanged;
they preserve the exact pre-update board contents.
