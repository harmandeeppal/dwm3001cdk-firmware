# HEAD read-only backup before BLE-first startup pilot

This safety backup was made through J9 from **HEAD**, J-Link serial `760225805`,
before any BLE-first pilot write. It performed reads only; it did not erase,
write, reset, or run the board.

| Item | Result |
|---|---|
| Internal flash range | `0x00000000`–`0x0007FFFF` (512 KiB) |
| Flash read 1 SHA-256 | `C5A0117BA22E02CDB843A9A5C5BC447CECA6748ABDEB152E4BD40092FE525502` |
| Flash read 2 SHA-256 | `C5A0117BA22E02CDB843A9A5C5BC447CECA6748ABDEB152E4BD40092FE525502` |
| Duplicate flash reads match | Yes |
| Same as prior verified RAM-guard readback | Yes |
| UICR range | `0x10001000`–`0x10001FFF` (4 KiB) |
| UICR read 1 SHA-256 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |
| UICR read 2 SHA-256 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |
| Duplicate UICR reads match | Yes |

The proposed BLE-first image is built but has not been flashed. Its SHA-256 is
`AFB8187480D48DA9E788E0C14924962C0A6AB2E2117C62F0EC234185A1E6DD9F`.
It changes application startup order only: BLE starts first and official UWB
startup begins two seconds later. It does not write UICR, OTP, antenna-delay,
or calibration values.
