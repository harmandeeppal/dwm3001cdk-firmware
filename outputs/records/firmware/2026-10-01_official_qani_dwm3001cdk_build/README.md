# Official QANI DWM3001CDK reference build

## Purpose

This is a **software-only** verification of Qorvo's unmodified QM33 SDK
`QANI` project for the DWM3001CDK.  It establishes that the official project
which includes the DWM3001CDK radio target, FreeRTOS and Nordic BLE stack can
be built in the COMP971 development environment.

It is not a wearable firmware release and was not written to any board.

## Source and build target

| Item | Value |
|---|---|
| Official SDK | DW3/QM33 SDK 1.1.1 |
| Official source root | `C:\COMP971_QORVO_SDK_1_1_1` |
| Qorvo project | `Projects/FreeRTOS/QANI/DWM3001CDK` |
| Board | DWM3001CDK |
| MCU | nRF52833 XXAA |
| Operating system | FreeRTOS |
| Bluetooth base | Nordic S113 SoftDevice, supplied by Qorvo project |
| Result | clean build succeeded on 1 October 2026 |

## Generated artifacts

The artifacts remain in the separate, unmodified official-SDK working copy:

`C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\QANI\FreeRTOS\DWM3001CDK\Debug`

| Artifact | SHA-256 |
|---|---|
| `DWM3001CDK-QANI-FreeRTOS.hex` | `E89D4EF57106CF5171B1E137A71430A93A012DEDA50C2D8C0247FBF0D28CEA4E` |
| `DWM3001CDK-QANI-FreeRTOS.bin` | `D73C789E732478BDEDB180FFAB021BED79F03A21E586D6FFAD8793BEBFEB2CC1` |
| `DWM3001CDK-QANI-FreeRTOS.elf` | `37CD89B2EB022786A9D10BB168F3DCF081AA2DD61E9844D031434B8C46E0A5E0` |

## What this proves

- The official DWM3001CDK target and its radio/board configuration compile.
- The official BLE/SoftDevice project layer compiles with that target.
- COMP971 can now use this project as the controlled reference for the final
  application port.

## What it does not prove

- It is not compatible with the COMP971 nRF52840 gateway.
- It does not read the external ICM-20948.
- It does not run the COMP971 15-pair body mesh.
- It was **not flashed** to PELVIS, LEFT-WRIST, or any other board.

QANI is Qorvo's Apple Nearby Interaction application.  Flashing it would
replace the current official UCI test firmware and would not deliver the
wearable system.  It is therefore retained as a safe source/build reference,
not used as a test image.
