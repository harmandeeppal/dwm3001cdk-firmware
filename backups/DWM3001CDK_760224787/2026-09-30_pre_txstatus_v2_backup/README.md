# PELVIS pre-v2 diagnostic firmware backup

## Read and identity check

- Board: DWM-02-PELVIS
- Registered J-Link serial: `760224787`
- Read date: 2026-09-30
- Tool: SEGGER J-Link Commander V9.72, SWD at 4000 kHz
- J-Link reported `VTref=3.300V` and the expected serial number.
- The 512 KiB code flash and 4 KiB UICR were each read twice; both duplicate
  pairs are byte-for-byte identical. FICR identity was also saved.
- The existing v1 PELVIS pair-5 diagnostic HEX verified against the board
  before the v2 update (nrfjprog verify exit code 0).

## SHA-256

| File | Bytes | SHA-256 |
|---|---:|---|
| `pelvis_flash_read1.bin` | 524,288 | `07DD8A6D81825E0BD94BB86015A268F73B9C4ED170A0E976DC143E15EB9EC568` |
| `pelvis_flash_read2.bin` | 524,288 | `07DD8A6D81825E0BD94BB86015A268F73B9C4ED170A0E976DC143E15EB9EC568` |
| `pelvis_uicr_read1.bin` | 4,096 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |
| `pelvis_uicr_read2.bin` | 4,096 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |
| `pelvis_ficr_identity.bin` | 4,096 | `199C840A813AAC04F351683AE427EED55E5AE5D79DA7E84F8C46AED9BA8A893B` |

Keep these files unchanged. They preserve the board state immediately before
installing the v2 pair-only diagnostic image. The exact read commands are in
`read_pelvis_before_txstatus_v2.jlink`.
