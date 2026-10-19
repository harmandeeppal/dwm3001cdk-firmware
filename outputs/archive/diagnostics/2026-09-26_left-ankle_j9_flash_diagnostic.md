# DWM-05-LEFT-ANKLE J9 flash diagnostic

Date: 2026-09-26

## Board

- Project name: `DWM-05-LEFT-ANKLE`
- J-Link serial number: `760225018`
- Target: `nRF52833_xxAA`
- Connection: onboard J-Link through J9
- Target voltage reported by J-Link: `3.300 V`

## Known-good application to restore

`hardware/outputs/dwm3001cdk_motion_node/2026-08-30_two_board_range_test/DWM-01-HEAD__range-initiator__760225018.hex`

The historical filename uses `HEAD`; the current project name for this physical board is **LEFT-ANKLE**.

## Checks completed

1. SWD target detection succeeds at 100 kHz.  The ARM Cortex-M4 and nRF52833 FICR are readable.
2. The application flash at `0x00000000` is blank (`0xFFFFFFFF`).
3. The NVMC flash controller reports ready (`NVMC.READY = 1`) in normal read-only mode (`NVMC.CONFIG = 0`).
4. J-Flash Lite v9.72, configured with `NRF52833_XXAA`, `SWD`, `4000 kHz`, and internal flash only, failed before downloading the known-good image.
5. A controlled one-word write test in unused blank flash at `0x0007F000` was attempted only after approval.  It failed before the write; subsequent read confirmed that location remains `0xFFFFFFFF`.
6. The NVMC controller was returned to normal read-only mode and rechecked: `READY = 1`, `CONFIG = 0`.

## Repeated programming failure

The J-Link programming path reports:

```text
Failed to preserve target RAM @ 0x20000000-0x2001FFFF.
Failed to prepare for programming.
```

This occurs while trying to use the flash programming mechanism, not while identifying or reading the target.

## Safe state now

- No UWB firmware was written.
- No saved historical file was modified.
- The application flash remains blank.
- The unused test page remains blank.
- Do not use **Erase Chip** or attempt further J-Flash programming until a supported recovery path is identified.

