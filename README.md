# dwm3001cdk-firmware

**Project:** AUT COMP971 Dissertation — Multimodal Inertial-UWB Fusion for Real-Time Human Motion Forecasting and XR Telepresence  
**Repository Name:** `dwm3001cdk-firmware`  
**Status (19 October 2026):** **OPERATIONAL, STANDARDIZED & VERIFIED** — 6-node star telemetry deployed, empirical 2-node bench and on-body calibration accepted, 30-minute benchmark soak captured, Step 28B causal 60 Hz dataset validated (107,999 frames with 0.0 ppm drift), and 3-node live telemetry streaming at 60.16 Hz with zero packet loss.

This repository is the dedicated, self-contained embedded systems workspace for:
- **6 Body-Worn Motion Nodes** (Qorvo DWM3001CDK boards combining nRF52833 BLE MCU + DW3110 UWB transceiver + external ICM-20948 6-axis IMU)
- **1 Central Gateway Dongle** (Nordic nRF52840 USB Dongle receiving 6 concurrent BLE streams and piping binary telemetry to PC over USB CDC ACM)

---

## 1. Fast 3-Minute Quick Start

### Step 1: Pre-Flight Configuration Verification
Verify that all 6 physical board serial numbers, anatomical assignments, and telemetry schemas match the registry:
```powershell
.\tools\validate_hardware_config.ps1
```

### Step 2: Build & Safely Flash Firmware
Build the single unified firmware binary (`Debug_UNIFIED_star`) from source and flash any connected node (e.g., Head):
```powershell
# 1. Compile unified image (resolves node identity dynamically from silicon FICR)
.\tools\build_unified_motion_node.ps1

# 2. Safely program node (sector-erase protected; never touches factory antenna delay calibration)
.\tools\flash_motion_node.ps1 -Snr 760225805 -HexPath "C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_UNIFIED_star\DWM3001CDK-COMP971-FreeRTOS.hex" -NodeName HEAD
```

### Step 3: Stream Live Multi-Node Telemetry
Plug in the nRF52840 Gateway Dongle (`COM16`) and launch the live real-time console dashboard:
```powershell
.\tools\live_six_node_viewer.ps1 -Port COM16
```

---

## 2. Directory Layout

The workspace is organized into six clean, purpose-driven directories:

```
dwm3001cdk-firmware/
├── README.md               <-- This Guide: 3-minute quick start, SDK links, architecture, and safety rules
├── .gitignore              <-- Excludes temporary build files, IDE configs, vendor PDFs, and large decoded dumps
├── config/                 <-- Permanent device registry (J-Link serials), anatomical positions, and protocol specs
├── docs/                   <-- Chronological technical monographs (00 to 09) and reference reports
├── firmware/               <-- Editable firmware source code (No build artifacts)
│   ├── qorvo_qm33_motion_node/         <-- Production Wearable Nodes (Official Qorvo SDK 1.1.1 + FreeRTOS + S113)
│   ├── nrf52840_dongle_motion_gateway/ <-- Production Gateway Dongle (Zephyr RTOS + nRF Connect SDK)
│   └── archive/                        <-- Historical Phase 1 prototype & standalone checkout sketches
├── tools/                  <-- High-level operational CLI tools (Build, Flash, Stream, Decode, Validate)
│   ├── diagnostics/        <-- In-depth diagnostic runners, RAM dump parsers, and stall analyzers
│   ├── jlink/              <-- SEGGER J-Link Commander scripts for memory and register inspection
│   ├── pipelines/          <-- Intermediate dataset pipelines (Step 28, 28A) and regression tests
│   └── validation/         <-- Schema and hardware configuration validation modules
├── outputs/                <-- Empirical test records, calibration datasets, and verified firmware releases
│   ├── records/acquisition/<-- Raw binary stream captures (.bin) and decoded telemetry (.ndjson)
│   ├── records/calibration/<-- Empirical 2-node and on-body distance/yaw calibration records
│   ├── records/firmware/   <-- Verified release hex binaries and pre/post-flash verification readbacks
│   └── records/validation_step28b/ <-- Canonical Step 28B 60 Hz causal dataset (107,999 frames)
└── backups/                <-- Protected golden factory readbacks and OTP crystal/antenna delay backups
```

