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

The workspace is organized into seven clean, purpose-driven directories:

```
dwm3001cdk-firmware/
├── README.md               <-- This Guide: 3-minute quick start, hardware map, and safety rules
├── .gitignore              <-- Excludes temporary build files, IDE configs, and large decoded dumps
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
├── backups/                <-- Protected golden factory readbacks and OTP crystal/antenna delay backups
└── official_docs/          <-- Official Qorvo datasheets, application notes, and vendor reference materials
```

---

## 3. Firmware Architecture: Single Source of Truth

The firmware subsystem uses a **Unified Binary Architecture**:
- All six body-worn nodes compile from the **single authoritative codebase** in [`firmware/qorvo_qm33_motion_node/official_sdk_project/`](firmware/qorvo_qm33_motion_node/official_sdk_project/).
- At boot time, `comp971_identify_node()` queries the chip's read-only Factory Information Configuration Registers (`NRF_FICR->DEVICEID[0]` and `[1]`).
- The board automatically resolves its node index (`0` to `5`), UWB role (Controller vs. Controlee), UWB short address (`0x1001`–`0x1006`), and BLE advertising name (`COMP971-MOTION-HEAD`, etc.).
- **Preserved Release Binaries:** Pre-compiled, verified `.hex` binaries are permanently archived in `C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\` and `outputs/records/firmware/unified_releases/`.

---

## 4. Latest Flashed Firmware & SHA-256 Verification

The active compiled firmware in the build output archive matches the latest physical hardware flash verification readback bit-for-bit:

| Firmware Build | Build Timestamp | File Size | SHA-256 Hash | Status on Physical Hardware |
|---|---|---|---|---|
| **Wearable Node (`Debug_UNIFIED_star`)** | **11 October 2026, 23:25:47** | **849,561 bytes** | `FB584C95BF4A3A8A629B6B5305C594D86BC91B2047DF444AECFA83284B29AAA4` | **Active / Matched 100%** with Pelvis `diag-v2-step17-unified` flash verification readback |
| **Wearable Node Verified Backup (`_A40`)** | 10 October 2026, 22:40:47 | 843,666 bytes | `A138E19CC49D8E4D22139496C084264382EECCC691C7702463A7CA51DD8AA0E0` | Verified stable star release backup |
| **Gateway Dongle (`gateway_dfu_v8`)** | **8 October 2026, 23:07:50** | **146,069 bytes** | Verified DFU Package | **Active on COM16**, streaming binary framed v1 protocol (`0xC9 0x71`) |

---

## 5. Inviolable Flash Safety Rules

> [!CAUTION]
> **PERMANENT FACTORY CALIBRATION PROTECTION**:
> Flash Sectors 126 and 127 (`0x0007E000` to `0x0007FFFF`) contain factory crystal trim settings and antenna delay calibration constants loaded during manufacturing.
> - **NEVER EXECUTE `nrfjprog --chiperase`**.
> - **ALWAYS USE `flash_motion_node.ps1`** or invoke `nrfjprog` with `--sectorerase`:
>   ```powershell
>   nrfjprog --snr <JLINK_SERIAL> --program <HEX_PATH> --sectorerase --verify --reset
>   ```
> - Factory recovery dumps for all six physical boards are permanently backed up in [`backups/`](backups/).
