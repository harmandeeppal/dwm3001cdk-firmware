# Explicit-command mesh build

These images contain the replacement mesh scheduler compiled on 2026-09-27.

- PELVIS: `DWM-02-PELVIS__explicit-command-mesh__760224787.hex`
- HEAD: `DWM-01-HEAD__explicit-command-mesh__760225805.hex`
- Target: nRF52833 / DWM3001CDK
- IMU: accelerometer and gyroscope at 60 Hz
- Magnetometer: disabled
- UWB: 15-pair full mesh

PELVIS broadcasts an explicit range or IMU command before each transaction.
The boards do not depend on a shared wall-clock schedule.

Build status: both images compiled successfully. PELVIS and HEAD have been
flashed and J-Link verified them. The live two-board exchange test is pending.
The previous shared-clock and address-filter-fix images are not used here.
