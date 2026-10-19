# Geni 4 Argon — UWB Hardware Scheduling, Concurrency, and Firmware Audit Report

- **Document ID**: `GENI-4-ARGON-UWB-AUDIT-2026-10-16`
- **Auditor / Analysis Engine**: Geni 4 Argon
- **Date**: 16 October 2026
- **Scope**: Read-only end-to-end engineering inspection of `hardware/` documentation, active FreeRTOS Qorvo QM33 FiRa firmware (`qorvo_qm33_motion_node`), legacy Zephyr 15-pair mesh firmware (`dwm3001cdk_motion_node`), BLE Central gateway firmware (`nrf52840_dongle_motion_gateway`), and all empirical 6-node hardware capture datasets and validation reports (`hardware/outputs/`).
- **Workspace Modification Status**: Strictly read-only audit (zero source code or hardware configuration modifications prior to this report).

---

## 1. Executive Summary

This report presents a comprehensive, evidence-backed engineering audit of the Ultra-Wideband (UWB) scheduling architecture, FreeRTOS task/interrupt concurrency model, transceiver driver instrumentation, and recovery state machines across the COMP971 6-node body motion capture hardware platform (DWM3001CDK / Qorvo QM33120W = nRF52833 + DW3110 + ICM-20948).

Every finding and recommendation in this document is grounded directly in:
1. The firmware source code in [`hardware/firmware/`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware),
2. The architectural and takeover documentation in [`hardware/docs/`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/docs), and
3. The decoded telemetry and diagnostic logs from the 30-minute 6-node hardware runs in [`hardware/outputs/records/captures/`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/captures) and [`hardware/outputs/records/`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records).

### Summary of Verified Issues

We identified **seven concrete scheduling, concurrency, and recovery state-machine defects** in the active Qorvo FiRa Star firmware ([`hardware/firmware/qorvo_qm33_motion_node/`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node)) and **one architectural scheduling trade-off** between the active 5-link FiRa star topology and the legacy 15-pair custom TDMA mesh ([`hardware/firmware/archive/dwm3001cdk_legacy_proto/`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/archive/dwm3001cdk_legacy_proto)):

| Issue ID | Severity | Subsystem / File | Summary of Verified Defect & Empirical Impact |
| :--- | :--- | :--- | :--- |
| **Issue 1** | **CRITICAL** | DW3110 Delayed-TX & Qorvo FiRa MAC (`deca_interface.c`, `uwbstack_core`, [`comp971_uwb_official_fira.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c)) | **Unhandled DW3110 Delayed-TX `HPDWARN` (`0x08000000`) Deadlocks the FiRa Controller Round State Machine.** In `7 out of 7` captured Pelvis (`0x0001`) stalls, the MAC issued exactly one delayed TX (`DWT_START_TX_DELAYED`) after the last callback; SPI/ISR latency pushed `CMD_DTX` into the DW3110 preamble/PLL guard window, raising `HPDWARN` (`0xE9 = 8`) and aborting TX (`result = -1`). `uwbstack_core` waits forever for a `TXFRS` IRQ that never fires. |
| **Issue 2** | **HIGH** | Pelvis Watchdog Supervisor ([`main.c:259-345`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L259-L345)) | **Destructive `Stage 3` (`NVIC_SystemReset()`) Recovery & 3-Strike Permanent Lockout Hazard.** `supervisor_task` skips `comp971_uwb_stage1_restart()` and `comp971_uwb_stage2_mac_restart()` and immediately reboots the Pelvis MCU, turning a single missed delayed-TX into a **9.0–10.2 s system-wide UWB blackout** and a **6.3–6.8 s (381-sample) Pelvis IMU dropout**. Furthermore, 4 stalls within 60 s permanently disables UWB (`uwb_recovery_disabled = true`). |
| **Issue 3** | **CRITICAL BUG** | v6.7 Controlee Recovery State Machine ([`comp971_uwb_official_fira.c:478-589`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L478-L589) vs [`982-1028`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L982-L1028)) | **Controlee Valid-Callback Counters Are Gated Inside `if (g_comp971_uwb_role == COMP971_UWB_CONTROLLER)`.** On any Controlee running v6.7, `g_fira_valid_sequence`, `g_fira_cb_valid_streak`, and `g_fira_last_valid_cookie` remain `0` forever. Once a v6.7 Controlee triggers `Attempt 1`, it can **never** exit `COMP971_REC_COOLDOWN`, forcing an unnecessary second MAC teardown (`Attempt 2`) and mandatory escalation to `COMP971_REC_HOLD` (30 s hold, `0xED`). Proven `8/8` times on Node 2 (Left Wrist) in `2026-10-12_101320`. |
| **Issue 4** | **HIGH** | Controlee Watchdog & Fleet Consistency ([`comp971_uwb_official_fira.c:939-977`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L939-L977)) | **Controlee Callback Silence on Missed Polls & Mixed Firmware Versions Across Nodes.** Missed Poll frames suppress `fira_notification_cb` on Controlees; after `5.0–6.25 s`, the Controlee tears down its active MAC/session (`stage2_mac_restart`), causing a mandatory 27–31 block (`5.4–6.2 s`) cold-start re-acquisition outage. In `2026-10-12_101320`, Node 2 ran v6.7 while Nodes 3 and 4 still ran pre-v6.7 firmware. |
| **Issue 5** | **MEDIUM** | FreeRTOS Task Scheduling ([`main.c:423-441, 488-534, 635-652`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L635-L652)) | **Priority Inversion Against Qorvo `DefaultTask` (Priority 1) & 100 ms Polling Latency in `uwb_task`.** Application tasks (`imu_task`, `uwb_task`, `status_task`) run at Priority 2 with `configUSE_TIME_SLICING = 0`, preempting Qorvo's `DefaultTask` (Priority 1) during 20 ms IMU I2C reads and BLE queueing. Additionally, `uwb_task` polls every 100 ms (`vTaskDelay(100)`) with a single-slot report buffer (`m_latest_star_report`). |
| **Issue 6** | **MEDIUM** | UWB Timestamp Generation ([`comp971_uwb_official_fira.c:238-242`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L238-L242) vs [`main.c:510-512`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L510-L512)) | **UWB `local_timestamp_us()` RTC1 Prescaler Scaling Error (`-7,030.3 ppm` Clock Drift).** FreeRTOS RTC1 ticks at `32768 / 33 = 992.9697 Hz`. `imu_task` uses exact `(tick * 33000000ULL) / 32768ULL` scaling, but `local_timestamp_us()` in `comp971_uwb_official_fira.c` divides by `configTICK_RATE_HZ` (`1000`), causing UWB `pelvis_timestamp_us` to lag IMU timestamps by **12.65 seconds over a 30-minute capture**. |
| **Issue 7** | **LOW** | Diagnostic Protocol Encoding ([`comp971_uwb_official_fira.c:95-106`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L95-L106) vs [`comp971_telemetry_protocol.h:76-88`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/include/comp971_telemetry_protocol.h#L76-L88)) | **Diagnostic Stage Code Collision (`0xE1..0xEA`).** `COMP971_REC_DIAG_*` reuses the exact hex values of `COMP971_DIAG_STALL_*` (`0xE0..0xEB`), causing `decode_gateway_capture.py` to misclassify Controlee recovery events (`0xE2`, `0xE3`) as Pelvis stall snapshot counters and `0xED` (`HOLD_ENTER`) as `"unknown"`. |
| **Issue 8** | **ARCHITECTURAL** | Topology & TDMA Schedule ([`comp971_telemetry_protocol.h:27-32`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/include/comp971_telemetry_protocol.h#L27-L32), [`archive/dwm3001cdk_legacy_proto/src/main.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/archive/dwm3001cdk_legacy_proto/src/main.c)) | **5-Link Star vs. 15-Pair Full Mesh Trade-Off.** The active FiRa firmware measures only the 5 Pelvis-centered star links (`0x0001 <-> 0x0002..0x0006`) at 5 Hz and omits the 10 cross-body links. The legacy Zephyr 15-pair sequential mesh (`dwm3001cdk_legacy_proto`) achieved only 1.21 sweeps/s with a 33.0% attempt failure rate due to sequential blocking timeouts (`16–40 ms`), 2 ms delayed-TX `HPDWARN` exhaustion, and missing Qorvo L1 OTP calibration. |

---

## 2. Complete Scope of Inspection

