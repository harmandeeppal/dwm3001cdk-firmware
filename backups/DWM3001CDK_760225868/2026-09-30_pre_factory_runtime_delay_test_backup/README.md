# LEFT-WRIST backup before factory runtime-delay test

- Board: DWM-03 LEFT-WRIST; registered J-Link serial `760225868`
- Target: `NRF52833_XXAA`; measured probe voltage: 3.300 V
- Purpose: preserve the temporary read-only OTP-reader image before installing
  the factory runtime-delay test image.

SEGGER J-Link Commander v9.24a read full flash and UICR twice. Both duplicate
pairs match byte-for-byte:

| Memory | Size | SHA-256 | Match |
|---|---:|---|---|
| Flash reads | 524,288 bytes each | `3ED67B323828EA63714ADAAB731E8FEC127C4CB2525C4D239D44594657CC0B50` | Yes |
| UICR reads | 4,096 bytes each | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` | Yes |
| FICR identity | 4,096 bytes | `33999BA266D84B2AF2B2D0D85C9BA56D92FFA6B5A1605EA20F00A99D57300362` | Saved |

No erase or programming command was used during the backup.
