# Current System Status — 19 October 2026

**Project:** AUT COMP971 Dissertation — Multimodal Inertial-UWB Fusion for Real-Time Human Motion Forecasting  
**Authoritative Hardware Root:** `hardware/`  
**Current Baseline:** Official Qorvo QM33 SDK 1.1.1 + FreeRTOS + Nordic SoftDevice S113/S140 + Zephyr Gateway  
**System Status:** **6-NODE STAR TELEMETRY OPERATIONAL; CALIBRATED BENCH & ON-BODY 2-NODE BASELINE ACCEPTED; 30-MINUTE 6-NODE BENCHMARK SOAK CAPTURED; STEP 28B CAUSAL 60 HZ DATASET VALIDATED**

---

## 1. Executive Summary & Current Operational State

The wearable motion capture hardware has transitioned through three major development phases:
1. **Phase 1 (September 2026):** Legacy custom Zephyr DW3000 driver and sequential 15-pair mesh evaluation (`archive/dwm3001cdk_legacy_proto/`). Proved basic connectivity but suffered from sequential blocking timeouts, clock drift, lack of factory antenna delay calibration, and high packet dropouts.
2. **Phase 2 (1–7 October 2026):** Migration to the Official Qorvo QM33 SDK 1.1.1 (`qorvo_qm33_motion_node/`) with shadow RAM persistence guard (`comp971_qflash_ram_guard.c`). Established 100% steady-state 2-node FiRa DS-TWR ranging, empirical distance calibration (30–150 cm), antenna orientation tolerance (0°–90°), on-body walking validation (>99.3% delivery, 2.63 cm inlier SD), and autonomous gateway reconnect resilience (>15 s unplug survived with 0 node lockups).
3. **Phase 3 (8–19 October 2026):** Deployment of the 6-Node Star Architecture (Pelvis Coordinator + 5 Limb Controlees: Head, Left Wrist, Right Wrist, Left Ankle, Right Ankle). Executed the 30-minute 6-node soak benchmark (`gateway_raw_2026-10-12_101320.bin`, 640k IMU records, 8,871 UWB blocks), validated the Step 28B causal 60 Hz dataset (107,999 frames with 0.0 ppm phase drift), completed the comprehensive Gemini 4 Argon firmware/scheduling audit, and executed the 3-node stationary live demo (19 October 2026) with zero sequence gaps.

---

## 2. Registered Hardware Identities & Port Mapping

Permanent board identity is bound to the onboard SEGGER J-Link serial number in `config/device_registry.json`. Runtime roles are dynamically assigned at boot via `NRF_FICR->DEVICEID` in `comp971_node_identity.c`:

| Node ID | Permanent Name | Board Model | J-Link Serial | Last-Seen COM | UWB FiRa Role | Short Addr | Assigned Time Slot |
| :---: | :--- | :--- | :---: | :---: | :--- | :---: | :---: |
| **0** | **DWM-01-HEAD** | Qorvo DWM3001CDK | `760225805` | COM13 | Controlee / Responder | `0x0002` | Slot 1 (2.0–4.0 ms) |
| **1** | **DWM-02-PELVIS** | Qorvo DWM3001CDK | `760224787` | COM6 | Controller / Coordinator | `0x0001` | Slot 0 Poll, 6 Final, 7 Report |
| **2** | **DWM-03-LEFT-WRIST** | Qorvo DWM3001CDK | `760225868` | COM12 | Controlee / Responder | `0x0003` | Slot 2 (4.0–6.0 ms) |
| **3** | **DWM-04-RIGHT-WRIST** | Qorvo DWM3001CDK | `760225748` | COM11 | Controlee / Responder | `0x0004` | Slot 3 (6.0–8.0 ms) |
| **4** | **DWM-05-LEFT-ANKLE** | Qorvo DWM3001CDK | `760225018` | COM9 | Controlee / Responder | `0x0005` | Slot 4 (8.0–10.0 ms) |
| **5** | **DWM-06-RIGHT-ANKLE** | Qorvo DWM3001CDK | `760225934` | COM10 | Controlee / Responder | `0x0006` | Slot 5 (10.0–12.0 ms) |
| **GW** | **GATEWAY-DONGLE** | nRF52840 Dongle | N/A | **COM16** | BLE Central / USB Bridge | N/A | N/A |

---

## 3. Verified Empirical Results Across All Phases