### 2.1 Documentation Inspected
1. [`hardware/README.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/README.md) & [`hardware/docs/09_HANDOVER_AND_DEBUGGING_MANUAL.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/docs/09_HANDOVER_AND_DEBUGGING_MANUAL.md)
2. [`hardware/docs/README.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/docs/README.md) & [`hardware/docs/01_CURRENT_SYSTEM_STATUS.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/docs/01_CURRENT_SYSTEM_STATUS.md)
3. [`hardware/docs/04_MULTI_NODE_SCHEDULING_INVESTIGATION.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/docs/04_MULTI_NODE_SCHEDULING_INVESTIGATION.md) (v6.4 Deep-Dive Investigation)
4. [`hardware/docs/reference/historical_2026-10-05_uwb_fira_takeover_guide.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/docs/reference/historical_2026-10-05_uwb_fira_takeover_guide.md)
5. [`hardware/docs/03_TWO_NODE_BASELINE_AND_CALIBRATION_REPORT.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/docs/03_TWO_NODE_BASELINE_AND_CALIBRATION_REPORT.md)
6. [`hardware/docs/00_HARDWARE_SYSTEM_GUIDE.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/docs/00_HARDWARE_SYSTEM_GUIDE.md) & [`hardware/docs/08_QORVO_SDK_SOURCE_REVIEW.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/docs/08_QORVO_SDK_SOURCE_REVIEW.md)
7. [`hardware/docs/07_OFFICIAL_SDK_INTEGRATION_STRATEGY.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/docs/07_OFFICIAL_SDK_INTEGRATION_STRATEGY.md) & [`hardware/docs/reference/historical_2026-10-03_official_uwb_remediation_plan.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/docs/reference/historical_2026-10-03_official_uwb_remediation_plan.md)
8. [`hardware/docs/reference/01-live-hardware-readiness.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/docs/reference/01-live-hardware-readiness.md) & [`hardware/docs/reference/historical_2026-09-29_transport_stability_update.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/docs/reference/historical_2026-09-29_transport_stability_update.md)
9. [`hardware/firmware/qorvo_qm33_motion_node/docs/UWB_15_PAIR_MESH_DESIGN.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/docs/UWB_15_PAIR_MESH_DESIGN.md) & [`hardware/firmware/qorvo_qm33_motion_node/README.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/README.md)

### 2.2 Firmware Source Code Inspected
1. **Active Qorvo QM33 SDK FreeRTOS FiRa Star Firmware** ([`hardware/firmware/qorvo_qm33_motion_node/`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node)):
   - [`official_sdk_project/Common/src/main.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c)
   - [`official_sdk_project/Common/src/comp971_uwb_official_fira.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c)
   - [`official_sdk_project/Common/src/comp971_ble_pilot.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_ble_pilot.c)
   - [`official_sdk_project/Common/src/comp971_node_identity.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_node_identity.c) & [`Common/include/comp971_node_identity.h`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/include/comp971_node_identity.h)
   - [`official_sdk_project/Common/include/comp971_config_shared.h`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/include/comp971_config_shared.h) & [`Common/include/comp971_uwb_two_node.h`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/include/comp971_uwb_two_node.h)
   - [`official_sdk_project/Common/src/comp971_uwb_two_node.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_two_node.c), [`comp971_icm20948.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_icm20948.c), [`comp971_qflash_ram_guard.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_qflash_ram_guard.c), [`hooks.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/hooks.c)
   - [`official_sdk_project/DWM3001CDK/isr_hs.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/DWM3001CDK/isr_hs.c), [`project_COMP971.cmake`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/DWM3001CDK/project_COMP971.cmake), [`COMP971-OfficialFira-FreeRTOS.cmake`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/DWM3001CDK/COMP971-OfficialFira-FreeRTOS.cmake)
   - [`include/comp971_telemetry_protocol.h`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/include/comp971_telemetry_protocol.h)
2. **Legacy Custom Zephyr 15-Pair Mesh Firmware** ([`hardware/firmware/archive/dwm3001cdk_legacy_proto/`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/archive/dwm3001cdk_legacy_proto)):
   - [`src/main.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/archive/dwm3001cdk_legacy_proto/src/main.c) (1,544-line custom Pelvis-commanded 15-pair DS-TWR scheduler)
   - [`lib/dwt_uwb_driver/deca_interface.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/archive/dwm3001cdk_legacy_proto/lib/dwt_uwb_driver/deca_interface.c)
3. **BLE Central USB Gateway Firmware**:
   - [`hardware/firmware/nrf52840_dongle_motion_gateway/src/main.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/nrf52840_dongle_motion_gateway/src/main.c)

### 2.3 Empirical Captures & Validation Reports Inspected
1. [`hardware/outputs/records/diagnostics/2026-10-03_fira_tx_failed_remediation/FAILURE_LOG.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/diagnostics/2026-10-03_fira_tx_failed_remediation/FAILURE_LOG.md)
2. [`hardware/outputs/records/captures/2026-10-07_three_node_star_bringup/six_node_star_test_2026-10-10_181236.json`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/captures/2026-10-07_three_node_star_bringup/six_node_star_test_2026-10-10_181236.json)
3. [`hardware/outputs/records/acquisition/gateway_raw_2026-10-11_153344_decoded/`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/acquisition/gateway_raw_2026-10-11_153344_decoded/summary.json) (`summary.json`, `stall_snapshots.ndjson`, `uwb_diag.ndjson`)
4. [`hardware/outputs/records/acquisition/gateway_raw_2026-10-11_155057_decoded/`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/acquisition/gateway_raw_2026-10-11_155057_decoded/summary.json) (`summary.json`, `stall_snapshots.ndjson`, `uwb_diag.ndjson`)
5. [`hardware/outputs/records/acquisition/gateway_raw_2026-10-11_173658_decoded/`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/acquisition/gateway_raw_2026-10-11_173658_decoded/summary.json) (`summary.json`, `stall_snapshots.ndjson`, `uwb_diag.ndjson`)
6. [`hardware/outputs/records/acquisition/gateway_raw_2026-10-11_200844_decoded/`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/acquisition/gateway_raw_2026-10-11_200844_decoded/summary.json) (`summary.json`, `stall_snapshots.ndjson`, `uwb_diag.ndjson`)
7. [`hardware/outputs/records/acquisition/gateway_raw_2026-10-12_101320_decoded/`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/acquisition/gateway_raw_2026-10-12_101320_decoded/summary.json) (`summary.json`, `stall_snapshots.ndjson`, `uwb_diag.ndjson`)
8. [`STEP_28_DATASET_VALIDATION_REPORT.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/validation_step28/STEP_28_DATASET_VALIDATION_REPORT.md), [`STEP_28A_CORRECTION_REPORT.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/validation_step28a/STEP_28A_CORRECTION_REPORT.md), and [`STEP_28B_FINAL_VALIDATION_REPORT.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/validation_step28b/STEP_28B_FINAL_VALIDATION_REPORT.md)

---

## 3. Empirical Baseline from Device Run Results

### 3.1 Multi-Capture Comparison (11–12 October 2026 Soak Runs)

| Capture Dataset | Duration (s) | Star Reports (`0x06`) | Pelvis `Stage 3` Resets (`0xDF`) | Valid Rate: Head (`0x0005`) | Valid Rate: LW (`0x0002`) | Valid Rate: RW (`0x0003`) | Valid Rate: LA (`0x0004`) | Valid Rate: RA (`0x0006`) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| [`2026-10-11_153344`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/acquisition/gateway_raw_2026-10-11_153344_decoded/summary.json) | 964.2 s | 4,761 | 1 (`blk=3524`) | 4.65 Hz | 4.30 Hz | 3.74 Hz | 3.84 Hz | 4.50 Hz |
| [`2026-10-11_155057`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/acquisition/gateway_raw_2026-10-11_155057_decoded/summary.json) | 1,809.3 s | 9,011 | 0 | 4.69 Hz | 3.82 Hz | 3.83 Hz | 3.92 Hz | 4.54 Hz |
| [`2026-10-11_173658`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/acquisition/gateway_raw_2026-10-11_173658_decoded/summary.json) | 1,806.9 s | 8,988 | 0 | 4.68 Hz | 3.55 Hz | 3.87 Hz | 3.89 Hz | 4.55 Hz |
| [`2026-10-11_200844`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/acquisition/gateway_raw_2026-10-11_200844_decoded/summary.json) | 1,809.6 s | 8,857 | 5 (`blk=3237, 5041, 34, 33, 1819`) | 4.61 Hz | 4.34 Hz | 3.74 Hz | 3.87 Hz | 4.49 Hz |
| [`2026-10-12_101320`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/acquisition/gateway_raw_2026-10-12_101320_decoded/summary.json) (Step 28) | 1,809.5 s | 8,901 | 2 (`blk=3371, 5887`) | 4.65 Hz | 4.30 Hz | 3.77 Hz | 3.74 Hz | 4.53 Hz |

### 3.2 Detailed Per-Link Breakdown of the Canonical Step 28 Capture (`2026-10-12_101320`)

From [`gateway_raw_2026-10-12_101320_decoded/summary.json`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/acquisition/gateway_raw_2026-10-12_101320_decoded/summary.json) and [`STEP_28_DATASET_VALIDATION_REPORT.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/validation_step28/STEP_28_DATASET_VALIDATION_REPORT.md) (`8,901` star reports over `1,809.54 s`, nominal schedule = `5.00 Hz` / `200 ms` per block):