---

## 3. SDKs, APIs, Drivers & Build Toolchains

Building, testing, and flashing this firmware relies on the following official vendor SDKs, protocols, and toolchains:

### 3.1 Primary Embedded SDKs & Operating Systems

| Component / Layer | Version / Release | Role in System | Official Vendor Resources & Downloads |
|---|---|---|---|
| **Qorvo QM33 SDK** | v1.1.1 (`DW3_QM33_SDK`) | Hardware abstraction, DW3000/DW3110 radio driver (`uwbstack_libs`), and FiRa Consortium ranging engine. | [Qorvo DWM3001CDK Product Page & Software](https://www.qorvo.com/products/p/DWM3001CDK) |
| **Nordic nRF5 SDK** | v17.1.0 | Microcontroller abstraction layer (`nrfx`), TWIM EasyDMA drivers, and FreeRTOS kernel port for ARM Cortex-M4F. | [Nordic nRF5 SDK Downloads](https://www.nordicsemi.com/Products/Development-software/nRF5-SDK/Download) |
| **Nordic SoftDevice S113** | v7.3.0 | Qualified BLE peripheral protocol stack for the nRF52833 SoC (enables ATT MTU 247 bytes, 2 Mbps PHY, and 60 Hz notifications). | [Nordic SoftDevice S113 Architecture](https://www.nordicsemi.com/Products/Development-software/S113) |
| **Zephyr RTOS / nCS** | v2.7+ / nRF Connect SDK | High-performance multi-link BLE Central observer and USB CDC ACM virtual serial bridge running on the nRF52840 Dongle. | [Nordic nRF Connect SDK Documentation](https://www.nordicsemi.com/Products/Development-software/nRF-Connect-SDK) |
| **FreeRTOS Kernel** | v10.x (Nordic port) | Preemptive real-time task scheduler managing concurrent IMU acquisition, UWB scheduling, and BLE queueing on wearable nodes. | [FreeRTOS Official Kernel Documentation](https://www.freertos.org/) |

### 3.2 Low-Level Hardware Drivers & Protocol Layers

- **FiRa Consortium One-to-Many DS-TWR MAC Layer**:
  - Implements hardware-timed double-sided two-way ranging (DS-TWR) on UWB Channel 9 (center frequency 7987.2 MHz, 64 MHz PRF).
  - Configures a 200 ms block structure with 25 slots of 2 ms each, allowing 5 Controlees to range concurrently with the Pelvis coordinator in a 50 ms active round without inter-node blocking.
  - Reference: [FiRa Consortium Technical Specifications](https://www.firaconsortium.org/)
- **TDK InvenSense ICM-20948 TWIM1 I2C Driver**:
  - Custom EasyDMA driver operating the nRF52833 TWIM1 peripheral at 400 kHz.
  - Samples 6-axis motion data (accelerometer ±8g, gyroscope ±2000 dps) at an exact 60 Hz interrupt cadence into a 9-sample FreeRTOS queue without bus stalls.
  - Reference: [TDK InvenSense ICM-20948 Product Specification](https://invensense.tdk.com/products/motion-tracking/9-axis/icm-20948/)
- **COMP971 Framed Binary Transport Protocol v1**:
  - Standardized frame format (`0xC9 0x71`) encapsulating 60 Hz 9-sample IMU batches (206 bytes), UWB range reports (16 bytes), node diagnostic status, and coordinator health counters.
  - Full wire specification: [`docs/02_WIRE_PROTOCOL_AND_TELEMETRY_SPEC.md`](docs/02_WIRE_PROTOCOL_AND_TELEMETRY_SPEC.md).

### 3.3 Host Build Toolchains & Flashing Utilities

To build and flash the firmware from source, the host PC requires:

1. **ARM GNU Embedded Toolchain (`arm-none-eabi-gcc`)**:
   - Recommended: GCC Version 10.3-2021.10 or 12.3.rel1.
   - Target Architecture: `-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16`.
   - Download: [ARM GNU Toolchain Downloads](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
2. **CMake (Version 3.19 or higher)**:
   - Used for configuring the modular Qorvo/Nordic/COMP971 build tree.
   - Download: [CMake Official Website](https://cmake.org/download/)
3. **MinGW32-Make**:
   - Parallel make build tool (`mingw32-make.exe -j4`).
   - Download: [MSYS2 Distribution](https://www.msys2.org/)
4. **Nordic Command Line Tools (`nrfjprog`)**:
   - Hardware programming utility executing sector-erased flashing and UICR memory verification.
   - Download: [Nordic nRF Command Line Tools](https://www.nordicsemi.com/Products/Development-tools/nRF-Command-Line-Tools/Download)
5. **SEGGER J-Link Software Pack**:
   - Provides USB drivers for the onboard J-Link OB interface on each DWM3001CDK board.
   - Download: [SEGGER J-Link Downloads](https://www.segger.com/downloads/jlink/)

---

## 4. Firmware Architecture: Single Source of Truth

The firmware subsystem uses a **Unified Binary Architecture**:
- All six body-worn nodes compile from the **single authoritative codebase** in [`firmware/qorvo_qm33_motion_node/official_sdk_project/`](firmware/qorvo_qm33_motion_node/official_sdk_project/).
- At boot time, `comp971_identify_node()` queries the chip's read-only Factory Information Configuration Registers (`NRF_FICR->DEVICEID[0]` and `[1]`).
- The board automatically resolves its node index (`0` to `5`), UWB role (Controller vs. Controlee), UWB short address (`0x1001`–`0x1006`), and BLE advertising name (`COMP971-MOTION-HEAD`, etc.).
- **Preserved Release Binaries:** Pre-compiled, verified `.hex` binaries are permanently archived in `C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\` and `outputs/records/firmware/unified_releases/`.

---

## 5. Latest Flashed Firmware & SHA-256 Verification

The active compiled firmware in the build output archive matches the latest physical hardware flash verification readback bit-for-bit:

| Firmware Build | Build Timestamp | File Size | SHA-256 Hash | Status on Physical Hardware |
|---|---|---|---|---|
| **Wearable Node (`Debug_UNIFIED_star`)** | **11 October 2026, 23:25:47** | **849,561 bytes** | `FB584C95BF4A3A8A629B6B5305C594D86BC91B2047DF444AECFA83284B29AAA4` | **Active / Matched 100%** with Pelvis `diag-v2-step17-unified` flash verification readback |
| **Wearable Node Verified Backup (`_A40`)** | 10 October 2026, 22:40:47 | 843,666 bytes | `A138E19CC49D8E4D22139496C084264382EECCC691C7702463A7CA51DD8AA0E0` | Verified stable star release backup |
| **Gateway Dongle (`gateway_dfu_v8`)** | **8 October 2026, 23:07:50** | **146,069 bytes** | Verified DFU Package | **Active on COM16**, streaming binary framed v1 protocol (`0xC9 0x71`) |

---

## 6. Inviolable Flash Safety Rules

> [!CAUTION]
> **PERMANENT FACTORY CALIBRATION PROTECTION**:
> Flash Sectors 126 and 127 (`0x0007E000` to `0x0007FFFF`) contain factory crystal trim settings and antenna delay calibration constants loaded during manufacturing.
> - **NEVER EXECUTE `nrfjprog --chiperase`**.
> - **ALWAYS USE `flash_motion_node.ps1`** or invoke `nrfjprog` with `--sectorerase`:
>   ```powershell
>   nrfjprog --snr <JLINK_SERIAL> --program <HEX_PATH> --sectorerase --verify --reset
>   ```
> - Factory recovery dumps for all six physical boards are permanently backed up in [`backups/`](backups/).
