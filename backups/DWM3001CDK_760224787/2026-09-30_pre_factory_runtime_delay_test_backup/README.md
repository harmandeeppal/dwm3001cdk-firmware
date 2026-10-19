# PELVIS backup before factory runtime-delay test

- Board: DWM-02 PELVIS; registered J-Link serial `760224787`
- Target: `NRF52833_XXAA`; measured probe voltage: 3.300 V
- Purpose: preserve the temporary read-only OTP-reader image before installing
  the factory runtime-delay test image.

SEGGER J-Link Commander v9.24a read full flash and UICR twice. Both duplicate
pairs match byte-for-byte:

| Memory | Size | SHA-256 | Match |
|---|---:|---|---|
| Flash reads | 524,288 bytes each | `4E5EC80EEC845EBF8F7BD673171A2406D78600A2E05A2CA7C0858AC4476D6AB4` | Yes |
| UICR reads | 4,096 bytes each | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` | Yes |
| FICR identity | 4,096 bytes | `199C840A813AAC04F351683AE427EED55E5AE5D79DA7E84F8C46AED9BA8A893B` | Saved |

No erase or programming command was used during the backup.
