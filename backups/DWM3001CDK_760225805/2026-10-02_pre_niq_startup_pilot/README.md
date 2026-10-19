# HEAD pre-NIQ-startup-pilot backup

Date: 2 October 2026  
Board: HEAD, J-Link serial `760225805`

## Scope and safety

This is a complete, verified, read-only backup made before any proposed
NIQ-startup-pilot flash. SEGGER J-Link Commander V9.72 used only `savebin`.
There was no reset, erase, program, UICR write, OTP write, antenna-delay
write, calibration write, or runtime flash write.

J-Link reported target voltage `3.300 V`.

## Verification

| Region | Reads | SHA-256 | Result |
|---|---:|---|---|
| Application flash `0x00000000`--`0x0007FFFF` (512 KB) | 2 | `BF4F615CA4105AB129B3FA5C6D5E5BDEF39757F6120CC0204EE9867DABFDFE3F` | Identical |
| UICR `0x10001000`--`0x10001FFF` (4 KB) | 2 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` | Identical |

Files:

- `flash_read_1_0x00000000_0x0007ffff.bin`
- `flash_read_2_0x00000000_0x0007ffff.bin`
- `uicr_read_1_0x10001000_0x10001fff.bin`
- `uicr_read_2_0x10001000_0x10001fff.bin`

This backup preserves the current HEAD state. A later normal application
`loadfile` remains a separate action requiring explicit approval.