### 3.1 Two-Node Baseline & Empirical Calibration (Phases 1, B′, C, D′)
- **High-Precision Baseline (Phase 1):** Head (Initiator) and Pelvis (Responder) achieved **100.0% steady-state ranging** (301/301 consecutive blocks) at 5 Hz with zero radio late-transmits (`min_lead_dtu = +71,794 DTU = +4.60 ms`).
- **Fixtured Bench Repeatability (Phase B′ @ 100 cm):** Elevated 2 cm above wooden table. Standard deviation dropped to **2.28–2.77 cm** (robust spread 1.48–2.97 cm) across 2,990 samples with **0 outliers (>10 cm)** and negligible within-block drift (<0.4 cm).
- **Two-Round Symmetric Distance Sweep (Phase C, 30 to 150 cm):**
  - Residual linearity standard deviation: **1.17 cm**.
  - Regression Model: `Measured_Distance = 1.0486 × True_Distance − 2.16 cm`.
  - **Host Software Inversion Formula:**
    ```text
    True_Distance_Estimated = (Raw_UWB_Range_cm + 2.16 cm) / 1.0486
    ```
  - Combined 1-sigma uncertainty budget: **~3.0 cm** (accounting for noise, multipath ripple, and repositioning).
- **Antenna Orientation Sensitivity (Phase D′ @ 50 cm):** Tested at 0°, 45°, and 90° yaw. Median range shifted by <= 1.0 cm with 99.66% to 100.00% packet delivery.

### 3.2 Wearable On-Body Validation (Phase E)
- **Static Standing (Torso path 46 cm tape distance):** **99.48% delivery** (574/577), inlier SD = **2.46 cm**, 0 outliers.
- **Dynamic Walking & Arm Swinging:** **99.33% delivery** (591/595), core inlier SD = **2.63 cm** (99.15% of samples). Only 5 single samples (0.85%) spiked during arm cross-overs, cleanly suppressed by a 10 cm outlier rejection filter.
- **Physical Gateway Unplug & Reconnection:** nRF52840 gateway unplugged for >15 seconds during active battery operation. Reconnection re-established dual 60.1 Hz IMU streaming (1,206 and 1,200 samples, 0 gaps) and 5 Hz UWB ranging instantly with **0 node crashes or lockups**.

### 3.3 30-Minute 6-Node Star Soak Benchmark (`gateway_raw_2026-10-12_101320.bin`)
- **Total IMU Records Received:** **640,924 samples** across all 6 nodes (average 106,820 samples per node, exactly **60.1 Hz**, <0.3% packet gap rate).
- **Total UWB Ranging Blocks:** **8,871 blocks** (nominal 9,000 blocks at 5 Hz).
- **Per-Node UWB Delivery:**
  - **Head (Slot 1):** **99.53% valid** (8,829 / 8,871 blocks, 0 stalls >= 5 s, max streak 1.2 s).
  - **Right Ankle (Slot 5):** **96.00% valid** (8,516 / 8,871 blocks).
  - **Left Wrist (Slot 2):** **72.27% valid** (6,411 / 8,871 blocks).
  - **Left Ankle (Slot 4):** **68.80% valid** (6,103 / 8,871 blocks).
  - **Right Wrist (Slot 3):** **62.34% valid** (5,530 / 8,871 blocks).
- **Streak Decomposition Insight:** Baseline single-packet RF quality is virtually identical across all nodes (**99.29% to 99.53%**; only 42 to 63 lost blocks outside of multi-second stalls). **98% of missing measurements on degraded links were caused by multi-second synchronization lock-loss stalls and firmware recovery bugs.**

### 3.4 Step 28B Offline Causal Dataset Alignment
- **Input:** `gateway_raw_2026-10-12_101320.bin` (30.2 MB).
- **Output:** `step28b_causal_60hz_aligned_mode_b.jsonl` (313 MB, 107,999 frames).
- **Grid Arithmetic:** Exact integer indexing eliminated cumulative microsecond rounding drift (**0.0 ppm drift** over 1,800 s).
- **Reboot Boundary Handling:** Explicitly invalidates all 5 UWB cache registers across Pelvis coordinator software reboots (`NVIC_SystemReset()`), preventing stale data propagation.

### 3.5 Live Demo Stationary Diagnostic (19 October 2026)
- **Capture:** `outputs/records/live_demo/three_node_stationary_raw_20261019_104721.bin`.
- **Duration:** 12.07 seconds.
- **Active Nodes:** Head (`Node 0`), Pelvis (`Node 1`), Left Wrist (`Node 2`).
- **Telemetry Integrity:** Exactly 726 samples per node received at **60.16 Hz** with **0 sequence gaps and 0.0% packet loss**.
- **Noise Metrics:** Accelerometer standard deviation noise floor <= 0.07 m/s²; gyroscope bias norm <= 1.15 dps.

