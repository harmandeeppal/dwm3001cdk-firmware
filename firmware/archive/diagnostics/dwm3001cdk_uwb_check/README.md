# DWM3001CDK DW3110 configuration-derived health probe

This standalone Zephyr application checks communication between the nRF52833
and the onboard DW3110 UWB transceiver. It reads the DW3110 device ID over the
board's internal SPI3 connection.

The expected ID, selected device identity, J-Link serial, serial baud, and
diagnostic timing are generated from JSON by
`scripts/diagnostics/prepare_dw3110_probe_config.py`. The source has no default target and
will not build until `include/generated_probe_config.h` is generated.

On success, the application emits the configured startup identity, prints
`PASS`, turns on D9, and emits periodic status lines. This check does not
transmit UWB radio packets and does not establish UWB ranging, IMU operation,
or calibration.
