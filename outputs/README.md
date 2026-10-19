# Hardware evidence records

This directory contains preserved evidence. It is not a source or build
directory.

- `records/` — measurement captures that remain relevant to the current work.
- `archive/diagnostics/` — past component diagnostics, probes, and readback
  evidence.
- `archive/firmware-iterations/` — superseded firmware trial images and their
  associated test output. None is an approved fleet release.
- `archive/environment/` — environment-recovery evidence from tooling setup.

The controlled Pelvis/Left-Wrist UWB captures in `records/calibration/` show
that UWB transport works but the reported range is not yet accurate enough to
calibrate. See `../docs/CURRENT_STATUS.md`.

The current pair-5 failure investigation is documented in:

- `records/calibration/2026-09-30_pair5_txstatus_both_50cm/` — latest 60 s
  capture with both pair nodes' v1 diagnostic images; retain it as the
  pre-v2 baseline.
- `records/firmware/2026-09-30_pair5_pre_recovery_tx_status_v2/` — matched
  pre-recovery status images installed on PELVIS and LEFT-WRIST. The first
  50 cm capture had no valid ranges and no LEFT-WRIST telemetry; see
  `records/calibration/2026-09-30_pair5_pre_recovery_v2_50cm/`.
- `records/calibration/2026-09-30_pair5_pre_recovery_v2_50cm_after_wrist_restart/`
  — the repeat after restarting LEFT-WRIST restored its 60 Hz IMU stream, but
  the pair still had a 36.0% invalid-attempt rate and inaccurate, widely spread
  valid ranges. Both boards logged delayed-transmit warning evidence; UWB
  calibration remains on hold.
- `records/firmware/2026-09-30_pair5_delayed_tx_margin_v3/` — temporary
  PELVIS and LEFT-WRIST diagnostic images built with 5 ms responder and 6 ms
  final-frame delays. They are archived and verified at build level but have
  **not** been flashed. The next step is to back up the current boards, then
  install one image at a time and repeat the same pair test.

When a new test is run, add it as a dated subfolder under `records/` rather
than adding loose files here. Do not save CMake, PlatformIO, Zephyr, or Python
cache output in this directory.
