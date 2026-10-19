# Temporary PELVIS pair-5 calibration firmware

## Purpose

The current PELVIS coordinator runs every one of the 15 mesh pairs, even when
the experiment is trying to measure only PELVIS–LEFT-WRIST. This temporary
image limits the coordinator to pair index 5 (`PELVIS <-> LEFT-WRIST`) and
skips the separate remote-IMU-over-UWB polling loop. The existing per-board
BLE IMU stream remains in the source. The goal is to reduce unrelated UWB
failures and diagnostic traffic during a one-pair measurement.

This is a bench-only test image, not the final six-node firmware. Only PELVIS
will be changed. LEFT-WRIST and all other boards remain untouched.

## Build

- SDK: Nordic Connect SDK 3.4.0 / Zephyr 4.4.0
- Target: `nrf52833dk/nrf52833`
- Identity config: `ble_pelvis.conf`, node index 1, DK overlay
- Build-only option: `COMP971_UWB_PAIR_ONLY_INDEX=5`
- Build result: success, 219 steps
- Flash use: 144,748 bytes of 512 KiB (27.6%)
- HEX: `DWM-02-PELVIS__pair5-calibration-only__nrf52833.hex`
- SHA-256: `854D1F717E9A5426623052B1C12BA4E001F753B32E3D020E326EAA1B2D8DD128`

The normal full-mesh build remains unchanged when the pair-only CMake option
is not set.

## Safety and installation

- Registered PELVIS J-Link serial: `760224787`.
- Before flashing, a fresh read-only backup of current 512 KiB code flash,
  UICR, and FICR was saved under
  `hardware/backups/DWM3001CDK_760224787/2026-09-30_pre_pair5_calibration_backup/`.
  Two code-flash reads and two UICR reads match byte-for-byte. Hashes and read
  details are in that backup folder's README.
- Installed on 2026-09-30 to registered PELVIS J-Link serial `760224787`.
  `nrfjprog --program --sectorerase --verify --reset` completed with
  “Write successful” and “Verify successful”; exit code 0. SEGGER backend
  printed JLinkARM.dll error -256 warnings, but the operation completed and
  image verification passed.
- LEFT-WRIST was not flashed. No other wearable or the gateway was changed.
- After testing, restore the full-mesh PELVIS image using the saved current
  backup/build record before running all six boards together.

## Next test

Power PELVIS from its battery after disconnecting J9. Keep LEFT-WRIST powered
from its battery and the other boards off. Place the PELVIS and LEFT-WRIST
antennas 50 cm apart with a clear path, then start the live telemetry viewer
and a 45-second pair-5 capture. Confirm that pair 5 is reporting and that the
other 14 pairs are no longer scheduled by this temporary PELVIS image. This
test image is temporary and is not the six-node release firmware.

## Files changed

- `hardware/firmware/archive/dwm3001cdk_legacy_proto/src/main.c`
- `hardware/firmware/archive/dwm3001cdk_legacy_proto/zephyr/CMakeLists.txt`
