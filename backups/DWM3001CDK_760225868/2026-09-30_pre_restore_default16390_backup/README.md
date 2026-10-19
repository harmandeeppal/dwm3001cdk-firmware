# LEFT-WRIST backup before restoring default antenna delay

- Board: DWM-03 LEFT-WRIST; registered J-Link serial `760225868`
- Target: `NRF52833_XXAA`; J-Link reference voltage: 3.300 V
- Purpose: preserve the temporary pair-5 factory-runtime-delay image before
  restoring the pair-5 reader/default-`16390` image.

SEGGER J-Link Commander v9.24a read full flash and UICR twice. The duplicate
reads match exactly by SHA-256:

| Memory | Size | SHA-256 | Match |
|---|---:|---|---|
| Flash reads | 524,288 bytes each | `28C0ECA5C0BC2666EAF91D283DACDE65261E9FC6127B6BB51EA78B4949027855` | Yes |
| UICR reads | 4,096 bytes each | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` | Yes |
| FICR identity | 4,096 bytes | `33999BA266D84B2AF2B2D0D85C9BA56D92FFA6B5A1605EA20F00A99D57300362` | Saved |

No erase or programming command was used while making this backup.
