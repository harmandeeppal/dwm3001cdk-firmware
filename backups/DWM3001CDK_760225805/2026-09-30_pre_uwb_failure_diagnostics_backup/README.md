# DWM-01-HEAD pre-flash backup — 30 September 2026

Board identity was confirmed by J-Link serial `760225805`, registered as
`DWM-01-HEAD` in `hardware/config/device_registry.json`.

The files below were read before installing the 2026-09-30 UWB failure
diagnostics image. The 512 KB code flash read completed successfully. The
read-only SEGGER Commander commands are retained in `read_head_flash.jlink`.

| File | Bytes | SHA-256 |
|---|---:|---|
| `head_code_flash.bin` | 524288 | `5D00AD3B2BD30A9BF6B226631FFF9251C1215735A0CA443E53F4206381AA3772` |
| `head_uicr.bin` | 4096 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |
| `head_ficr.bin` | 4096 | `DDCFBDD2E7F67A9B5B2721BD84F9683893F8089DEE91F932CA1B84BAFB4414FC` |
