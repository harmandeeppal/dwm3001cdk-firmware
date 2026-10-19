# PELVIS backup before restoring default antenna delay

- Board: DWM-02 PELVIS; registered J-Link serial `760224787`
- Target: `NRF52833_XXAA`; J-Link reference voltage: 3.300 V
- Purpose: preserve the temporary pair-5 factory-runtime-delay image before
  restoring the pair-5 reader/default-`16390` image.

SEGGER J-Link Commander v9.24a read full flash and UICR twice. The duplicate
reads match exactly by SHA-256:

| Memory | Size | SHA-256 | Match |
|---|---:|---|---|
| Flash reads | 524,288 bytes each | `EC20AD66A3A9B0C4E612950EC0F6061C983CB568C15CF68972297E390D0462F2` | Yes |
| UICR reads | 4,096 bytes each | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` | Yes |
| FICR identity | 4,096 bytes | `199C840A813AAC04F351683AE427EED55E5AE5D79DA7E84F8C46AED9BA8A893B` | Saved |

No erase or programming command was used while making this backup.
