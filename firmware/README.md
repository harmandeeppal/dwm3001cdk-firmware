# COMP971 Firmware Architecture & Repository Layout

This directory houses the firmware source code for the COMP971 wearable sensor network and the central USB gateway.

---

## 1. Directory Overview

| Directory | Subsystem | Stack & Framework | Status |
|---|---|---|---|
| **[`qorvo_qm33_motion_node/`](qorvo_qm33_motion_node/)** | 6 Body-Worn Wearable Motion Nodes | Official Qorvo QM33 SDK 1.1.1 + FreeRTOS + Nordic SoftDevice S113 + FiRa UWB MAC | **Authoritative Production Source** (Single Source of Truth) |
| **[`nrf52840_dongle_motion_gateway/`](nrf52840_dongle_motion_gateway/)** | Central USB Receiver Dongle (`COM16`) | Zephyr RTOS + nRF Connect SDK + Bluetooth Central + USB CDC ACM | **Authoritative Gateway Source** |
| **`archive/`** | Historical Prototypes & Bring-Up Sketches | PlatformIO / Early Zephyr / Standalone SPI Checkouts | Archived (September 2026 Phase 1) |

---

## 2. Production Wearable Firmware: Unified Binary Architecture

All six body-worn nodes (Head, Pelvis, Left Wrist, Right Wrist, Left Ankle, Right Ankle) compile from the **single authoritative codebase** in [`qorvo_qm33_motion_node/official_sdk_project/`](qorvo_qm33_motion_node/official_sdk_project/).

### Runtime Auto-Discovery via Silicon Registers
Instead of maintaining six separate firmware branches, the firmware uses a **Unified Binary Architecture**:
- At power-on, `comp971_identify_node()` queries the Nordic chip's read-only Factory Information Configuration Registers (`NRF_FICR->DEVICEID[0]` and `DEVICEID[1]`).
- It matches the silicon ID against `g_comp971_node_table` in ROM to dynamically bind:
  - Node Index (`0` to `5`)
  - Anatomical Position Name (e.g. `COMP971-MOTION-HEAD`)
  - UWB Role (Controller vs. Controlee)
  - UWB Short Address (`0x1001` to `0x1006`)
  - Staggered Reconnect Jitter (avoids connection storms)

### Compilation & Safe Programming
- **Build Command:**
  ```powershell
  .\hardware\tools\build_unified_motion_node.ps1 -Clean
  ```
- **Flashing Command:**
  ```powershell
  .\hardware\tools\flash_motion_node.ps1 -Snr 760225805 -HexPath <HEX_PATH> -NodeName HEAD
  ```
  *(Enforces `--sectorerase` only; sectors 126–127 containing factory radio crystal trims and antenna delays are permanently guarded).*

---

## 3. Production Gateway Dongle Firmware

Located in [`nrf52840_dongle_motion_gateway/`](nrf52840_dongle_motion_gateway/):
- Scans and establishes concurrent BLE connections with all 6 wearable nodes.
- Receives 9-sample 60 Hz IMU batches (206 bytes) and UWB ranging reports.
- Packages all packets into binary framed protocol v1 (`0xC9 0x71`) and streams them to the PC over high-speed USB CDC ACM at 115,200 baud.
- The compiled Nordic DFU package is archived as `gateway_dfu_v8.zip`.

---

## 4. Archive Directory

- **`archive/dwm3001cdk_legacy_proto/`**: Early PlatformIO 2-board SS-TWR sanity check prototype from September 2026.
- **`archive/diagnostics/`**: Standalone I2C, SPI, and register bring-up sketches used during initial PCB assembly.
