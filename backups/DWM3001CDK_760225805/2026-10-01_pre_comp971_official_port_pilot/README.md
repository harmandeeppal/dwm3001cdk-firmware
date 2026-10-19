# DWM-01 HEAD protected pre-pilot backup

## Identity and purpose

| Item | Value |
|---|---|
| Registered board | DWM-01 HEAD |
| J-Link serial | `760225805` |
| Date | 1 October 2026 |
| Purpose | protect the current HEAD image before any future official-Qorvo custom firmware pilot |
| Operation performed | read-only flash, UICR and FICR backup; board was reset and resumed |

## Readback verification

| Region | Files | Bytes each | SHA-256 | Result |
|---|---|---:|---|---|
| Code flash | `head_flash_read1.bin`, `head_flash_read2.bin` | 524,288 | `D680C8414B81201C1B11FA8271B02611AA65FA902926100AC79F002B6E60B7D4` | duplicate reads match |
| UICR | `head_uicr_read1.bin`, `head_uicr_read2.bin` | 4,096 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` | duplicate reads match |
| FICR identity | `head_ficr_identity.bin` | 4,096 | `DDCFBDD2E7F67A9B5B2721BD84F9683893F8089DEE91F932CA1B84BAFB4414FC` | read successfully |

The current flash hash is expected to differ from the 30 September pre-
diagnostic backup because the later diagnostic firmware was installed after
that older backup. No erase, program, UICR write, OTP write, or calibration
write occurred during this backup.

`read_head_readonly.jlink` is the exact read-only SEGGER command script.
