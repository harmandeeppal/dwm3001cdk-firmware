# Official QM33 SDK no-board build record

## Result

**Passed.** The unmodified official Qorvo QM33 SDK 1.1.1 built its exact
`DWM3001CDK-UCI-FreeRTOS` Release target on 1 October 2026.

This is a source and toolchain verification only. No DWM3001CDK was connected,
erased, programmed, reset or otherwise changed.

## Inputs

| Item | Value |
|---|---|
| Official archive | `hardware/official_docs/DW3_QM33_SDK_1.1.1.zip` |
| Archive verification | 623,516,600 bytes; valid ZIP; 17,284 entries |
| Nested source archive | `SDK/Firmware/DW3_QM33_SDK_1.1.1.zip` |
| Source verification | 139,459,794 bytes; valid ZIP; 17,241 entries |
| Build workspace | `C:\COMP971_QORVO_SDK_1_1_1` (a separate unchanged copy; no spaces) |
| Target | `Projects/FreeRTOS/UCI/DWM3001CDK` |
| Build type | Release |
| ARM compiler | GNU Arm Embedded Toolchain 10.3-2021.10 (10.3.1) |
| CMake | 4.4.3 |
| Python | 3.12.10 |
| Make | MinGW Make 4.4.1 |

## Generated artifacts

| File | Size | SHA-256 |
|---|---:|---|
| `DWM3001CDK-UCI-FreeRTOS.hex` | 727,814 bytes | `847EB8F07DE3042DEBC65DE260073E43E793C3390518072E43C4A8422C90E89C` |
| `DWM3001CDK-UCI-FreeRTOS.bin` | 258,740 bytes | `0319CB58F06C0CA522CDED2585F635DA2472C5DE0045B92DA0272515FE9144C6` |
| `DWM3001CDK-UCI-FreeRTOS.elf` | 522,392 bytes | Build output; not a flashing input |
| `DWM3001CDK-UCI-FreeRTOS.map` | 1,245,104 bytes | Linker map; not a flashing input |

The official linker reported 107,420 B / 128 KiB RAM (81.95%) and 256,572 B /
504 KiB flash (49.71%). It also allocates the SDK calibration partition.

## Safety boundary

The `.hex` exists only in the separate build workspace. It must not be flashed
until the two-board baseline phase begins: a fresh full-flash and UICR backup
of the named board, explicit recording of the target board, a known-distance
test plan, and a restoration image must all be ready first.

See [`QORVO_SDK_SOURCE_REVIEW.md`](../../../../docs/08_QORVO_SDK_SOURCE_REVIEW.md)
and [`OFFICIAL_SDK_INTEGRATION_STRATEGY.md`](../../../../docs/07_OFFICIAL_SDK_INTEGRATION_STRATEGY.md).
