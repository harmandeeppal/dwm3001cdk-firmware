# Wearable BLE ICM-20948 peripheral test

This is a temporary, one-board diagnostic for one named DWM3001CDK. It reads the
external ICM-20948 at 60 Hz and publishes one to three samples per custom BLE
GATT notification using the shared version-1 packet contract.

The default is DWM-02-PELVIS, node index 1. The target name and node index are
selected in a small configuration file under `config/` at build time.

It intentionally has no UWB functionality. Flashing it temporarily replaces
the target board's UWB test firmware. It must be used only after an explicitly
named flash approval. The board must later be restored to a known UWB image
before UWB testing resumes.
