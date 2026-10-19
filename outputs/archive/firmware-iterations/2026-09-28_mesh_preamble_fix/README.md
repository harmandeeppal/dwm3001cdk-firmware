# 2026-09-28 mesh preamble-fix build

This build changes only the mesh receiver startup path. It explicitly disables
the DW3110 preamble-detection timeout before the receiver waits for mesh slots,
matching the standalone responder that was verified in the direct two-board
test. The four other node images were not changed.

## Images

- `DWM-01-HEAD__mesh-preamble-fix__760225805.hex` — mesh node index 0
- `DWM-02-PELVIS__mesh-preamble-fix__760224787.hex` — mesh node index 1

Build settings for both images:

- Nordic nRF52833 / Zephyr 4.4.0 (NCS v3.4.0)
- DW3110 SPI chip select P1.06
- UART telemetry 460800 baud
- IMU stream 60 Hz
- TX/RX antenna delay 16378
- Responder delay 1000 UUS

## Safety and verification

The existing flash backups and the previously verified mesh images remain in
their dated folders. Flash only the matching serial-number script:

- HEAD: `flash_DWM-01-HEAD__mesh-preamble-fix__760225805.jlink`
- PELVIS: `flash_DWM-02-PELVIS__mesh-preamble-fix__760224787.jlink`

After flashing, test only the HEAD/PELVIS pair at 40 cm first. Expected output
is approximately 60 IMU samples per second and valid UWB pair-0 ranges. Do not
flash the other four boards until this pair passes.