---

## 4. Current Firmware Architecture & Verified Root Causes

### 4.1 Production Codebase Layout
- **Wearable Motion Node:** `hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/`
  - `Common/src/main.c`: FreeRTOS tasks (`uwb_task`, `imu_task`, `status_task`), hardware watchdog.
  - `Common/src/comp971_uwb_official_fira.c`: DW3110 driver wrappers, FiRa session 42 manager, Controlee recovery state machine, stage-2 MAC restart.
  - `Common/src/comp971_node_identity.c`: Dynamic `NRF_FICR->DEVICEID` role resolution.
  - `Common/src/comp971_icm20948.c`: TWIM1 I2C driver for 60 Hz 6-axis IMU sampling.
  - `Common/src/comp971_ble_pilot.c`: SoftDevice BLE peripheral, custom 128-bit UUID GATT telemetry service.
  - `Common/src/comp971_qflash_ram_guard.c`: Shadow RAM persistence guard protecting flash calibration sectors.
- **Central Gateway:** `hardware/firmware/nrf52840_dongle_motion_gateway/`
  - `src/main.c`: Zephyr BLE Central observer (100% scan duty cycle), USB CDC ACM binary protocol v1 stream.

### 4.2 Verified Firmware Issues from the 16 October Audit
1. **v6.7 Controlee Recovery State Machine Counter Bug (`comp971_uwb_official_fira.c:566–589`):**
   `g_fira_valid_sequence`, `g_fira_cb_valid_streak`, and `g_fira_last_valid_cookie` were updated only inside the `role == CONTROLLER` branch. On Controlees running v6.7, these counters remain 0 forever. In `COMP971_REC_COOLDOWN`, even when valid ranging is restored, the state machine interprets the condition as a failure, executes an unnecessary second MAC restart (tearing down the working session), and locks the node into a **30-second `COMP971_REC_HOLD` lockout (`0xED`)**.
2. **Firmware Version Skew Across Physical Nodes:**
   Nodes 0, 3, 4, and 5 still run the 9 October build (`Debug_UNIFIED_star`), while Node 2 runs the 10 October v6.7 build and Node 1 runs the 11 October Step 17 build. Nodes 3 and 4 execute unthrottled 5-second restart loops (88 restarts on Right Wrist, 60 on Left Ankle).
3. **Delayed-TX Deadline Misses (`HPDWARN` / `status = 32` `TX_FAILED`):**
   `imu_task` runs at FreeRTOS Priority 2 (equal to `uwb_task`) polling 100 kHz I2C (~2.0 ms bus time), while SoftDevice BLE interrupts execute at NVIC Priorities 0/1. On Slots 2–5, `CMD_DTX` misses `DX_TIME`, asserting `HPDWARN` and causing multi-second synchronization loss.
4. **Pelvis Controller Hard Reset Cascade:**
   On an `HPDWARN` freeze, Pelvis skips soft recovery and reboots via `NVIC_SystemReset()`. The resulting 10-second blackout causes all 5 Controlees to expire their 5-second watchdog timers simultaneously, triggering a network-wide restart cascade.

---

## 5. Next Technical Actions & Workspace Cleanliness

1. **Firmware Remediation (Pre-Deployment):**
   - Apply the 5-line fix in `comp971_uwb_official_fira.c:574–589` to update valid sequence counters on Controlees.
   - Elevate `uwb_task` priority above `imu_task` (`Priority 3` vs `Priority 2`) and upgrade I2C frequency from 100 kHz to 400 kHz in `comp971_icm20948.c`.
   - Widen `slot_duration_rstu` from 2400 (2.0 ms) to 3600 (3.0 ms) to provide +1.0 ms extra turnaround lead time.
   - Replace the hard `NVIC_SystemReset()` on Pelvis with an in-place soft MAC restart.
   - Flash **all 6 physical nodes** with the single unified, verified firmware binary using `--sectorerase`.
2. **Workspace Maintenance:**
   - Obsolete build folders and temporary compilation artifacts have been purged.
   - Loose diagnostic and flash verification hex files have been organized under `outputs/records/`.
   - Golden factory backups in `backups/` remain strictly protected and verified.
