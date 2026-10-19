# Direct official FiRa two-node pilot

## Purpose

This is an offline-built replacement for the earlier QANI-based PELVIS/HEAD
pilot. It is **superseded** by the startup-order correction in
`../2026-10-02_direct_official_fira_startup_order_fix/`. It is not a validated
final firmware.

## Why this build exists

The earlier pilot successfully created and started a Qorvo UWB session, but it
produced no ranging reports. That pilot used Qorvo's QANI sample layer, which
is the Apple Nearby Interaction demonstration path.

This build calls Qorvo's normal direct FiRa APIs instead. Its lifecycle follows
the official `Src/Apps/Src/fira/fira_app.c` path:

1. `fira_uwb_mcps_init()` initialises the Qorvo platform, L1 configuration,
   low-level hardware driver and UWB-MAC.
2. `fira_helper_open()` and `fira_helper_set_scheduler()` configure the FiRa
   session scheduler.
3. `fira_prepare_measurement_sequence(..., true)` requests normal range
   measurement reporting.
4. The application creates the session, sets session parameters, adds the
   controller's peer, starts UWB-MAC, then starts the FiRa session.

## What remains official and unchanged

The build still uses the frozen Qorvo QM33 SDK v1.1.1 for all low-level work:

- DWM3001CDK board support and pin configuration;
- DWM3001C/DW3110 driver and UWB-MAC;
- Qorvo L1 initialisation, including OTP-derived configuration and calibration;
- Nordic SoftDevice and Qorvo FreeRTOS integration.

It does not write UICR, OTP, antenna delay, calibration, NVM, or runtime
flash. The direct FiRa settings are RAM-only.

## Fixed pilot roles

| Board | J-Link serial | Role | Short address |
|---|---:|---|---:|
| PELVIS | 760224787 | controller / initiator | 0 |
| HEAD | 760225805 | controlee / responder | 1 |

## Direct FiRa session used for the pilot

- Channel 9
- BPRF, SP3, SFD 2, preamble code 10
- Deferred DS-TWR
- 2.4 ms slot duration
- 200 ms block duration
- 25 slots per ranging round
- Session ID 42
- Normal range-report measurement sequence enabled

These are Qorvo's general direct FiRa defaults. They are deliberately separate
from the earlier official UCI baseline, which proved channel-9 hardware ranging
on PELVIS and LEFT-WRIST but used the UCI host interface.

## Images and verification

| Image | SHA-256 | Build result |
|---|---|---|
| `DWM-02-PELVIS__direct-fira-controller__760224787.hex` | `313FB5AB0DB8E243903A73F5703FEC24A4FACB0E6A0A5CDEE6CE5C93F367F2D8` | compiled successfully |
| `DWM-01-HEAD__direct-fira-responder__760225805.hex` | `51D87F5E963D9B3C5CBDB76F6907B4C6A15DCD0BEB5763B9D6B8774DDE3A31E4` | compiled successfully |

The PELVIS image map was checked after the build: it contains the direct
FiRa start task and Qorvo UWB-MAC/FiRa symbols, and does not contain the old
QANI start functions (`StartUWB`, `CreateQaniTask`, or `niq_init`).

## Programming record

### PELVIS — complete

- Date: 2 October 2026
- Board: DWM-02 PELVIS, J-Link serial `760224787`
- Image: `DWM-02-PELVIS__direct-fira-controller__760224787.hex`
- SHA-256 checked before programming:
  `313FB5AB0DB8E243903A73F5703FEC24A4FACB0E6A0A5CDEE6CE5C93F367F2D8`
- J-Link result: **Program & Verify succeeded**.
- Affected normal flash: one range at `0x00000000`, `225,280` bytes.
- No chip erase command was issued. The tool erased only the application flash
  pages required for that image. No UICR, OTP, antenna-delay, calibration, NVM
  or runtime-flash write was issued.

### HEAD — complete

- Date: 2 October 2026
- Board: DWM-01 HEAD, J-Link serial `760225805`
- Image: `DWM-01-HEAD__direct-fira-responder__760225805.hex`
- SHA-256 checked before programming:
  `51D87F5E963D9B3C5CBDB76F6907B4C6A15DCD0BEB5763B9D6B8774DDE3A31E4`
- J-Link result: **Program & Verify succeeded**.
- Affected normal flash: one range at `0x00000000`, `225,280` bytes.
- No chip erase command was issued. The tool erased only the application flash
  pages required for that image. No UICR, OTP, antenna-delay, calibration, NVM
  or runtime-flash write was issued.

## Test outcome and replacement

The two-board 59-second test delivered both BLE/ICM streams but showed `UWB
not started` on both boards, with zero UWB attempts and no UWB diagnostics.
The source review then found that Qorvo initialises its UWB platform before
BLE/SoftDevice. The corrected images and full reason are in
[`direct official FiRa startup-order correction`](../2026-10-02_direct_official_fira_startup_order_fix/README.md).

## Next controlled test

Only after explicit approval:

1. Program the normal application image to PELVIS through J9.
2. Program the normal application image to HEAD through J9.
3. Power both from battery at a measured 100 cm separation, with the BLE
   gateway connected.
4. After a 20-second settling period, run the single live quality viewer for
   60 seconds:

   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File ".\hardware\tools\live_gateway_binary_viewer.ps1" -Port COM16 -DurationSeconds 60 -WarmupSeconds 20 -Pair 0 -KnownDistanceCm 100
   ```

5. Accept the pilot only if pair 0 reports ranges and the PELVIS counters show
   non-zero UWB attempts and results.

The first test is deliberately two boards and one pair. It must work before
the 15-pair schedule is introduced.
