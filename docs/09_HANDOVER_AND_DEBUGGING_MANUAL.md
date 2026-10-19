# Agent Handover Manual: UWB FiRa Two-Node Telemetry Integration

**Target Audience**: Any AI Agent, Systems Engineer, or Embedded Developer taking over this project.  
**Project**: COMP971 Dissertation (60 points) — Dual Wearable Motion Capture & Ultra-Wideband (UWB) Telemetry  
**Primary Goal**: Establish real-time Ultra-Wideband (UWB) two-node FiRa ranging between two DWM3001CDK boards (~30 cm separation) concurrently with dual 60 Hz ICM-20948 IMU telemetry, streamed through the nRF52840 USB Gateway Dongle (`COM16`), with the physical Green LED (`D11` / `P0.22`) flashing rhythmically on active ranging.

---

## 1. Physical Hardware Setup & Port Mapping

| Node / Device | Serial Number | Physical Board | J-Link COM Port | Native USB Port | Architecture Role & Short Addr |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Node 0 (HEAD)** | `760225805` | DWM3001CDK | **COM13** | J20 (unplugged) | Controller / Initiator (`0x0002`) |
| **Node 1 (PELVIS)** | `760224787` | DWM3001CDK | **COM6** | J20 (unplugged) | Controlee / Responder (`0x0001`) |
| **Gateway** | N/A | nRF52840 Dongle | **COM16** | Native USB | Central BLE Receiver & USB Binary Bridge |

*Physical Connection Note*: Both DWM3001CDK boards are connected strictly via their **J2** micro-USB connectors (J-Link On-Board debugger with VCOM UART). The secondary **J20** micro-USB port (native nRF52833 USB) is completely unpopulated.

---

## 2. Inviolable Governance & Safety Protocols

> [!CAUTION]
> **FLASH CALIBRATION PROTECTION RULE**:
> NEVER execute `nrfjprog --chiperase` under ANY circumstance.
> ALWAYS execute `nrfjprog --sectorerase --verify --reset`.
> The top 8 KiB of flash (`0x0007E000` to `0x0007FFFF`) contains factory-calibrated crystal trim, golden SHA hashes, and antenna delay constants that cannot be recovered if erased.

> [!IMPORTANT]
> **NO LATEX FORMATTING CONSTRAINT**:
> Absolutely DO NOT use LaTeX math syntax (`$`, `$$`, `\text{}`, `\times`, etc.). The target UI does not support MathJax or KaTeX and renders broken raw strings. Format all math, formulas, and registers using standard Markdown backticks, Unicode characters, and plain text.

### Golden Backups on Disk:
* [`hardware/outputs/records/firmware/2026-10-04_two_node_fira_preflash/pelvis_uicr_backup.hex`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/firmware/2026-10-04_two_node_fira_preflash/pelvis_uicr_backup.hex)
* [`hardware/outputs/records/firmware/2026-10-04_two_node_fira_preflash/head_uicr_backup.hex`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/firmware/2026-10-04_two_node_fira_preflash/head_uicr_backup.hex)
* [`hardware/outputs/records/firmware/2026-10-04_two_node_fira_preflash/pelvis_full_flash_backup.hex`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/firmware/2026-10-04_two_node_fira_preflash/pelvis_full_flash_backup.hex) (512 KiB full flash)
* [`hardware/outputs/records/firmware/2026-10-04_two_node_fira_preflash/head_full_flash_backup.hex`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/firmware/2026-10-04_two_node_fira_preflash/head_full_flash_backup.hex) (512 KiB full flash)

---

## 3. Established Facts & Baseline Verification (Ground Truth)

