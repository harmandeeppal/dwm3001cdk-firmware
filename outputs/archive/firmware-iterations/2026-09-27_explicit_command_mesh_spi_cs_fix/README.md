# Explicit-command mesh with DWM3001CDK SPI-CS fix

These are the clean mesh images built with the DWM3001CDK-specific overlay.
The overlay selects the DW3110 chip-select at **P1.06**. The generic
nRF52833-DK default (`P0.16`) is not connected to the DWM3001CDK UWB device.

| Board | J-Link serial | Image |
|---|---:|---|
| DWM-02-PELVIS | 760224787 | `DWM-02-PELVIS__explicit-command-mesh-spi-cs-fix__760224787.hex` |
| DWM-01-HEAD | 760225805 | `DWM-01-HEAD__explicit-command-mesh-spi-cs-fix__760225805.hex` |

Both boards were flashed and verified. HEAD reports the expected boot identity
after reset, and PELVIS produces approximately 60 Hz ICM samples with clean
serial frames. The first ten-second two-board mesh test produced no valid UWB
ranges (all mesh reports were `-1`), so calibration is not yet valid and the
mesh exchange still needs diagnosis.

The 40 cm retest initially encountered a transient USB/application stall. A
clean, non-destructive PELVIS restart then produced 599 IMU samples in 10
seconds (about 60 Hz, no corrupt frames), but still produced 0 valid UWB
ranges and 899 non-positive mesh reports. This is not a valid calibration
measurement; the mesh exchange still needs diagnosis.

The complete PELVIS pre-flash backup is retained under
`hardware/outputs/diagnostics/2026-09-27_pelvis_pre_explicit_mesh_backup/`.

On 2026-09-28, a temporary direct initiator/responder diagnostic using the
same two boards produced 200 valid samples at 40 cm (median 39 cm, standard
deviation 2.80 cm). The mesh images in this folder were restored and J-Link
verified afterward.