| Target Controlee | UWB Short Addr | Valid Measurements (`status = 0`) | Timeout Measurements (`status = 33`) | Valid Block Share (%) | Effective Valid Rate (Hz) | Longest Consecutive Timeout Streak (Blocks / Seconds) | Streaks >= 25 Blocks (>= 5.0 s) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Head** (Node 5) | `0x0005` | 8,423 | 478 | **94.63%** | **4.65 Hz** | 28 blocks (5.6 s) | 3 |
| **Right Ankle** (Node 6) | `0x0006` | 8,205 | 696 | **92.18%** | **4.53 Hz** | 30 blocks (6.0 s) | 6 |
| **Left Wrist** (Node 2) | `0x0002` | 7,777 | 1,124 | **87.37%** | **4.30 Hz** | 185 blocks (37.0 s) | 12 |
| **Right Wrist** (Node 3) | `0x0003` | 6,818 | 2,083 | **76.60%** | **3.77 Hz** | 216 blocks (43.2 s) | 17 |
| **Left Ankle** (Node 4) | `0x0004` | 6,772 | 2,129 | **76.08%** | **3.74 Hz** | 215 blocks (43.0 s) | 28 |

---

## 4. System Scheduling & Concurrency Architecture

To understand why the failures occur, it is necessary to examine the three interacting scheduling layers inside [`hardware/firmware/qorvo_qm33_motion_node/`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node):

### 4.1 Hardware & Interrupt Priority Hierarchy (nRF52833 + S140 SoftDevice)
1. **Priority 0 (Highest, Unmaskable by Application)**: Nordic S140 SoftDevice radio and `RTC0` / `TIMER0` link-layer interrupts (BLE advertising, connection events, GATT notification transmissions). SoftDevice critical sections can preempt any application ISR or FreeRTOS task for `250 to 600+ us`.
2. **Priority 2 (`NRFX_IRQ_PRIORITY_HIGH`)**:
   - **DW3110 External GPIO Interrupt (`GPIOTE` on pin `P1.02`)**: Signals frame TX complete (`TXFRS`), RX good frame (`RXFCG`), RX timeout (`RXRFTO` / `RXPTO`), etc., waking Qorvo's transceiver/MAC worker thread.
   - **High-Speed Timer (`TIMER2` at 1 MHz / 1 us resolution)** in [`isr_hs.c:54-98`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/DWM3001CDK/isr_hs.c#L54-L98): Drives Qorvo's `qtime` / MCPS slot timer callbacks (`timer_cfg.callback()`).
3. **Priority 7 (Lowest)**: FreeRTOS `RTC1` tick interrupt and `PendSV` context switch.

### 4.2 FreeRTOS Task Priority Hierarchy ([`main.c:635-652`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L635-L652))
FreeRTOS is configured with `configUSE_PREEMPTION = 1`, `configUSE_TIME_SLICING = 0` (cooperative scheduling among equal-priority tasks), and `configTICK_RATE_HZ = 1000` (backed by `RTC1` with prescaler `32`, actual rate `992.9697 Hz`):
- **Priority 3**: [`supervisor_task`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L259-L345) — Wakes every `500 ms` (`vTaskDelay(pdMS_TO_TICKS(500U))`). On the Pelvis Controller (`node_index == 1`), monitors `g_fira_last_cb_tick` and `g_fira_cb_count`.
- **Priority 2**:
  - [`imu_task`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L488-L534) — Wakes every `20 ticks` (`~20.14 ms`, `49.65 Hz`) via `vTaskDelayUntil()`, reads the ICM-20948 over blocking TWI/I2C, and queues 32-byte `COMP971_TELEMETRY_IMU_SAMPLE` packets to BLE.
  - [`uwb_task`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L423-L441) — Initializes UWB after `1,500 ms` (`COMP971_UWB_START_DELAY_MS`), then loops forever calling `comp971_uwb_official_fira_Process()` followed by `vTaskDelay(pdMS_TO_TICKS(100U))`.
  - [`status_task`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L353-L421) — Wakes every `500 ms` to service BLE pilot queues (`comp971_ble_pilot_process()`) and emit node status / diagnostic packets.
- **Priority 1 (`osPriorityLow`)**: Qorvo's `DefaultTask` (created inside Qorvo's UWB stack initialization) — Executes deferred UWB MAC (`uwbmac`) and FiRa session state-machine processing.

### 4.3 Active FiRa Ranging Schedule ([`comp971_config_shared.h:20-35`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/include/comp971_config_shared.h#L20-L35) & [`comp971_uwb_official_fira.c:671-805`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L671-L805))
- **Topology**: One-to-Many Star (`QUWBS_FBS_MULTI_NODE_MODE_ONE_TO_MANY`), Controller = Pelvis (`0x0001`), Controlees = `0x0002` (LW), `0x0003` (RW), `0x0004` (LA), `0x0005` (Head), `0x0006` (RA).
- **Ranging Method**: DS-TWR (`QUWBS_FBS_RANGING_ROUND_USAGE_DS_TWR_NON_DEFERRED`), `schedule_mode = QUWBS_FBS_SCHEDULE_TIME_SCHEDULED`.
- **Timing Parameters**:
  - `session_id = 42`
  - `channel_number = 9` (7.9872 GHz), `preamble_code_index = 10`, `sfd_id = 2` (SP1 frames, BPRF 6.81 Mbps, 64 MHz PRF)
  - `slot_duration_rstu = 2400` (`2,400 RSTU = 2.0 ms` per slot)
  - `round_duration_slots = 25` (`25 * 2.0 ms = 50.0 ms` active ranging round window per block)
  - `block_duration_ms = 200` (`200 ms` per block = `5.00 Hz` repetition rate; `150 ms` idle window between rounds)
  - `round_hopping = false`

---

## 5. Detailed Technical Findings & Proof

---

### Issue 1 (CRITICAL — Root Cause of Pelvis Controller Stalls): Unhandled DW3110 Delayed-TX `HPDWARN` (`0x08000000`) Aborts Poll Transmission and Deadlocks the Qorvo FiRa Round State Machine

