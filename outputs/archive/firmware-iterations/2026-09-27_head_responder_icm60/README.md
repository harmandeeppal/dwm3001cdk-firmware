# HEAD responder and ICM firmware

`DWM-01-HEAD` (J-Link `760225805`) was readback-protected. Its prior firmware could not be backed up. On 2026-09-27 the user approved recovery, which permanently erased that firmware.

The replacement image is a UWB responder with an external ICM-20948 raw stream at 60 Hz. It was programmed and verified successfully, then produced 299 clean ICM samples in a five-second live check.

UWB calibration is pending until the full six-board arrangement is ready.