1. **Hardware is 100% Proven Functional**:
   - Both physical boards were tested using stock Qorvo CLI FreeRTOS firmware ([`DWM3001CDK-CLI-FreeRTOS.hex`](file:///C:/COMP971_QORVO_SDK_1_1_1/BuildOutput/CLI/FreeRTOS/DWM3001CDK/Release/DWM3001CDK-CLI-FreeRTOS.hex)).
   - Automated two-way ranging was executed at 10 Hz across both boards in both directions via [`hardware/tools/diagnostics/run_stock_fira_test.ps1`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/tools/diagnostics/run_stock_fira_test.ps1):
     - **Test A (HEAD = Controller `INITF`, PELVIS = Responder `RESPF`)**: 50/50 consecutive blocks with status `"SUCCESS"` (~27 cm to 36 cm).
     - **Test B (Role Swap: PELVIS = Controller `INITF`, HEAD = Responder `RESPF`)**: 50/50 consecutive blocks with status `"SUCCESS"` (~28 cm to 37 cm).
   - Antennas, RF transceivers, crystal trims, and SPI communications are fully functional. Neither board has any hardware defect.
2. **Calibration Integrity Verified**:
   - Post-test readbacks of UICR and calibration sectors (`0x0007E000`–`0x0007FFFF`) were verified byte-for-byte identical to the backups (512/512 records match). Stock firmware does not touch calibration sectors.
3. **Problem Scope**:
   - The failure of our custom build ([`DWM3001CDK-COMP971-FreeRTOS.elf`](file:///C:/COMP971_QORVO_SDK_1_1_1/BuildOutput/COMP971/FreeRTOS/DWM3001CDK/Debug_HEAD_v5/DWM3001CDK-COMP971-FreeRTOS.elf)) to range is **strictly a software configuration/environment issue** between our build and stock Qorvo firmware.

---

## 4. Key Technical Gotchas & Critical Source Discoveries

1. **`persistent_time_init(0)` is Mandatory**:
   - In [`Src/Apps/Src/fira/fira_app.c`](file:///C:/COMP971_QORVO_SDK_1_1_1/Src/Apps/Src/fira/fira_app.c), `persistent_time_init(0)` initializes hardware **RTC2** (`BASE_TIMER_RTC_INSTANCE`).
   - Nordic SoftDevice uses RTC0; FreeRTOS uses RTC1; Qorvo QHAL uses RTC2.
   - If `persistent_time_init(0)` is omitted, `nrfx_rtc_counter_get()` in `persistent_time.c:70` returns 0, causing the CPU to hang indefinitely in `while (ticks == ticks_prev)`. It must always be called at startup.
2. **Error Code Definitions (SDK `fbs.h` and `fira_app.c`)**:
   - `0x00` (0): `QUWBS_FBS_STATUS_RANGING_SUCCESS` ("Ok")
   - `0x20` (32): `QUWBS_FBS_STATUS_ERROR_NO_RESP` ("No Response")
   - `0x21` (33): `QUWBS_FBS_STATUS_ERROR_RX_TIMEOUT` ("Rx Timeout")
   - `0x22` (34): `QUWBS_FBS_STATUS_RANGING_TX_FAILED` ("Tx Failed")
   - *Never refer to status 32 as "TX_FAILED". It is `NO_RESP`.*
3. **`dw3000_tx_count` Instrumentation**:
   - Located in [`Libs/dwt_uwb_driver/deca_interface.c`](file:///C:/COMP971_QORVO_SDK_1_1_1/Libs/dwt_uwb_driver/deca_interface.c).
   - Increments on entry to `dwt_starttx()`. When `dw3000_tx_count == 0` while the controller reports `NO_RESP`, it proves the transmission was aborted before the radio driver was called.
4. **Stock CLI UART vs USB CDC**:
   - The stock CLI project defines `-DUSB_ENABLE` by default, which sets `COMM_UART_ALLOWED_DEFAULT = false` and directs CLI output to the unpopulated native USB port (J20).
   - In [`Projects/FreeRTOS/CLI/DWM3001CDK/project_CLI.cmake`](file:///C:/COMP971_QORVO_SDK_1_1_1/Projects/FreeRTOS/CLI/DWM3001CDK/project_CLI.cmake), `-DUSB_ENABLE` was removed so the CLI communicates directly over J-Link VCOM UART (`COM6` and `COM13`) at 115200 baud.
5. **Prime Culprits for Custom Build Failure**:
   - `CONFIG_L1_CONFIG_VOLATILE` and our RAM flash guard ([`comp971_qflash_ram_guard.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_qflash_ram_guard.c)): Stock Qorvo reads and writes L1 config to flash. If L1 config in RAM is unpopulated, radio TX setup fails.
   - SoftDevice S113 priority 0/1 interrupt preemption.
   - Clock configuration differences (`nrf_drv_clock_init` vs SoftDevice clock ownership).
   - FreeRTOS priority assignments and stack allocations.

---

## 5. Repository & Workspace Map

### Custom Firmware Source Files
* **Adapter implementation**: [`Common/src/comp971_uwb_official_fira.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c)
* **Application entry point**: [`Common/src/main.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c)
* **BLE pilot implementation**: [`Common/src/comp971_ble_pilot.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_ble_pilot.c)
* **CMake project definition**: [`DWM3001CDK/project_COMP971.cmake`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/DWM3001CDK/project_COMP971.cmake)
* **Flash RAM Guard**: [`Common/src/comp971_qflash_ram_guard.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_qflash_ram_guard.c)

### Stock SDK Reference Files
* **Qorvo SDK Root**: `C:\COMP971_QORVO_SDK_1_1_1`
* **Stock `fira_app.c`**: [`Src/Apps/Src/fira/fira_app.c`](file:///C:/COMP971_QORVO_SDK_1_1_1/Src/Apps/Src/fira/fira_app.c)
* **Stock CLI Hex**: [`BuildOutput/CLI/FreeRTOS/DWM3001CDK/Release/DWM3001CDK-CLI-FreeRTOS.hex`](file:///C:/COMP971_QORVO_SDK_1_1_1/BuildOutput/CLI/FreeRTOS/DWM3001CDK/Release/DWM3001CDK-CLI-FreeRTOS.hex)

### Active CMake Build Directories
* **HEAD (Node 0)**: `C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_HEAD_v5`
* **PELVIS (Node 1)**: `C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_PELVIS_v5`

### Diagnostic Scripts & Tools
* **Gateway Capture**: [`hardware/tools/diagnostics/capture_gateway_raw.ps1`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/tools/diagnostics/capture_gateway_raw.ps1) (records binary stream on COM16)
* **Gateway Decoder**: [`hardware/tools/decode_gateway_capture.py`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/tools/decode_gateway_capture.py) (decodes IMU, UWB, and diagnostics)
* **Stock Test Runner**: [`hardware/tools/diagnostics/run_stock_fira_test.ps1`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/tools/diagnostics/run_stock_fira_test.ps1) (automated stock CLI runner)
* **Stock Probing Script**: [`hardware/tools/diagnostics/test_stock_cli.ps1`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/tools/diagnostics/test_stock_cli.ps1)

---

## 6. Step-by-Step Bisection Plan (Next Immediate Tasks)

Execute the bisection plan in this exact sequence:

```text
+-------------------------------------------------------------------+
| Experiment 3a: COMP971 Build, Clean UWB-Only                      |
| (Suppress SoftDevice, IMU task, and status task. Only uwb_task)   |
+-------------------------------------------------------------------+
                                  |
                                  v
                      +-----------------------+
                      | Does UWB FiRa Range?  |
                      +-----------------------+
                       /                     \
                   [YES]                     [NO]
                     /                         \
+-------------------------------+  +--------------------------------+
| SoftDevice / Interrupt        |  | Experiment 3b: Restore Native  |
| Latency or IMU is the Culprit |  | QFlash (Remove volatile L1)    |
+-------------------------------+  +--------------------------------+
                                                   |
                                                   v
                                       +-----------------------+
                                       | Does UWB FiRa Range?  |
                                       +-----------------------+
                                        /                     \
                                    [YES]                     [NO]
                                      /                         \
                       +----------------------+  +---------------------+
                       | Root Cause: L1 RAM   |  | Inspect Clock,      |
                       | Config Unpopulated.  |  | FreeRTOS config,    |
                       | Pre-populate in RAM! |  | and compiler flags  |
                       +----------------------+  +---------------------+
```

### Task 1: Execute Experiment 3a (Clean UWB-Only Build)
1. In [`Common/src/main.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c):
   - Comment out `comp971_ble_pilot_init(...)`.
   - Comment out `xTaskCreate(status_task, ...)` and `xTaskCreate(imu_task, ...)`.
   - Keep only `uwb_task` running.
2. Build both targets:
   ```powershell
   cmake --build "C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_HEAD_v5"
   cmake --build "C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_PELVIS_v5"
   ```
3. Flash both boards using sector erase:
   ```powershell
   nrfjprog --snr 760225805 --program "C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_HEAD_v5\DWM3001CDK-COMP971-FreeRTOS.hex" --sectorerase --verify --reset
   nrfjprog --snr 760224787 --program "C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_PELVIS_v5\DWM3001CDK-COMP971-FreeRTOS.hex" --sectorerase --verify --reset
   ```
4. Observe Green LED (`D11` / `P0.22`) and inspect RAM variables via J-Link SWD:
   - Read `m_cb_call_count`, `m_latest_report`, `dw3000_tx_count`.
   - If Green LED flashes at 10 Hz: ranging is working in isolation! SoftDevice/IMU is the culprit.
   - If `dw3000_tx_count` remains 0: SoftDevice is exonerated; proceed to Task 2.

### Task 2: Execute Experiment 3b (Native QFlash & L1 Config)
*(Only if Experiment 3a fails)*
1. In [`DWM3001CDK/project_COMP971.cmake`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/DWM3001CDK/project_COMP971.cmake), remove `-DCONFIG_L1_CONFIG_VOLATILE`.
2. Remove `comp971_qflash_ram_guard.c` from compilation, restoring Qorvo's native `qflash.c`.
3. Rebuild, flash, and test.
4. If this ranges, the root cause is confirmed as unpopulated L1 configuration in RAM. Pre-populate the required L1 structures directly in RAM to maintain calibration safety without needing flash writes.

### Task 3: Step-by-Step Reintroduction
Once a working baseline is established:
1. Re-enable SoftDevice initialization (without advertising).
2. Re-enable BLE advertising and connection to Gateway Dongle (`COM16`).
3. Re-enable 60 Hz ICM-20948 IMU task.
4. Confirm dual 60 Hz IMU telemetry + real-time UWB ranges stream to `COM16` with zero queue drops.