#### 1. Code & Driver Instrumentation Context
- [`comp971_uwb_official_fira.c:604-669`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L604-L669) ([`comp971_uwb_Send_Stall_Snapshot`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L604-L669))
- [`UWB_FIRA_ARCHITECTURE_AND_SCHEDULING_INVESTIGATION.md:285-455`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/docs/04_MULTI_NODE_SCHEDULING_INVESTIGATION.md#L285-L455) (Sections 7.2–8.3, `dw3000_deca_starttx` instrumentation in `deca_interface.c`)

When [`supervisor_task`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L280-L307) detects that the Pelvis Controller has not received a [`fira_notification_cb`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L466-L590) for `>= 2,000 ms`, it emits 12 diagnostic records (`0xE0..0xEB`) before resetting the MCU.

#### 2. Complete Empirical Proof (`7 out of 7` Captured Pelvis Stalls)
From [`gateway_raw_2026-10-11_200844_decoded/stall_snapshots.ndjson`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/acquisition/gateway_raw_2026-10-11_200844_decoded/stall_snapshots.ndjson) and [`gateway_raw_2026-10-12_101320_decoded/stall_snapshots.ndjson`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/acquisition/gateway_raw_2026-10-12_101320_decoded/stall_snapshots.ndjson), here is the exact hardware register state for all 7 Pelvis stalls:

| Capture & Stall # | `0xE3` `fira_cb_seq` (Last Block) | `0xE0` `irq_pin` | `0xE4` `irq_cnt` | `0xE5` `cb_last_irq` | `0xE2` `tx_cnt` | `0xE6` `cb_last_tx` | `0xEB` `last_tx_seq` | `0xE7` `last_tx_info` | `0xE9` `last_tx_status` (`SYS_STATUS[31:24]`) | `0xEA` `last_tx_diff` (`dx_time - cur_sys`) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `2026-10-11_200844` #1 | `3237` | `0` | `42052` | `42052` | `9708` | `9707` | `9708` | `0x01010001` | `8` (`0x08`) | `+8383` DTU (`+535.7 us`) |
| `2026-10-11_200844` #2 | `5041` | `0` | `65486` | `65486` | `15117` | `15116` | `15117` | `0x01010001` | `8` (`0x08`) | `+8275` DTU (`+528.8 us`) |
| `2026-10-11_200844` #3 | `34` | `0` | `455` | `455` | `106` | `105` | `106` | `0x01010001` | `8` (`0x08`) | `+10464` DTU (`+668.6 us`) |
| `2026-10-11_200844` #4 | `33` | `0` | `436` | `436` | `102` | `101` | `102` | `0x01010001` | `8` (`0x08`) | `-7073` DTU (`-451.9 us`) |
| `2026-10-11_200844` #5 | `1819` | `0` | `23642` | `23642` | `5458` | `5457` | `5458` | `0x01010001` | `8` (`0x08`) | `-28048` DTU (`-1792.2 us`) |
| `2026-10-12_101320` #1 | `3371` | `0` | `43788` | `43788` | `10109` | `10108` | `10109` | `0x0001FF01` | `8` (`0x08`) | `+7815` DTU (`+499.4 us`) |
| `2026-10-12_101320` #2 | `5887` | `0` | `76460` | `76460` | `17651` | `17650` | `17651` | `0x0001FF01` | `8` (`0x08`) | `+7180` DTU (`+458.8 us`) |

#### 3. Step-by-Step Root Cause Analysis
1. **No Missing GPIO Interrupt Edge (`0xE0 == 0` and `0xE4 == 0xE5`)**:
   In all 7 stalls, the DW3110 IRQ pin (`P1.02`) is low (`0`), and the total DW3110 interrupt count at the moment of the stall (`0xE4`) is **identical** to the interrupt count recorded during the last successful FiRa block callback (`0xE5`). Zero DW3110 hardware interrupts occurred after the last successful block.
2. **Exactly One Delayed-TX Attempt (`0xE2 == 0xE6 + 1` and `0xEB == 0xE2`)**:
   In a normal DS-TWR round on the Controller, the Pelvis transmits 3 frames per block (`Poll`, `Final`, and measurement report / control phase = 3 TX operations per block; notice `3371 * 3 = 10113 ~= 10108`). After the callback for block `N` finished, the FiRa MAC woke up for block `N + 1` and issued **one** `dwt_starttx()` call (`tx_cnt = cb_last_tx + 1`) to schedule the opening Poll frame of block `N + 1` in delayed-TX mode (`mode = 0x01` = `DWT_START_TX_DELAYED`).
3. **DW3110 `HPDWARN` (`0x08000000`) Asserted on `7 out of 7` Stalls**:
   Register `0xE9` captures byte 3 (bits `[31:24]`) of the DW3110 `SYS_STATUS` register immediately after `CMD_DTX`. In all 7 stalls, `0xE9 == 8` (`0x08` in bits `[31:24]`, which is `0x08000000` = `DWT_INT_HPDWARN_BIT_MASK`, Half-Period Delay Warning).
   In `2026-10-12_101320` (v6.4+, after the `CMD_TX` fallback was removed), `0xE7` (`stall_last_tx_info`, packed as `[31:24]fastcmd | [23:16]mode | [15:8]result | [7:0]hpdwarn`) is `0x0001FF01`:
   - `fastcmd = 0x00` (no immediate TX fallback issued)
   - `mode = 0x01` (`DWT_START_TX_DELAYED`)
   - `result = 0xFF` (`(uint8_t)-1` = `DWT_ERROR`)
   - `hpdwarn = 0x01` (`HPDWARN` detected)
4. **Why `HPDWARN` Occurs Even When `last_tx_diff > 0` (`+458 us` to `+668 us`)**:
   In 5 of the 7 stalls (`blk = 3237, 5041, 34, 3371, 5887`), `0xEA` (`last_tx_diff = (int32_t)(dx_time - cur_sys)`) was positive (`+7,180` to `+10,464` DTU, corresponding to `+458.8 us` to `+668.6 us` at `15.65 DTU/us` in the `SYS_TIME` high-32-bit units) when `DWT_READSYSTIME` was read in the pre-check before writing the TX frame.
   Why did `CMD_DTX` still fail with `HPDWARN`?
   - Between the `DWT_READSYSTIME` pre-check and the `CMD_DTX` fast command inside `dwt_starttx()`, the CPU must complete SPI transfers to load the TX buffer (`DWT_WRITETXDATA`), frame control (`DWT_WRITETXFCTRL`), and delayed TX timestamp (`DWT_SETDELAYEDTRXTIME`).
   - Crucially, the DW3110 hardware requires `CMD_DTX` to be latched **at least `tx_prep_guard` (~400 to 500 us) before `dx_time`** so the radio can power up its RF PLL, synthesizer, and preamble generator before the RMARKER timestamp.
   - When `dx_time - cur_sys` is already down to `458–668 us` (because Qorvo's `DefaultTask` at Priority 1 was delayed by Priority 2 tasks `imu_task`/`uwb_task`/`status_task` or preempted by a Priority 0 SoftDevice BLE radio event), the remaining `458–668 us` is smaller than `SPI_setup_time + DW3110_PLL_prep_guard`. By the time `CMD_DTX` reaches the DW3110 over SPI, the target `dx_time` is inside the hardware preparation window, so the DW3110 sets `HPDWARN` (`0x08000000`), refuses to transmit, and never asserts `TXFRS`.
5. **Why `uwbstack_core` Deadlocks**:
   Inside Qorvo's prebuilt `libuwbstack_core.a`, the FiRa Controller state machine transitions into an active TX wait state expecting the DW3110 `TXFRS` (TX Frame Sent) interrupt callback to trigger the subsequent RX window for Controlee responses. When `dwt_starttx()` aborts on `HPDWARN`, no `TXFRS` interrupt ever fires, and the round state machine hangs indefinitely until the 2,000 ms watchdog intervenes.

---

### Issue 2 (HIGH — Destructive Controller Recovery): Pelvis `supervisor_task` Bypasses Soft Session/MAC Restart (`Stage 1` / `Stage 2`) and Immediately Calls `NVIC_SystemReset()` (`Stage 3`)

#### 1. Code Evidence
- [`main.c:259-345`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L259-L345) ([`supervisor_task`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L259-L345))
- [`comp971_uwb_official_fira.c:1100-1155`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L1100-L1155) ([`comp971_uwb_stage1_restart`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L1100-L1125) and [`comp971_uwb_stage2_mac_restart`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L1127-L1155))

In [`comp971_uwb_official_fira.c:1100-1155`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L1100-L1155), two non-destructive recovery functions are already implemented:
- [`comp971_uwb_stage1_restart()`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L1100-L1125): Calls `quwbs_fbs_session_stop -> quwbs_fbs_session_deinit -> quwbs_fbs_session_init -> configure_session_parameters -> quwbs_fbs_set_controlles -> quwbs_fbs_session_start`.
- [`comp971_uwb_stage2_mac_restart()`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L1127-L1155): Calls `quwbs_fbs_session_stop -> quwbs_fbs_session_deinit -> uwbmac_stop -> uwbmac_exit -> l1_config_exit -> deinit_uwb -> init_uwb -> uwbmac_init -> start_session`.

However, [`supervisor_task`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L280-L341) on the Pelvis Controller never calls either `stage1_restart()` or `stage2_mac_restart()`. Instead, as soon as `cb_delta_ms >= 2000U` or `blocks_advanced < 3U` ([`main.c:293`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L293)), it logs `COMP971_DIAG_STAGE3_SYS_RESET` (`0xDF`), sends the stall snapshot, sleeps `100 ms`, and executes `NVIC_SystemReset()` ([`main.c:339`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L339)).

#### 2. Empirical Impact
1. **9.0 to 10.2 Second Total UWB Outage per Stall**:
   In [`gateway_raw_2026-10-12_101320`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/validation_step28/STEP_28_DATASET_VALIDATION_REPORT.md#L76-L108):
   - Stall #1 (`blk = 3371`, `t = 682.9 s`): Last star packet before stall at `682.92 s`; first star packet after reboot (`blk = 0`) at `692.04 s` (**9.12 s total UWB blackout**, ~45 blocks lost across all 5 links).
   - Stall #2 (`blk = 5887`, `t = 1,879.0 s`): Last star packet before stall at `1,879.01 s`; first star packet after reboot (`blk = 0`) at `1,889.18 s` (**10.17 s total UWB blackout**, ~51 blocks lost across all 5 links).
2. **6.3 to 6.8 Second Pelvis IMU Blackout (381 Lost IMU Samples)**:
   Because `NVIC_SystemReset()` reboots the entire Pelvis MCU and drops the BLE peripheral connection to the gateway dongle, Pelvis IMU streaming stops completely for `6.31 s` (Stall #1) and `6.84 s` (Stall #2), losing **381 IMU samples** ([`STEP_28_DATASET_VALIDATION_REPORT.md:100-108`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/validation_step28/STEP_28_DATASET_VALIDATION_REPORT.md#L100-L108)). By contrast, all 5 Controlee nodes achieved `99.99% to 100.00%` IMU sample delivery with zero dropouts > `0.21 s`.
3. **3-Strike Circuit Breaker Hazard (`uwb_recovery_disabled = true`)**:
   In [`main.c:310-322`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L310-L322):
   ```c
   if (now_tick < 60000U) {
     g_retained_diag.reset_loop_count++;
     if (g_retained_diag.reset_loop_count > 3U) {
       uwb_recovery_disabled = true;
       comp971_ble_pilot_send_uwb_Status(COMP971_DIAG_RESET_LOOP_DETECTED,
                                         g_retained_diag.reset_loop_count);
       continue;
     }
   }
   ```
   In [`gateway_raw_2026-10-11_200844`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/acquisition/gateway_raw_2026-10-11_200844_decoded/stall_snapshots.ndjson), Pelvis stalled at `blk = 5041`, then stalled immediately after reboot at `blk = 34` (`now_tick ~= 8,300 < 60,000`), and stalled again after that reboot at `blk = 33` (`now_tick ~= 8,100 < 60,000`). That reached `reset_loop_count = 3` — meaning one more early stall would have permanently disabled UWB on the Pelvis Controller for the remainder of the capture.

---

### Issue 3 (CRITICAL BUG in Current `comp971_uwb_official_fira.c` v6.7): Controlee Valid-Callback Counters Are Placed Inside `if (g_comp971_uwb_role == COMP971_UWB_CONTROLLER)`, Making Controlee `COMP971_REC_COOLDOWN` Impossible to Exit

#### 1. Code Evidence
Inspect [`comp971_uwb_official_fira.c:478-589`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L478-L589) inside [`fira_notification_cb()`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L466-L590):

```c
  if (g_comp971_uwb_role == COMP971_UWB_CONTROLLER) {          // <-- Opens at line 478
    comp971_telemetry_star_ranges_t report;
    ...
    bool cb_has_valid_measurement = false;
    for (int i = 0; i < info->n_measurements && i < COMP971_MAX_STAR_PAIRS; i++) {
      ...
      if (res->status == QUWBS_FBS_STATUS_OK) {
        cb_has_valid_measurement = true;
      }
      ...
    }
    ...
    /* Only count callbacks with >=1 valid measurement (status == 0) AND
     * matching the current active session cookie toward recovery exit. */
    if (cb_has_valid_measurement) {                            // <-- Line 568 (STILL INSIDE Controller block!)
      uint32_t active_cookie = m_rec_session_cookie;
      if (g_fira_last_valid_cookie != active_cookie) {
        g_fira_cb_valid_streak = 1U;
        g_fira_last_valid_cookie = active_cookie;
      } else {
        g_fira_cb_valid_streak++;
      }
      g_fira_valid_sequence++;
      if ((g_fira_cb_valid_streak == 1U) || (g_fira_cb_valid_streak == 2U)) {
        send_diag(COMP971_REC_DIAG_VALID_OBS,
                  (uint8_t)(g_fira_cb_valid_streak & 0xFFU));
      }
    } else {
      g_fira_cb_valid_streak = 0U;
    }
  }                                                            // <-- Closes at line 589!
```

Now compare this with the Bounded Controlee Recovery State Machine in [`comp971_uwb_official_fira_Process()`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L925-L1087):
- Line 925 gates the entire recovery state machine to **Controlees only**:
  ```c
  if ((g_comp971_uwb_role != COMP971_UWB_CONTROLLER) && m_prepared) {
  ```
- When a Controlee enters `COMP971_REC_COOLDOWN` ([`lines 982-1028`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L982-L1028)), it checks whether the session has recovered by reading `g_fira_valid_sequence`, `g_fira_cb_valid_streak`, and `g_fira_last_valid_cookie`:
  ```c
  case COMP971_REC_COOLDOWN: {
    uint32_t cur_valid_seq = g_fira_valid_sequence;
    uint32_t cb_streak = g_fira_cb_valid_streak;
    uint32_t valid_cookie = g_fira_last_valid_cookie;
    ...
    if ((m_rec_consecutive_valid >= COMP971_REC_VALID_EXIT_COUNT) &&
        (cb_streak >= COMP971_REC_VALID_EXIT_COUNT)) {
      /* Genuine post-recovery valid measurements confirmed — clear recovery */
      m_rec_state = COMP971_REC_NORMAL;
  ```

Because `g_fira_valid_sequence`, `g_fira_cb_valid_streak`, and `g_fira_last_valid_cookie` are **only updated inside `if (g_comp971_uwb_role == COMP971_UWB_CONTROLLER)`**, they are **permanently `0` on every Controlee**!

#### 2. Deterministic Failure Cascade Caused by Issue 3
Whenever a Controlee running v6.7 experiences a single 5-second Poll timeout:
1. It transitions `COMP971_REC_NORMAL -> COMP971_REC_RESTARTING` (`Attempt 1`), emits `0xE3` (`RESTART_BEGIN`, `detail = 1`), calls `comp971_uwb_stage2_mac_restart()`, emits `0xE2` (`RESTART_OK`, `detail = 1`), and enters `COMP971_REC_COOLDOWN` for `5,000 ms`.
2. **Even when `Attempt 1` immediately succeeds and valid 5 Hz ranging resumes**, `g_fira_valid_sequence` and `g_fira_cb_valid_streak` stay `0` on the Controlee.
3. After `5,000 ms` (`COMP971_REC_COOLDOWN_MS`), the Controlee falsely concludes that `Attempt 1` failed, transitions to `COMP971_REC_BACKOFF` for `2,000 ms`, and then **tears down its healthy, actively ranging UWB session a second time (`Attempt 2`)**!
4. After `Attempt 2` completes, it waits another `5,000 ms` in `COMP971_REC_COOLDOWN` (ignoring all valid callbacks again), increments `m_rec_attempt_count` to `3` (`>= COMP971_REC_MAX_ATTEMPTS`), emits `0xED` (`COMP971_REC_DIAG_HOLD_ENTER`, `detail = 3`), and enters `COMP971_REC_HOLD` for `30,000 ms`!

#### 3. Complete Empirical Proof from `gateway_raw_2026-10-12_101320_decoded/uwb_diag.ndjson`
In the Step 28 capture ([`gateway_raw_2026-10-12_101320_decoded/uwb_diag.ndjson`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/acquisition/gateway_raw_2026-10-12_101320_decoded/uwb_diag.ndjson)), Node 2 (Left Wrist, `0x0002`) was flashed with v6.7. Every single time Node 2 triggered recovery during the 30-minute run (**8 times in total**), it executed this exact `Attempt 1 -> +2.1s Attempt 2 -> +5.0s HOLD_ENTER (0xED)` cascade, with **zero** `0xE5` (`COOLDOWN_EXIT`) or `0xEA` (`VALID_OBS`) events ever emitted on Node 2:

| Cascade # on Node 2 (LW) | `Attempt 1` (`0xE3 -> 0xE2`, `detail=1`) `local_timestamp_us` | `Attempt 2` (`0xE3 -> 0xE2`, `detail=2`) `local_timestamp_us` | `HOLD_ENTER` (`0xED`, `detail=3`) `local_timestamp_us` | Observed Delta (`Attempt 2` to `HOLD_ENTER`) |
| :--- | :--- | :--- | :--- | :--- |
| **Cycle 1** | `2,228,615,000` (2228.62 s) | `2,230,742,000` (+2.13 s) | `2,235,787,000` | `+5.045 s` (`COMP971_REC_COOLDOWN_MS`) |
| **Cycle 2** | `2,336,053,000` (2336.05 s) | `2,343,191,000` (+7.14 s) | `2,348,236,000` | `+5.045 s` (`COMP971_REC_COOLDOWN_MS`) |
| **Cycle 3** | `2,389,031,000` (2389.03 s) | `2,396,170,000` (+7.14 s) | `2,401,215,000` | `+5.045 s` (`COMP971_REC_COOLDOWN_MS`) |
| **Cycle 4** | `2,512,960,000` (2512.96 s) | `2,520,101,000` (+7.14 s) | `2,525,146,000` | `+5.045 s` (`COMP971_REC_COOLDOWN_MS`) |
| **Cycle 5** | `2,712,866,000` (2712.87 s) | `2,720,008,000` (+7.14 s) | `2,725,053,000` | `+5.045 s` (`COMP971_REC_COOLDOWN_MS`) |
| **Cycle 6** | `2,794,422,000` (2794.42 s) | `2,801,563,000` (+7.14 s) | `2,806,608,000` | `+5.045 s` (`COMP971_REC_COOLDOWN_MS`) |
| **Cycle 7** | `3,007,619,000` (3007.62 s) | `3,014,760,000` (+7.14 s) | `3,019,806,000` | `+5.046 s` (`COMP971_REC_COOLDOWN_MS`) |
| **Cycle 8** | `3,399,151,000` (3399.15 s) | `3,406,294,000` (+7.14 s) | `3,411,340,000` | `+5.046 s` (`COMP971_REC_COOLDOWN_MS`) |

Notice the `7.141 s` interval between `Attempt 1` and `Attempt 2` in Cycles 2–8 (`5,000 ms` `COMP971_REC_COOLDOWN_MS` + `2,000 ms` `COMP971_REC_BACKOFF_MS` + ~`141 ms` `uwb_task` polling/restart execution) followed by `5.045 s` (`5,000 ms` cooldown) to `HOLD_ENTER` (`0xED`). Every single cycle was forced to perform a redundant `Attempt 2` MAC teardown because `cb_has_valid_measurement` was never evaluated on the Controlee.

---

### Issue 4 (HIGH — Controlee Watchdog Asymmetry & Mixed Firmware Versions Across Nodes)

#### 1. Code Evidence
- [`comp971_uwb_official_fira.c:553-559`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L553-L559) and [`lines 939-977`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L939-L977)
- [`comp971_config_shared.h:42-49`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/include/comp971_config_shared.h#L42-L49) (`g_comp971_restart_jitter_ms`)

#### 2. Technical Justification & Empirical Proof
1. **Asymmetric Callback Behavior**:
   - On the Controller (`0x0001`), `fira_notification_cb` is invoked by the Qorvo FiRa stack at the end of every 200 ms round regardless of whether Controlees responded (populating `status = 33`, `QUWBS_FBS_STATUS_RANGING_RX_TIMEOUT`, for silent Controlees).
   - On a Controlee (`0x0002..0x0006`), if the Controlee misses the Controller's Poll frame (due to wrist/ankle body shadowing, antenna orientation null, or a Pelvis Controller reboot), `fira_notification_cb` is **not** called for that block, so `g_fira_last_cb_tick` stops advancing.
2. **Quantized 27–31 Block (`5.4–6.2 s`) Cold-Start Penalty**:
   When a Controlee misses Polls for `5,000 ms + g_comp971_restart_jitter_ms` (`5,250 ms` on LW, `5,500 ms` on RW, `5,750 ms` on LA, `6,000 ms` on Head, `6,250 ms` on RA), [`comp971_uwb_official_fira_Process()`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L964) calls [`comp971_uwb_stage2_mac_restart()`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L1127-L1155). Tearing down the active MAC and starting a fresh session (`block_index = 0`) while the Controller is at `block_index = K` forces the Controlee through a multi-second cold-start resynchronization window (~27 to 31 blocks = `5.4 to 6.2 s`).
3. **Mixed Firmware Deployment Across the 5 Controlees in `2026-10-12_101320`**:
   Counting the diagnostic events per node in [`gateway_raw_2026-10-12_101320_decoded/uwb_diag.ndjson`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/acquisition/gateway_raw_2026-10-12_101320_decoded/uwb_diag.ndjson) reveals that the 5 Controlees were running different firmware versions during the Step 28 capture:

| Node Index & Role | `0xE3` (`RESTART_BEGIN`) | `0xE2` (`RESTART_OK` / Legacy `WATCHDOG_RECOVERY`) | `0xED` (`HOLD_ENTER`) | Firmware Behavior Observed in `uwb_diag.ndjson` |
| :--- | :--- | :--- | :--- | :--- |
| **Node 2 (Left Wrist)** | 16 (`detail = 1..2`) | 16 (`detail = 1..2`) | 8 (`detail = 3`) | Running **v6.7** bounded recovery state machine (affected by Issue 3). |
| **Node 3 (Right Wrist)** | 0 | 16 (`detail = 0`) | 0 | Running **pre-v6.7** firmware (unconditional 5.5 s watchdog restart with `detail = 0`). |
| **Node 4 (Left Ankle)** | 0 | 26 (`detail = 0`) | 0 | Running **pre-v6.7** firmware (unconditional 5.75 s watchdog restart with `detail = 0`). |
| **Node 5 (Head)** | 0 | 0 | 0 | Zero watchdog triggers during the 30-minute run. |
| **Node 6 (Right Ankle)** | 0 | 0 | 0 | Zero watchdog triggers during the 30-minute run. |

Because Nodes 3 (RW) and 4 (LA) were still running pre-v6.7 firmware without bounded hold/backoff, whenever they lost sync for >5.5 s they repeatedly tore down and restarted their UWB MAC every `~5.5 to 5.75 s` (`16` times on RW, `26` times on LA), creating the `215 to 216 block` (`43.0 to 43.2 s`) dropout streaks seen in Section 3.2.

---

### Issue 5 (MEDIUM — FreeRTOS Task Priority Inversion, Same-Priority Contention, and 100 ms Polling Latency in `uwb_task`)

#### 1. Code Evidence
- [`main.c:423-441`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L423-L441) ([`uwb_task`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L423-L441))
- [`main.c:488-534`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L488-L534) ([`imu_task`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L488-L534))
- [`main.c:635-652`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L635-L652) (`xTaskCreate` priority assignments)
- [`comp971_uwb_official_fira.c:556-558`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L556-L558) and [`903-907`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L903-L907)

#### 2. Technical Justification
1. **Priority Inversion Against Qorvo's `DefaultTask` (Priority 1)**:
   In [`main.c:635-652`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L635-L652), `status_task`, `imu_task`, and `uwb_task` are all created at FreeRTOS priority `2U`, and `supervisor_task` is created at priority `3U`. Meanwhile, Qorvo's internal `DefaultTask` (which services `uwbmac` / MCPS event queues) is created at `osPriorityLow` (FreeRTOS priority `1U`).
   Whenever `imu_task` wakes up (every 20 ms) and executes blocking TWI/I2C transactions to read 14 bytes from the ICM-20948 plus BLE notification queueing, or whenever `status_task` drains the BLE ring buffer, `DefaultTask` (Priority 1) is preempted and cannot run. Any delay in `DefaultTask` directly eats into the ~`500 to 1,000 us` preparation lead time before the next UWB slot's `dx_time`, increasing the likelihood of the DW3110 `HPDWARN` error identified in Issue 1.
2. **100 ms Blind Polling Loop in `uwb_task` & Single-Slot Buffer Overwrite Risk**:
   In [`main.c:434-440`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L434-L440):
   ```c
   for (;;) {
     comp971_uwb_official_fira_Process();
     comp971_ble_pilot_process();
     vTaskDelay(pdMS_TO_TICKS(100U));
   }
   ```
   Instead of waking immediately when [`fira_notification_cb`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L557-L558) stores a new star report into `m_latest_star_report`, `uwb_task` sleeps for a blind `100 ticks` (`~100.7 ms`). This adds `0 to 100.7 ms` of variable latency before `comp971_ble_pilot_send_star_ranges(&report)` is called, and because `m_latest_star_report` is a single static struct (`m_latest_star_report = report; m_star_report_pending = true;`), if `uwb_task` is delayed across two callbacks or if `block_duration_ms` is ever reduced below `100 ms` (e.g., 10 Hz / 100 ms or 20 Hz / 50 ms), star reports will be silently overwritten before transmission.

---

### Issue 6 (MEDIUM — Clock Scaling Bug): UWB `local_timestamp_us()` Drifts by `-7,030.3 ppm` (`-0.703%`) Relative to IMU Timestamps and Wall Clock

#### 1. Code Evidence
Compare [`main.c:510-512`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L510-L512) (`imu_task`) with [`comp971_uwb_official_fira.c:238-242`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L238-L242) (`local_timestamp_us`):

- In [`main.c:510-512`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L510-L512):
  ```c
  /* True tick frequency is 32768 / 33 = 992.9697 Hz (portNRF52_RTC_PRESCALER = 32).
   * Using exact integer math: ticks * 1,000,000 * 33 / 32768 */
  const uint64_t timestamp_us =
      ((uint64_t)last_wake_tick * 33000000ULL) / 32768ULL;
  ```
- In [`comp971_uwb_official_fira.c:238-242`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L238-L242):
  ```c
  static uint64_t local_timestamp_us(void) {
    return ((uint64_t)xTaskGetTickCount() * 1000000U) / configTICK_RATE_HZ;
  }
  ```

#### 2. Mathematical & Empirical Proof
Because `configTICK_RATE_HZ` is `1000`, `local_timestamp_us()` in `comp971_uwb_official_fira.c` multiplies `xTaskGetTickCount()` by `1000 us/tick` instead of the true `33000000 / 32768 = 1007.080078125 us/tick`.
- Relative rate error: `(992.969697 - 1000.0) / 1000.0 = -0.0070303` (`-7,030.3 ppm`, or `-0.703%`).
- Over a 30-minute (`1,800 s`) capture, `pelvis_timestamp_us` in `COMP971_TELEMETRY_STAR_RANGES` (`0x06`) drifts behind `local_timestamp_us` in `COMP971_TELEMETRY_IMU_SAMPLE` (`0x01`) on the exact same Pelvis MCU by:
  ```text
  1,800 s * 0.0070303 = 12.654 seconds
  ```
- This is the exact reason [`STEP_28_DATASET_VALIDATION_REPORT.md:219-234`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/validation_step28/STEP_28_DATASET_VALIDATION_REPORT.md#L219-L234) noted that UWB and IMU packets cannot be joined directly on device `timestamp_us` without accumulating a multi-second offset and had to be aligned via gateway `received_at_us`.

---

### Issue 7 (LOW / DIAGNOSTIC — Telemetry Stage Code Collision): `COMP971_REC_DIAG_*` Overlaps `COMP971_DIAG_STALL_*` (`0xE1..0xEA`)

#### 1. Code Evidence
Compare [`comp971_uwb_official_fira.c:95-106`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L95-L106) with [`comp971_telemetry_protocol.h:76-88`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/include/comp971_telemetry_protocol.h#L76-L88):

| Hex Code | Meaning in [`comp971_telemetry_protocol.h`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/include/comp971_telemetry_protocol.h#L76-L88) (Pelvis Stall Snapshot) | Meaning in [`comp971_uwb_official_fira.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L95-L106) (Controlee Recovery) |
| :--- | :--- | :--- |
| `0xE1` | `COMP971_DIAG_STALL_DW3000_RX_OK_COUNT` | `COMP971_REC_DIAG_RESTART_FAIL` |
| `0xE2` | `COMP971_DIAG_STALL_DW3000_TX_COUNT` | `COMP971_REC_DIAG_RESTART_OK` (and legacy `STAGE_WATCHDOG_RECOVERY`) |
| `0xE3` | `COMP971_DIAG_STALL_FIRA_CB_SEQ` | `COMP971_REC_DIAG_RESTART_BEGIN` |
| `0xE4` | `COMP971_DIAG_STALL_DW3000_IRQ_COUNT` | `COMP971_REC_DIAG_HOLD_PROBE` |
| `0xE5` | `COMP971_DIAG_STALL_CB_LAST_IRQ_COUNT` | `COMP971_REC_DIAG_COOLDOWN_EXIT` |
| `0xE6` | `COMP971_DIAG_STALL_CB_LAST_TX_COUNT` | `COMP971_REC_DIAG_SESSION_TERMINATED` |
| `0xE7` | `COMP971_DIAG_STALL_LAST_TX_INFO` | `COMP971_REC_DIAG_SESSION_ERROR` |
| `0xE8` | `COMP971_DIAG_STALL_LAST_TX_SYS_TIME` | `COMP971_REC_DIAG_SESSION_STOPPED` |
| `0xE9` | `COMP971_DIAG_STALL_LAST_TX_STATUS` | `COMP971_REC_DIAG_SESSION_DEINIT` |
| `0xEA` | `COMP971_DIAG_STALL_LAST_TX_DIFF` | `COMP971_REC_DIAG_VALID_OBS` |
| `0xED` | *(Not defined in `comp971_telemetry_protocol.h`)* | `COMP971_REC_DIAG_HOLD_ENTER` |

Because of this overlap, [`decode_gateway_capture.py`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/tools/decode_gateway_capture.py) aggregates Controlee `RESTART_OK` (`0xE2`) and `RESTART_BEGIN` (`0xE3`) packets together with Pelvis stall snapshot fields in `summary.json` (`"stall_dw3000_tx_count": 60`, `"stall_fira_cb_seq": 18`), and reports `0xED` (`HOLD_ENTER`) as `"unknown": 8`.

---

### Issue 8 (ARCHITECTURAL — 5-Link Star vs. 15-Pair Full Mesh): Why the Legacy Zephyr Mesh Failed and What the Active Star Topology Trades Off

#### 1. Code & Design Context
- Active 5-Link FiRa Star: [`comp971_telemetry_protocol.h:27-32`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/include/comp971_telemetry_protocol.h#L27-L32) (`COMP971_MAX_STAR_PAIRS = 5U`)
- Planned 15-Pair Mesh Specification: [`UWB_15_PAIR_MESH_DESIGN.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/docs/UWB_15_PAIR_MESH_DESIGN.md)
- Legacy Custom Zephyr 15-Pair Mesh: [`hardware/firmware/archive/dwm3001cdk_legacy_proto/src/main.c:54-98`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/archive/dwm3001cdk_legacy_proto/src/main.c#L54-L98)
- Historical Diagnostic Record: [`FAILURE_LOG.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/diagnostics/2026-10-03_fira_tx_failed_remediation/FAILURE_LOG.md)

#### 2. Technical Comparison
1. **Why the Legacy Custom Zephyr 15-Pair Mesh (`dwm3001cdk_legacy_proto`) Underperformed**:
   - In [`dwm3001cdk_legacy_proto/src/main.c:54-98`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/archive/dwm3001cdk_legacy_proto/src/main.c#L54-L98), the Pelvis node iterated sequentially over all 15 node pairs (`k_pairs[15]`), sending an explicit `MSG_CMD_MEASURE` packet, waiting up to `WAIT_REPORT_TIMEOUT_MS = 40 ms` per attempt (`WAIT_STAGE_TIMEOUT_MS = 16 ms` inside DS-TWR) with up to `MAX_PAIR_ATTEMPTS = 3` retries per pair, and using a hardcoded `RESP_TX_DELAY_US = 2000U` / `FINAL_TX_DELAY_US = 2000U`.
   - Because any single occluded pair blocked the entire sequential schedule for up to `3 * 40 ms = 120 ms` and the custom driver lacked Qorvo `l1_config` OTP calibration and suffered frequent `HPDWARN` failures (`33.0%` raw attempt failure rate documented in [`FAILURE_LOG.md`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/outputs/records/diagnostics/2026-10-03_fira_tx_failed_remediation/FAILURE_LOG.md)), full 15-pair sweep throughput collapsed to `1.21 Hz` (`~826 ms` per sweep).
2. **What the Active Qorvo FiRa Star (`qorvo_qm33_motion_node`) Achieves and Trades Off**:
   - By switching to Qorvo's hardware-timed FiRa one-to-many DS-TWR session (`session_id = 42`, `200 ms` block, `25 * 2 ms = 50 ms` active round), non-responding Controlees time out within their dedicated `2 ms` slot without delaying any other Controlee in the block.
   - However, it measures only the **5 Pelvis-centered links** (`Pelvis <-> Head/LW/RW/LA/RA`) and does not measure the **10 inter-peripheral links** (`LW <-> RW`, `LA <-> RA`, `Wrists <-> Head`, etc.).

---

## 6. Prioritized, Evidence-Backed Engineering Recommendations

Below are the exact, prioritized engineering recommendations to resolve Issues 1 through 7 in [`hardware/firmware/qorvo_qm33_motion_node/`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node).

---

### Recommendation 1 (CRITICAL — Immediate Fix for Issue 3): Move Controlee Valid-Callback Tracking Outside `if (g_comp971_uwb_role == COMP971_UWB_CONTROLLER)` and Flash All 5 Controlees

- **Target File**: [`comp971_uwb_official_fira.c:478-589`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L478-L589)
- **What to Change**:
  Compute `cb_has_valid_measurement` from `info->results[i].status == QUWBS_FBS_STATUS_OK` **unconditionally** at the top of `QUWBS_FBS_NOTIFICATION_RANGING_DATA` in [`fira_notification_cb()`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L466-L590), and execute the `g_fira_valid_sequence++` / `g_fira_cb_valid_streak++` / `g_fira_last_valid_cookie` block **outside** `if (g_comp971_uwb_role == COMP971_UWB_CONTROLLER)`.
  ```c
  /* Evaluate valid measurements for BOTH Controller and Controlee roles */
  bool cb_has_valid_measurement = false;
  for (int i = 0; i < info->n_measurements && i < COMP971_MAX_STAR_PAIRS; i++) {
    if (info->results[i].status == QUWBS_FBS_STATUS_OK) {
      cb_has_valid_measurement = true;
      break;
    }
  }
  ```
- **Why**:
  Allows Controlees in `COMP971_REC_COOLDOWN` to detect that their first MAC restart (`Attempt 1`) restored valid ranging after 2 callbacks (`400 ms`), returning immediately to `COMP971_REC_NORMAL` (`0xE5`) instead of tearing down a working UWB session a second time (`Attempt 2`) and escalating into a 30-second hold (`0xED`). Flash this unified build across **all 5 Controlees** (`0x0002..0x0006`) so Nodes 3 and 4 are also upgraded from legacy pre-v6.7 firmware.

---

### Recommendation 2 (CRITICAL — Mitigate Issue 1 & Fix Issue 5): Eliminate FreeRTOS Priority Inversion Against Qorvo's `DefaultTask` and Make `uwb_task` Event-Driven

- **Target Files**: [`main.c:423-441, 635-652`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L635-L652) and [`comp971_uwb_official_fira.c:556-559`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L556-L559)
- **What to Change**:
  1. Raise Qorvo's `DefaultTask` (the `uwbmac` / FiRa session runner) above `imu_task`, `uwb_task`, and `status_task` (or lower `uwb_task` and `status_task` to Priority `1` and elevate `DefaultTask` to Priority `3`), ensuring that when `TIMER2` or `GPIOTE` wakes the UWB MAC to prepare a delayed transmit (`CMD_DTX`), it is never blocked behind a 20 ms `imu_task` I2C read or BLE buffer flush.
  2. Replace the blind `vTaskDelay(pdMS_TO_TICKS(100U))` in [`uwb_task`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L439) with `ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100U))`, signaled from [`fira_notification_cb()`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L558) via `xTaskNotifyGive()`.
- **Why**:
  Directly protects the `~460–670 us` delayed-TX margin (`dx_time - cur_sys`) where `5 out of 7` Pelvis `HPDWARN` stalls occurred, and eliminates the `0–100 ms` polling delay and single-slot overwrite window on `m_latest_star_report`.

---

### Recommendation 3 (HIGH — Fix Issue 2): Replace Immediate Pelvis `Stage 3` (`NVIC_SystemReset()`) With In-Process `Stage 2` MAC Restart in `supervisor_task`

- **Target File**: [`main.c:280-345`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L280-L345)
- **What to Change**:
  When `supervisor_task` detects a Controller stall (`cb_delta_ms >= 1500..2000 ms`), first attempt an in-process UWB MAC restart via [`comp971_uwb_stage2_mac_restart()`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L1127-L1155) (after emitting the `0xE0..0xEB` stall snapshot). Only escalate to `Stage 3` (`NVIC_SystemReset()`) if `comp971_uwb_stage2_mac_restart()` fails or if callbacks do not resume within `2,000 ms` of the in-process restart.
- **Why**:
  1. **Zero Pelvis IMU Data Loss**: Because the nRF52833 does not reboot and the BLE connection stays up, `imu_task` continues streaming at 50 Hz without interruption (eliminating the `6.3–6.8 s` / `381-sample` Pelvis IMU gaps).
  2. **4x Faster UWB Recovery**: Avoids MCU boot time, avoids the `1,500 ms` `COMP971_UWB_START_DELAY_MS`, and avoids BLE Central reconnection delay, cutting the UWB outage from **9.0–10.2 seconds** down to **~1.5–2.5 seconds** (below the Controlee watchdog threshold, preventing secondary Controlee teardowns).

---

### Recommendation 4 (HIGH — Mitigate Issue 4): Increase Controlee Watchdog Threshold (`COMP971_REC_WATCHDOG_BASE_MS`) From `5,000 ms` to `8,000–10,000 ms`

- **Target File**: [`comp971_uwb_official_fira.c:66`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L66)
- **What to Change**:
  Increase `COMP971_REC_WATCHDOG_BASE_MS` from `5000U` to `8000U` (or `10000U`).
- **Why**:
  During dynamic limb motion, a wrist or ankle node can experience temporary non-line-of-sight body shadowing for 2–5 seconds while its localFiRa slot timer is still synchronized with the Controller. Tearing down the Controlee's MAC at `5.0 s` converts a temporary RF occlusion (or a Pelvis recovery window) into a mandatory `5.4–6.2 s` cold-start re-acquisition outage.

---

### Recommendation 5 (MEDIUM — Fix Issue 6): Use Exact RTC1 Scaling in `local_timestamp_us()` in `comp971_uwb_official_fira.c`

- **Target File**: [`comp971_uwb_official_fira.c:238-242`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L238-L242)
- **What to Change**:
  Update [`local_timestamp_us()`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L238-L242) to use the exact same `32768 / 33 Hz` RTC1 formula already used by `imu_task` in [`main.c:510-512`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L510-L512):
  ```c
  static uint64_t local_timestamp_us(void) {
    return ((uint64_t)xTaskGetTickCount() * 33000000ULL) / 32768ULL;
  }
  ```
- **Why**:
  Eliminates the `-7,030.3 ppm` (`12.65 s` per 30 minutes) clock drift between UWB `pelvis_timestamp_us` and IMU `local_timestamp_us`, making on-device Pelvis UWB and IMU timestamps 100% synchronous.

---

### Recommendation 6 (LOW — Fix Issue 7): Decouple `COMP971_REC_DIAG_*` Stage Codes From `COMP971_DIAG_STALL_*` (`0xE0..0xEB`)

- **Target Files**: [`comp971_uwb_official_fira.c:95-106`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L95-L106), [`comp971_telemetry_protocol.h:76-88`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/include/comp971_telemetry_protocol.h#L76-L88), and [`decode_gateway_capture.py`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/tools/decode_gateway_capture.py)
- **What to Change**:
  Reassign `COMP971_REC_DIAG_*` to a dedicated non-overlapping range (for example, `0xC1..0xCD`) in `comp971_telemetry_protocol.h` and register those names in `decode_gateway_capture.py`.
- **Why**:
  Prevents Controlee recovery diagnostics from polluting Pelvis `stall_dw3000_tx_count` (`0xE2`) and `stall_fira_cb_seq` (`0xE3`) counts in `summary.json`.
