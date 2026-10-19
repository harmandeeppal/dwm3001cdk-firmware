# Hardware Operational Tools Index

This directory contains the operational tools for flashing, capturing, decoding, validating, and diagnosing the COMP971 wearable motion capture system.

---

## 1. Core Operational Tools (Top Level)

The top level contains only the essential, everyday operational tools:

| Tool | Type | Purpose | Example Command |
|---|---|---|---|
| **`validate_hardware_config.ps1`** | PowerShell | Pre-flight validation verifying that all 6 board identities, J-Link serials, body positions, and profiles match the authoritative registry. | `.\hardware\tools\validate_hardware_config.ps1` |
| **`build_unified_motion_node.ps1`** | PowerShell | Compiles the single unified firmware binary (`Debug_UNIFIED_star`) for all wearable nodes using the official Qorvo SDK and CMake. | `.\hardware\tools\build_unified_motion_node.ps1 -Clean` |
| **`flash_motion_node.ps1`** | PowerShell | Safely flashes a node via J-Link using `--sectorerase` only, permanently protecting factory calibration sectors 126–127 and UICR. | `.\hardware\tools\flash_motion_node.ps1 -Snr 760225805 -HexPath <PATH> -NodeName HEAD` |
| **`live_six_node_viewer.ps1`** | PowerShell | Real-time console TUI dashboard displaying live 6-node IMU sample rates, sequence gaps, and UWB ranges. | `.\hardware\tools\live_six_node_viewer.ps1 -Port COM16` |
| **`capture_gateway_high_perf.py`** | Python | High-throughput multithreaded serial capture backend for the nRF52840 USB Gateway. | `python hardware/tools/capture_gateway_high_perf.py --port COM16 --duration 60` |
| **`decode_gateway_capture.py`** | Python | Parses raw binary `.bin` capture archives into timestamped NDJSON streams (`imu.ndjson`, `star_ranges.ndjson`, `summary.json`). | `python hardware/tools/decode_gateway_capture.py <CAPTURE.bin>` |
| **`prepare_step28b_dataset.py`** | Python | Canonical Step 28B dataset pipeline: aligns 60 Hz IMU and 5 Hz UWB to an exact integer time grid (0.0 ppm drift) with causal future-leakage protection. | `python hardware/tools/prepare_step28b_dataset.py` |
| **`test_step28b_pipeline.py`** | Python / pytest | Regression verification suite for the Step 28B dataset pipeline. | `pytest hardware/tools/test_step28b_pipeline.py` |
| **`run_disconnect_stress_test.py`** | Python | 60-cycle / 240s BLE disconnect/reconnect stress test evaluating transport recovery. | `python hardware/tools/run_disconnect_stress_test.py` |

---

## 2. Directory Structure

- **`jlink/`** — SEGGER J-Link Commander scripts (`.jlink`) for non-intrusive runtime register readbacks, memory dumps, and stall injection tests.
- **`diagnostics/`** — In-depth diagnostic runners, RAM dump decoders, task watermark analyzers, and empirical test capture scripts (Phase B′, C, on-body, etc.).
- **`pipelines/`** — Historical intermediate dataset pipelines and regression tests (`prepare_step28_dataset.py`, `prepare_step28a_dataset.py`, etc.).
- **`validation/`** — Backend validation modules (e.g. `validate_config.py`) invoked by top-level checkers.
