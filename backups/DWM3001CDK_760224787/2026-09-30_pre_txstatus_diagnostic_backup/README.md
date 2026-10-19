# PELVIS pre transmit-status diagnostic backup

## Read-only backup

- Board: DWM-02-PELVIS
- Registered J-Link serial: `760224787`
- Read date: 2026-09-30
- Tool: SEGGER J-Link Commander V9.72, SWD at 4000 kHz
- J-Link reported `VTref=3.300V` and identified the requested serial.
- Code flash (512 KiB) and UICR (4 KiB) were each read twice. Each pair is
  byte-for-byte identical. FICR identity was read once.
- J-Link reset and started the board after reading.

## SHA-256

| File | Size | SHA-256 |
|---|---:|---|
| `pelvis_flash_read1.bin` | 524288 | `3EC892F02DB721ED43C6FBAEF17AA1C909B9C3995EF0BD10707D5A81F02DAF78` |
| `pelvis_flash_read2.bin` | 524288 | `3EC892F02DB721ED43C6FBAEF17AA1C909B9C3995EF0BD10707D5A81F02DAF78` |
| `pelvis_uicr_read1.bin` | 4096 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |
| `pelvis_uicr_read2.bin` | 4096 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |
| `pelvis_ficr_identity.bin` | 4096 | `199C840A813AAC04F351683AE427EED55E5AE5D79DA7E84F8C46AED9BA8A893B` |

The Intel HEX bytes in the saved flash were compared with the archived
pair-5 calibration image: all 144,744 programmed bytes matched, with zero
mismatches. This confirms the board held the expected pair-only firmware
before the diagnostic update.

The exact read-only commands are in
`read_pelvis_before_txstatus_diagnostic.jlink`. Keep these files unchanged;
they preserve the exact pre-update board contents.
