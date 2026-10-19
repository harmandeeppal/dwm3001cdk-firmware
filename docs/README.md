# Hardware Documentation Index

This directory contains the authoritative technical documentation, empirical audit reports, calibration models, and handover guides for the COMP971 wearable hardware subsystem.

---

## 1. Primary Architecture & Status Documents

- **[00: Hardware System Guide](00_HARDWARE_SYSTEM_GUIDE.md)**  
  *System architecture guide:* physical wiring, electrical specifications, pin mappings, sampling rates, safety rules, and troubleshooting procedures.
- **[01: Current System Status (19 October 2026)](01_CURRENT_SYSTEM_STATUS.md)**  
  *Authoritative operational status:* 6-node star telemetry, 30-minute benchmark soak results, Step 28B causal 60 Hz dataset validation, live demo diagnostic results, and firmware version audit.
- **[02: Wire Protocol & Telemetry Specification](02_WIRE_PROTOCOL_AND_TELEMETRY_SPEC.md)**  
  *Protocol specification:* loss-aware binary framed USB protocol v1, gateway packet structures, and telemetry types.

---

## 2. Empirical Verification & Calibration Reports

- **[03: Two-Node Baseline & Empirical Calibration Report](03_TWO_NODE_BASELINE_AND_CALIBRATION_REPORT.md)**  
  *Two-node baseline:* 100% steady-state DS-TWR ranging, fixtured bench repeatability (Phase B′), symmetric distance sweep (Phase C, 30 to 150 cm), orientation sensitivity (Phase D′), on-body walking validation (Phase E), and autonomous gateway unplug resilience.
- **[04: Multi-Node Scheduling Investigation](04_MULTI_NODE_SCHEDULING_INVESTIGATION.md)**  
  *Scheduling analysis:* TDMA time slot budgeting, 5-pair star vs. 15-pair mesh evaluation, radio duty cycle, and FreeRTOS task pacing.

---

## 3. Authoritative Embedded Audits (16 October 2026)

- **[05: UWB Node Asymmetry & Root Cause Audit Report](05_AUDIT_UWB_NODE_ASYMMETRY_AND_ROOT_CAUSES.md)**  
  *Root-cause audit:* detailed empirical decomposition of the 30-minute soak benchmark (`2026-10-12_101320`), isolating the five core firmware bugs causing link asymmetry between Head (99.5%) and limb nodes (62%–72%).
- **[06: UWB Scheduling, Concurrency & Full System Audit](06_AUDIT_UWB_SCHEDULING_AND_CONCURRENCY.md)**  
  *Full system audit:* comprehensive 8-issue engineering audit covering FreeRTOS concurrency, priority inversion against Qorvo `DefaultTask`, RTC1 timestamp scaling, and delayed-TX deadline misses.

---

## 4. Embedded Software Engineering & Handover

- **[07: Official Qorvo SDK Integration Strategy](07_OFFICIAL_SDK_INTEGRATION_STRATEGY.md)**  
  *Migration architecture:* strategy for adopting the official Qorvo QM33 SDK 1.1.1 and isolating flash calibration sectors using `comp971_qflash_ram_guard.c`.
- **[08: Qorvo SDK Source Review](08_QORVO_SDK_SOURCE_REVIEW.md)**  
  *Driver inspection:* technical review of Qorvo `uwbstack_libs`, QHAL, and `fira_helper`.
- **[09: Handover & Debugging Manual](09_HANDOVER_AND_DEBUGGING_MANUAL.md)**  
  *Operational manual:* physical setup, flash calibration safety, golden backup verification, and step-by-step bisection debugging procedures.

---

## 5. Historical Research & Reference Notes (`reference/`)

Archived technical records from early development and transitional phases:
- `reference/01-live-hardware-readiness.md`: Initial hardware checkout.
- `reference/02-official-platform-review.md`: Platform SDK review.
- `reference/03-six-node-mesh-and-six-axis-imu-design.md`: Early 15-pair mesh design.
- `reference/04-dwm3001cdk-full-mesh-capability-and-validation-status.md`: Mesh capability status.
- `reference/05-final-ble-uwb-telemetry-and-synchronisation-plan.md`: Early synchronization proposal.
- `reference/06-verified-six-node-deployment-and-live-telemetry.md`: Initial 6-node live telemetry record.
- `reference/historical_2026-09-29_transport_stability_update.md`: 29 September bring-up update.
- `reference/historical_2026-10-03_official_uwb_remediation_plan.md`: 3 October remediation plan.
- `reference/historical_2026-10-05_uwb_fira_takeover_guide.md`: 5 October takeover guide.
