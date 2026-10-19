# LEFT-WRIST backup before v2 transmit-status diagnostic

This is a read-only backup of LEFT-WRIST, J-Link serial `760225868`, taken
immediately before installing the pair-5 pre-recovery status diagnostic.

- Date: 2026-09-30.
- Tool: SEGGER J-Link Commander V9.72, SWD at 4000 kHz.
- J-Link serial selected: `760225868`; target voltage: 3.300 V.
- Code flash: two complete 512 KiB reads; the files match exactly.
- UICR: two complete 4 KiB reads; the files match exactly.
- FICR identity: one 4 KiB read.
- The archived v1 LEFT-WRIST image verified against the board before writing.
- The v2 image was then programmed and verified successfully; reset/run
  completed. The programming command returned exit code 0. SEGGER printed
  JLinkARM.dll `-256` warnings, but erase, write, and verify all reported
  success. A separate verify after reset also passed (exit code 0).

## SHA-256

| File | Size (bytes) | SHA-256 |
|---|---:|---|
| `left_wrist_flash_read1.bin` | 524288 | `C71A712658E4015B8618D0498A5EC6DAC01F8CA4D53132F3A5C9FA047B14EC71` |
| `left_wrist_flash_read2.bin` | 524288 | `C71A712658E4015B8618D0498A5EC6DAC01F8CA4D53132F3A5C9FA047B14EC71` |
| `left_wrist_uicr_read1.bin` | 4096 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |
| `left_wrist_uicr_read2.bin` | 4096 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |
| `left_wrist_ficr_identity.bin` | 4096 | `33999BA266D84B2AF2B2D0D85C9BA56D92FFA6B5A1605EA20F00A99D57300362` |

The exact read-only commands are in
[`read_left_wrist_before_txstatus_v2.jlink`](read_left_wrist_before_txstatus_v2.jlink);
the J-Link session output is in
[`read_left_wrist_before_txstatus_v2.log`](read_left_wrist_before_txstatus_v2.log).
Keep this folder unchanged as the recovery copy for the pre-v2 board state.
