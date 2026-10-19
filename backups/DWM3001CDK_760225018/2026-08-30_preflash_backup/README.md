# DWM3001CDK 760225018 pre-flash backup

This folder preserves and audits the original nRF52833 state before the first
application is programmed.

## Validated result

- J-Link serial: `760225018`
- Target: `NRF52833_XXAA`
- SWD access: successful at 1000 kHz and 3.300 V
- Read protection: not enabled at the time of capture
- Code flash: all 524,288 bytes are `0xFF` (erased)
- UICR: all 4,096 bytes are `0xFF` (erased)
- Two independent reads of code flash are byte-for-byte identical
- Two independent reads of UICR are byte-for-byte identical
- FICR was captured for device identity/audit only and must never be written

The original board therefore contained no nRF52833 application firmware or
custom UICR configuration. The steady/flashing D20 LED is the onboard J-Link
status indicator and does not demonstrate application firmware on the target.

## Canonical artifacts

| Artifact | Address | Size | SHA-256 |
|---|---:|---:|---|
| `dwm3001cdk_760225018_code_flash.bin` | `0x00000000` | 524288 | `043E238A765F7CFBC62596A50E53C8FFB6B188A99357B0EBEDE251725D67589F` |
| `dwm3001cdk_760225018_code_flash.hex` | `0x00000000` | 1474692 | `881F3341E84C9A38C4BCB08723CF29A0F2BB3E97A350D8EEB0F290B4EA466747` |
| `dwm3001cdk_760225018_uicr.bin` | `0x10001000` | 4096 | `F47A8EC3E9AFF2318D896942282AD4FE37D6391C82914F54A5DA8A37DE1300C6` |
| `dwm3001cdk_760225018_uicr.hex` | `0x10001000` | 11571 | `E690B44075DE201841CE24E0EA305ACA6D1D9EFEA4EF5F93272E711C906A8C48` |
| `dwm3001cdk_760225018_ficr_identity.bin` | `0x10000000` | 4096 | `7BFA843A37F1D03255ADDC5C071735A79D9BCF4CD77B788A255C871E30E800CA` |

The raw `read1` and `read2` files and both SEGGER logs are retained as
independent evidence. `backup_manifest.json` is the machine-readable record.

## Commands used

Only `connect` and `savebin` were used against the nRF52833. No `erase`,
`recover`, `loadfile`, `loadbin`, or explicit reset command was used.

SEGGER J-Link Commander automatically upgraded the separate onboard J-Link
firmware on first connection from its 2017 build to the June 2026 build. This
did not program or erase the nRF52833 application flash.

## Restoration warning

Do not write `ficr_identity.bin`; FICR is factory-programmed and read-only.
Because both the original code flash and UICR were completely erased, restoring
the captured original state means erasing the nRF52833 code flash and UICR. Do
not perform that destructive operation unless restoration is explicitly needed
and the then-current firmware has first been backed up separately.
