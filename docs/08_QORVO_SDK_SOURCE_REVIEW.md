# Qorvo QM33 SDK 1.1.1 source review

## Purpose

This is a read-only review of Qorvo's official source for the exact
**DWM3001CDK** target. It records what the final COMP971 firmware must retain
from the official SDK. No wearable board was connected, changed, erased or
flashed for this review.

## Reviewed package

| Item | Result |
|---|---|
| Outer archive | `official_docs/DW3_QM33_SDK_1.1.1.zip` |
| Outer archive check | Valid ZIP; 623,516,600 bytes; 17,284 entries |
| Nested source archive | `SDK/Firmware/DW3_QM33_SDK_1.1.1.zip` |
| Nested source check | Valid ZIP; 139,459,794 bytes; 17,241 entries |
| Exact target | `Projects/FreeRTOS/UCI/DWM3001CDK` |
| Exact board layer | `Src/Boards/Src/DWM3001CDK` |

The source package README requires ARM GNU Toolchain 10.3-2021.10, CMake newer
than 3.23, Python 3.10 or newer, MinGW `make`, and SEGGER J-Link on Windows.
The SDK must be placed in a directory path with **no spaces** before its own
build scripts are used. This project path contains spaces, so the SDK will be
copied unchanged to a short, no-space build workspace when the implementation
phase begins. The original downloaded archive remains preserved here.

## What Qorvo's DWM3001CDK source does at the low level

The board-specific source file
`Src/Boards/Src/DWM3001CDK/platform_l1_config.c` does all of the following:

1. reads the DWM3001C platform and calibration revision from OTP word `0x1F`;
2. selects the board's TX and RX antenna paths;
3. loads channel-5 and channel-9 antenna delays from OTP words `0x1A` and
   `0x1C`;
4. loads channel-5 and channel-9 TX power from OTP words `0x11` and `0x13` and
   converts them into the SDK's TX-power index;
5. loads crystal trim from OTP word `0x1E`;
6. loads pulse-generator count from OTP word `0x18`; and
7. enables adaptive TX-power control.

The code also has a Qorvo revision-specific condition: for DWM3001C calibration
revision `PVT2` on channel 5, it uses Qorvo's characterised common value
`16366` rather than the raw OTP antenna-delay word. That rule cannot safely be
replaced by a guessed constant. It is one reason to migrate the low-level
configuration into the official SDK rather than continue trying single
antenna-delay constants in the present firmware.

The SDK's L1 configuration manager validates and stores this configuration in
non-volatile memory. Qorvo's UCI target creates the UWB configuration manager
and UCI task around that layer. This is different from simply calling the
DW3xxx driver directly.

## Direct comparison

| Concern | Present COMP971 firmware | Official SDK source | Final direction |
|---|---|---|---|
| Application base | Zephyr application with direct DW3xxx-driver calls | FreeRTOS DWM3001CDK UCI target | Use the official DWM3001CDK SDK as the base application |
| PHY | Fixed compile-time channel-5, 128-preamble, code-9, 6.8 Mb/s settings | SDK-managed UWB configuration | Retain a validated motion PHY, but configure it through Qorvo's interfaces |
| Antenna delay | Fixed fallback TX/RX value `16390` | OTP plus calibration-revision logic, separately for TX/RX paths | Let Qorvo board code select defaults; only apply a controlled, documented calibration afterwards |
| TX power | Fixed `0xfdfdfdfd` and pulse-generator count `0` | Board OTP values, conversion to SDK indices, adaptive power control | Preserve official board configuration |
| Crystal trim | Not explicitly loaded by the custom application | Loaded from OTP when present | Preserve official board configuration |
| Ranging protocol | Custom double-sided TWR and custom 15-pair schedule | UCI/FiRa-capable official transport and configuration framework | Add the COMP971 mesh above the official low-level layer; do not use the stock UCI image as the final wearable application |
| IMU and BLE telemetry | Custom ICM-20948 and nRF52840 gateway protocol | Not part of the stock UCI sample | Port these custom features after the official UWB base is working |

## What this does and does not prove

This review proves that the official source contains a complete, current
DWM3001CDK board configuration path and that it differs materially from the
current fixed-constant setup.

It does **not** prove that the existing boards are faulty, that the official
source will immediately make every range accurate, or that a calibration value
should be written now. The observed range error can still involve the custom
TWR calculation, antenna surroundings, case/battery placement, test geometry,
or calibration. The official two-board baseline separates those causes.

## No-board build verification

On 1 October 2026, the unmodified official
`DWM3001CDK-UCI-FreeRTOS` Release target compiled successfully in the separate
no-space workspace `C:\COMP971_QORVO_SDK_1_1_1`. It used GNU Arm Embedded
Toolchain 10.3-2021.10, CMake 4.4.3, Python 3.12.10 and MinGW Make 4.4.1.

The generated `.hex` is an official **baseline test image**, not the final
COMP971 firmware and not an approved flash instruction. Its SHA-256 is
`847EB8F07DE3042DEBC65DE260073E43E793C3390518072E43C4A8422C90E89C`.
No J-Link command was run and no board was connected or written during this
work. The complete record is in
`../outputs/records/firmware/2026-10-01_official_qm33_sdk_build/`.

## Next safe engineering step

Finish the source-only migration design: create an unchanged official SDK build
workspace, build the supplied DWM3001CDK UCI target without a board, and record
its cryptographic checksum and build output. Only after that will the separate
two-board baseline be prepared. That later phase requires fresh backups and
the user's explicit permission for the named PELVIS and LEFT-WRIST boards.
