# STEP 28: OFFLINE IMU/UWB DATASET VALIDATION AND SENSOR-FUSION READINESS REPORT

**Project:** AUT COMP971 — 150 ms Ahead Human Motion Prediction  
**Mode:** Offline data analysis and software development only  
**Firmware Baseline:** Step 17 unified release (`FB584C95...`)  
**Capture Target:** Step 19 raw gateway capture (`2A9BD73C...`)  
**Hardware Status:** Quiescent, untouched, strictly no operations  

---

## 1. Executive Summary

Step 28 validates the physical 6-node IMU and 5-link FiRa UWB telemetry dataset collected in Step 19 and evaluates whether the measured hardware data can support causal 150 ms-ahead human motion prediction.

### Key Technical Findings

1. **Reproduction of Benchmark Ground Truth**:
   The immutable 30-minute dataset (`gateway_raw_2026-10-12_101320.bin`, SHA-256: `2A9BD73C...`) was independently parsed by a new standalone analysis pipeline ([`prepare_step28_dataset.py`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/hardware/tools/prepare_step28_dataset.py)). All benchmark metrics were reproduced with bitwise precision:
   - Total star-ranging blocks logged: **8,871**.
   - Total valid range measurements: **35,389 / 44,355** (**79.79%** aggregate within-block validity).
   - Total IMU records received across 6 nodes: **640,924**.
   - Both coordinator stall events (Incident 1 at block 3,371; Incident 2 at block 5,887) and both non-reset BLE transport delays were reproduced.
2. **Reconciliation of Node 2 Sequences and Resets**:
   Node 2 (Left Wrist) logged 107,586 records across sequence span `[53,514 .. 161,513]` (span = 108,000). A negative sequence difference at index 67,536 was proven to be caused by **6 duplicated BLE frames** re-received with identical local timestamps (`local_us = 2,020,002,227 .. 2,020,085,815 us`). Node 2 experienced **0 MCU resets**, and net missing packets were exactly **420**.
3. **Clock Drift and Timestamp Hierarchy**:
   Each node executes an independent hardware clock at **60.0000 Hz** nominal sampling rate. Relative to the nRF52840 USB gateway reception timebase (`gw_us`), the individual node clocks exhibit an empirical drift of **-23.7 to -85.7 ppm** (~1.4 to 5.1 ms per minute). BLE notification transport groups samples into batches of 6, introducing **16.6 to 100 ms of transport arrival jitter**. Consequently, sensor fusion must use node local timestamps for relative sample spacing and gateway arrival timestamps solely for causal stream alignment.
4. **Topology Gap: 5 Hardware Links vs. 15 Theoretical Links**:
   The physical wearable hardware implements a **5-link star topology** centered on the Pelvis coordinator (Pelvis–Head, Pelvis–Left Wrist, Pelvis–Right Wrist, Pelvis–Left Ankle, Pelvis–Right Ankle). The remaining 10 cross-body pairwise distances (e.g. Wrist–Wrist, Ankle–Ankle, Head–Wrist) are **not measured by hardware**. The deep learning input contract must be explicitly adapted (Design A) rather than hallucinating synthetic measurements as hardware truth.
5. **Causal 60 Hz Multimodal Representation**:
   A causal forward alignment engine was implemented and verified with automated test suites ([`test_step28_pipeline.py`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/hardware/tools/test_step28_pipeline.py)). The pipeline guarantees **zero future data leakage**: at grid time $t_k$, model inputs contain only the most recently received IMU samples and UWB ranges with strict age thresholds and explicit binary validity masks.

---

## 2. Existing Analysis Pipeline Audit

Before creating new tooling, the codebase was audited for existing capture decoders, fusion scripts, and data loaders:

| Existing Script / Module | Current Functionality | Audit Verdict & Limitations |
| :--- | :--- | :--- |
| [`decode_gateway_capture.py`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/hardware/tools/decode_gateway_capture.py) | Parses USB binary protocol v1 into Python generators (`as_imu`, `as_star_ranges`, `as_stall_snapshot`). | **Functional Core**. Validates binary sync word `0xC971` and unpacks structures accurately. Lacks dataset-level integrity auditing and time alignment. |
| [`analyze_soak_v10.py`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/hardware/tools/analyze_soak_v10.py) | Analyzes long-duration test runs for task watermarks and disconnections. | **Functional for Diagnostics**. Specialized for memory and stack depth checks; not designed for multimodal sensor fusion. |
| [`models/input_contract.py`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/workspace/models/input_contract.py) | Tensor concatenation for UWB values and validity masks. | **Incompatible with 5-Link Hardware**. Assumes configured pairwise link count matches 15 pairs from `smplh_wearable_topology_v0.json`. |
| [`models/topology.py`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/workspace/models/topology.py) | Canonical topology loader based on AMASS/SMPL-H 6-node setup. | **Assumes 15 Pairs**. Hardcoded expectation: `len(uwb_pair_order) == 15` ($6 \times 5 / 2$). Rejects 5-link star topologies at initialization. |
| [`datasets/sensor_archive_adapter.py`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/workspace/datasets/sensor_archive_adapter.py) | Formats AMASS synthetic sensor streams for neural network training. | **Synthetic AMASS Only**. Designed for simulated UWB chords computed from SMPL mesh vertices. Cannot ingest raw binary hardware captures. |

### Component Status Summary

- **Working**: Binary frame parsing, raw field unpacking, stall snapshot decoding.
- **Requiring Corrections**: Reset detection logic (preventing duplicate packets from tripping false reset flags); UWB link order definitions.
- **Missing Prior to Step 28**:
  - Unified data integrity and sequence gap auditing engine.
  - Multi-node clock drift and timestamp synchronization estimator.
  - Causal 60 Hz temporal alignment grid with age tracking.
  - 5-link hardware-compatible neural network input schema.

---

## 3. Reproduced Step 19 Benchmark Statistics

The dataset was parsed independently using [`prepare_step28_dataset.py`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/hardware/tools/prepare_step28_dataset.py) and output to [`step28_benchmark_statistics.json`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/hardware/outputs/records/validation_step28/step28_benchmark_statistics.json).

### 3.1 Five-Link UWB Star-Ranging Statistics

```text
Session Duration:               1,800.000 seconds (30.0 minutes)
Nominal Blocks (at 5.0 Hz):     9,000 blocks
Recorded Star Blocks:           8,871 blocks (Block Reception Coverage: 98.57%)
Unrecorded Blocks:              129 blocks (Incident 1: ~51, Incident 2: ~47, BLE gaps: ~31)
Evaluated Slot Opportunities:   44,355 slots (8,871 blocks * 5 links)
Valid Distance Measurements:    35,389 slots (Aggregate Validity: 79.79%)
```

#### Link Breakdown

| Link Name | Pair Index | Target Node ID | Anatomical Vector | Valid Measurements | Evaluated Slots | Observed Validity |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `pelvis_head` | Pair 0 | `0x0002` (Node 0) | Torso to Cranium | **8,829** | 8,871 | **99.53%** |
| `pelvis_left_wrist` | Pair 1 | `0x0003` (Node 2) | Pelvis to Left Arm | **6,411** | 8,871 | **72.27%** |
| `pelvis_right_wrist`| Pair 2 | `0x0004` (Node 3) | Pelvis to Right Arm | **5,530** | 8,871 | **62.34%** |
| `pelvis_left_ankle` | Pair 3 | `0x0005` (Node 4) | Pelvis to Left Leg | **6,103** | 8,871 | **68.80%** |
| `pelvis_right_ankle`| Pair 4 | `0x0006` (Node 5) | Pelvis to Right Leg | **8,516** | 8,871 | **96.00%** |
| **Total / Aggregate**| - | - | **5 Links** | **35,389** | **44,355** | **79.79%** |

### 3.2 Six-Node IMU Sample Counts and Sequence Loss Accounting

A total of **640,924 IMU samples** were decoded:

| Node Index | Anatomical Placement | Total Received Samples | Logged Sequence Range | Duplicate Samples | Sequence Jumps | Net Missing Samples | Observed MCU Resets |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Node 0** | Head | **105,882** | `[47,016 .. 155,021]` | 0 | 106 | 2,124 | 0 |
| **Node 1** | Pelvis (Coordinator)| **106,868** | `[22,903 .. 19,277]` (multiboot) | 36,302 (wrap/reboot)| 2 | 381 | **2** (Boots 7 -> 8 -> 9) |
| **Node 2** | Left Wrist | **107,586** | `[53,514 .. 161,513]` | 6 (BLE re-delivery) | 4 | **420** | **0** |
| **Node 3** | Right Wrist | **107,080** | `[46,752 .. 154,757]` | 0 | 28 | 926 | 0 |
| **Node 4** | Left Ankle | **107,602** | `[46,542 .. 154,547]` | 0 | 3 | 404 | 0 |
| **Node 5** | Right Ankle | **105,906** | `[46,404 .. 154,379]` | 0 | 142 | 2,070 | 0 |

#### Categorical Distinction of Sample Statuses

- **Samples actually measured**: Indicated by continuous sequence increments.
- **Samples received at the gateway**: 640,924 validly framed binary records.
- **Packets lost during transport**: Quantified by sequence jumps (`diff > 1`) during active boot epochs (e.g. 2,124 samples on Node 0; 2,070 samples on Node 5).
- **Sampling interrupted by reset**: Node 1 ceased local sampling during the two supervisor recovery intervals:
  - Incident 1: 6.992 s interruption (sample 40,679 to 372).
  - Incident 2: 5.880 s interruption (sample 70,859 to 294).
- **Missing observations that cannot be reconstructed**: Local IMU samples during Pelvis reboots do not exist in hardware and cannot be reconstructed without relying on external interpolation.

---

## 4. Timestamp Hierarchy and Synchronization Assessment

```text
[Node Local IMU Sampling Clock]
   - FreeRTOS timer derived from nRF52833 RTC1 (32.768 kHz LFCLK crystal)
   - Nominal interval: 16,666.67 us (60.0000 Hz)
   - Drift: -23.7 to -85.7 ppm relative to Gateway
   - Discontinuity: Resets to 0 upon MCU software reset
           |
           v (Local batching into 6-sample BLE notifications)
[BLE Transport Layer]
   - Nordic SoftDevice S113 Connection Events (interval: 15-30 ms)
   - Incurrs 16.6 ms to 100 ms queuing and transmission latency
           |
           v (USB CDC framing at nRF52840 Gateway)
[Gateway Reception Clock (gw_us)]
   - Monotonic 64-bit microsecond counter on Gateway MCU
   - Serves as the Common Reference Timeline for multi-stream alignment
           ^
           | (Over-the-air Star Ranging completed at DW3000 transceiver)
[FiRa UWB Pelvis Timebase (pelvis_us)]
   - Recorded by Node 1 at completion of ranging round
   - Offsets by ~491,180 us relative to gw_us during Boot 7
   - Resets to 0 upon Node 1 software reboot
```

### Conversion Equations and Alignment Formulation

1. **Intra-Node Continuous Elapsed Time**:
   For any single node within an un-rebooted session epoch:
   ```text
   t_node_elapsed(k) = (local_timestamp_us[k] - local_timestamp_us[0]) * 1e-6 seconds
   ```
   This representation preserves microsecond-level sample spacing (variance < 2 us).
2. **Gateway Global Reference Timeline**:
   To align all six asynchronous nodes and the UWB stream without causal leakage:
   ```text
   t_ref(k) = gateway_receive_timestamp_us[k]
   ```
3. **Synchronization Uncertainty**:
   - Intra-node sample-to-sample jitter: **< 10 microseconds**.
   - Cross-node arrival jitter due to BLE batching: **16.6 to 100 milliseconds**.
   - Long-term crystal frequency drift across nodes: **~35 microseconds per second** (35 ppm).
4. **Reboot Boundary Handling**:
   Node 1 local timestamps reset to zero at $t = 303.333 \text{ s}$ and $t = 1483.651 \text{ s}$. Local timestamps cannot be subtracted across reboots. Multi-stream alignment must register to the gateway timebase (`gw_us`), with state-space filters resetting their Pelvis covariance matrices at each boot epoch.

---

## 5. Actual UWB Topology vs Original Planned Topology

### Topology Matrix Comparison

```text
========================================================================================
              PAIRWISE UWB TOPOLOGY COMPARISON MATRIX (6 NODES)
========================================================================================
Node Pair                Link Index     Design A (Current Hardware)  Design B (Original)
----------------------------------------------------------------------------------------
Pelvis <-> Head             Pair 0      MEASURED (Ch 9, 5 Hz)        MEASURED
Pelvis <-> Left Wrist       Pair 1      MEASURED (Ch 9, 5 Hz)        MEASURED
Pelvis <-> Right Wrist      Pair 2      MEASURED (Ch 9, 5 Hz)        MEASURED
Pelvis <-> Left Ankle       Pair 3      MEASURED (Ch 9, 5 Hz)        MEASURED
Pelvis <-> Right Ankle      Pair 4      MEASURED (Ch 9, 5 Hz)        MEASURED
Head <-> Left Wrist         Pair 5      NOT MEASURED (Absent)        MEASURED / SYNTHETIC
Head <-> Right Wrist        Pair 6      NOT MEASURED (Absent)        MEASURED / SYNTHETIC
Head <-> Left Ankle         Pair 7      NOT MEASURED (Absent)        MEASURED / SYNTHETIC
Head <-> Right Ankle        Pair 8      NOT MEASURED (Absent)        MEASURED / SYNTHETIC
Left Wrist <-> Right Wrist  Pair 9      NOT MEASURED (Absent)        MEASURED / SYNTHETIC
Left Wrist <-> Left Ankle   Pair 10     NOT MEASURED (Absent)        MEASURED / SYNTHETIC
Left Wrist <-> Right Ankle  Pair 11     NOT MEASURED (Absent)        MEASURED / SYNTHETIC
Right Wrist <-> Left Ankle  Pair 12     NOT MEASURED (Absent)        MEASURED / SYNTHETIC
Right Wrist <-> Right Ankle Pair 13     NOT MEASURED (Absent)        MEASURED / SYNTHETIC
Left Ankle <-> Right Ankle  Pair 14     NOT MEASURED (Absent)        MEASURED / SYNTHETIC
========================================================================================
```

### Model Input Design Comparison

| Evaluation Dimension | Design A: 5 Measured Hardware Links | Design B: 15 Pairwise Links (Theoretical) |
| :--- | :--- | :--- |
| **Hardware Fidelity** | **100% Ground Truth**. Ingests only real DW3000 physical-layer measurements. | Requires 10 synthetic links fabricated via kinematics or simulation. |
| **Model Input Vector** | 6 IMUs (36-D) + 5 UWB ranges + 5 validity masks + 5 age values = **51-D**. | 6 IMUs (36-D) + 15 UWB ranges + 15 validity masks = **66-D**. |
| **Kinematic Observability**| Anchors all extremities directly to the pelvis root. Limb-to-limb cross distances must be inferred by the model's kinematic backbone. | Provides explicit geometric constraints between hands and feet (e.g. clapping, foot clearance). |
| **Dissertation Defensibility**| **High**. Demonstrates real-world wearable deployment under actual RF constraints. | Moderate to Low if synthetic cross-links are presented without clear labelling. |
| **Recommendation** | **Adopt Design A as primary hardware-evaluation track**. | Retain Design B strictly for synthetic AMASS benchmark comparisons. |

---

## 6. Causal Freshness and Validity Policy

To prevent temporal leakage in predictive modeling, observations at grid time $t_k$ must obey a causal information barrier:

$$\mathcal{I}(t_k) = \{ x \in \mathcal{D} \mid t_{\text{receive}}(x) \le t_k \}$$

### Multi-State Validity Definition

For each of the 5 measured UWB links at inference step $t_k$:

```text
                 +--------------------------------------+
                 | Is most recent UWB receive time <= t |
                 +--------------------------------------+
                        |                    |
                       YES                   NO
                        |                    |
                        v                    v
          +---------------------------+   [NO MEASUREMENT]
          | Age = t - UWB_receive_time|   Range = -1, Mask = 0
          +---------------------------+
                 |             |
           Age <= 350 ms   Age > 350 ms
                 |             |
                 v             v
       +------------------+ [STALE MEASUREMENT]
       | Check raw_valid  | Range = -1, Mask = 0
       +------------------+
          |            |
        TRUE         FALSE
          |            |
          v            v
    [VALID RANGE]  [INVALID RANGE]
    Range = r_cm   Range = -1
    Mask = 1       Mask = 0
```

1. **Valid Measurement (`valid_mask = 1`)**:
   Received at or before $t_k$, age $\le 350 \text{ ms}$ (1.75 block intervals), and physical flag `valid = True` (`range_cm >= 0`).
2. **Invalid Measurement (`valid_mask = 0`, `raw_valid = False`)**:
   Received fresh, but DW3000 CIR threshold was not met or TOF calculation failed. Raw range recorded, model input clamped to `-1`.
3. **Stale Measurement (`valid_mask = 0`, `is_fresh = False`)**:
   Last valid measurement was received $> 350 \text{ ms}$ prior to $t_k$. Prevents outdated ranges from acting as active constraints.
4. **Coordinator Outage / Reset (`valid_mask = 0`)**:
   During the two supervisor reset windows (10.350 s and 9.418 s), all 5 UWB masks drop to 0. The neural network must transition to IMU-only dead-reckoning.

---

## 7. Causal 60 Hz Aligned Representation Specification

### Temporal Grid Structure

The dataset alignment engine maps asynchronous sensor streams to a regular 60.0 Hz reference timeline:
- Grid period: $\Delta t = 16,666.67 \text{ microseconds}$ ($\approx 16.67 \text{ ms}$).
- Reference time: $t_k = t_0 + k \times 16,667 \text{ us}$.

### Output Schema (`step28_causal_60hz_aligned.jsonl`)

Each line represents an independent temporal frame $k$:

```json
{
  "frame_index": 120,
  "t_rel_seconds": 2.0000,
  "grid_timestamp_us": 872408477,
  "imu": {
    "head":        {"sequence": 47141, "ax": 612, "ay": -288, "az": 16320, "gx": -12, "gy": 44, "gz": -8, "age_us": 4200, "valid": true},
    "pelvis":      {"sequence": 23027, "ax": 196, "ay": -744, "az": 16408, "gx": -2, "gy": 18, "gz": 4, "age_us": 6100, "valid": true},
    "left_wrist":  {"sequence": 53634, "ax": 240, "ay": -16, "az": 16712, "gx": 8, "gy": -12, "gz": -4, "age_us": 3800, "valid": true},
    "right_wrist": {"sequence": 46877, "ax": 598, "ay": -312, "az": 16296, "gx": 14, "gy": 22, "gz": 10, "age_us": 5100, "valid": true},
    "left_ankle":  {"sequence": 46667, "ax": 544, "ay": -272, "az": 16960, "gx": -6, "gy": 14, "gz": -18, "age_us": 4900, "valid": true},
    "right_ankle": {"sequence": 46524, "ax": 560, "ay": -256, "az": 16840, "gx": 4, "gy": -8, "gz": 12, "age_us": 7200, "valid": true}
  },
  "uwb_block_index": 1919,
  "uwb_age_us": 82000,
  "uwb_5link": {
    "pelvis_head":        {"range_cm": 51, "valid_mask": 1, "raw_range_cm": 51, "raw_valid": true, "is_fresh": true},
    "pelvis_left_wrist":  {"range_cm": 55, "valid_mask": 1, "raw_range_cm": 55, "raw_valid": true, "is_fresh": true},
    "pelvis_right_wrist": {"range_cm": -1, "valid_mask": 0, "raw_range_cm": -1, "raw_valid": false, "is_fresh": true},
    "pelvis_left_ankle":  {"range_cm": 48, "valid_mask": 1, "raw_range_cm": 48, "raw_valid": true, "is_fresh": true},
    "pelvis_right_ankle": {"range_cm": 54, "valid_mask": 1, "raw_range_cm": 54, "raw_valid": true, "is_fresh": true}
  }
}
```

---

## 8. 150 ms Prediction Feasibility Analysis

```text
Current Time t_k                                  Target Prediction Horizon (t_k + 150 ms)
       |                                                             |
       v                                                             v
=======|=============================================================|======> Timeline
       [--- Frame 0 ---][--- Frame 1 ---] ... [--- Frame 8 ---][--- Frame 9 ---]
       |<----------------------- 9 IMU Frames ---------------------->|
       |
       |<- (UWB Range updated every 200 ms: at most 0 or 1 new UWB arrival) ->|
```

### Physical Feasibility Characteristics

1. **IMU Observation Horizon**:
   At a 60 Hz sampling rate, exactly **9 IMU frames** elapse across a 150 ms window:
   $$\frac{150 \text{ ms}}{16.67 \text{ ms/frame}} = 9.0 \text{ frames}$$
   This provides dense inertial integration data to track angular velocity and linear acceleration trends.
2. **UWB Update Dynamics**:
   At 5.0 Hz (200 ms block interval), the UWB update period exceeds the 150 ms prediction horizon:
   $$\text{UWB Period } (200 \text{ ms}) > \text{Horizon } (150 \text{ ms})$$
   Consequently, in approximately **75% of prediction steps**, no new UWB measurement will arrive before the prediction target. The model must project future joint poses using the most recent valid UWB anchor coupled with incoming 60 Hz IMU dynamics.
3. **Robustness to UWB Degradation**:
   - *Partial Link Dropout* (e.g. wrist shadowing): The model retains spatial anchors from head and ankle links while integrating wrist IMU orientation.
   - *Total Coordinator Outage* (10-second resets): UWB validity masks drop to zero across all links. The architecture must smoothly transition to pure IMU dead-reckoning without generating unbounded numerical errors.
4. **Evaluation Protocol**:
   Prediction error (MPJPE / joint error) must be evaluated separately across three test regimes:
   - *Regime A (Full UWB)*: All 5 links fresh and valid.
   - *Regime B (Partial UWB)*: 1 to 4 links dropped or invalid.
   - *Regime C (UWB Outage)*: All links masked to zero (simulating coordinator reboot or severe RF blockage).

---

## 9. Verification Artifacts and Reproducible Commands

### Implemented Tooling and Automated Tests

1. **Preparation Pipeline**:
   [`hardware/tools/prepare_step28_dataset.py`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/hardware/tools/prepare_step28_dataset.py)
   - Decodes USB protocol v1 captures.
   - Computes sequence integrity, loss, and duplicate metrics.
   - Outputs benchmark statistics JSON and causal 60 Hz aligned JSONL streams.
2. **Automated Verification Suite**:
   [`hardware/tools/test_step28_pipeline.py`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/hardware/tools/test_step28_pipeline.py)
   - `test_reproduce_step19_benchmark_statistics()`: Validates exact reproduction of all Step 19 ground truth counts and validity ratios against `gateway_raw_2026-10-12_101320.bin`.
   - `test_causal_no_future_leakage()`: Validates that future samples are never leaked into prior grid frames and verifies strict freshness mask enforcement.

### Reproducible Execution Commands

To execute the dataset preparation and generate benchmark outputs:

```powershell
# Run the Step 28 dataset preparation pipeline
C:\ncs\comp971_dissertation\.venv\Scripts\python.exe hardware\tools\prepare_step28_dataset.py `
    --capture hardware\outputs\records\acquisition\gateway_raw_2026-10-12_101320.bin `
    --output_dir hardware\outputs\records\validation_step28

# Execute the automated verification test suite
C:\ncs\comp971_dissertation\.venv\Scripts\python.exe -c "from hardware.tools.test_step28_pipeline import test_reproduce_step19_benchmark_statistics, test_causal_no_future_leakage; test_reproduce_step19_benchmark_statistics(); print('Benchmark test passed'); import tempfile, pathlib; tmp = pathlib.Path(tempfile.mkdtemp()); test_causal_no_future_leakage(tmp); print('Causality test passed')"
```

---

## 10. Open Issues and Next Action for the Dissertation

### Open Issues

1. **Model Topology Configuration Mismatch**:
   The current model configuration in [`workspace/configs/topology/smplh_wearable_topology_v0.json`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/workspace/configs/topology/smplh_wearable_topology_v0.json) requires 15 pairwise UWB channels. Training directly on the hardware dataset requires creating a derived 5-link topology config (`smplh_wearable_topology_5link_star.json`).
2. **Ground Truth SMPL Pose Labels**:
   The raw gateway binary file contains IMU accelerations, gyro rates, and UWB ranges, but **does not contain ground truth 3D body pose joint angles (SMPL poses)**. For quantitative training of a 150 ms-ahead neural predictor, synthetic datasets (such as AMASS) with simulated sensor noise remain required for pretraining, while the live hardware dataset serves as the real-world validation benchmark.

### Recommended Next Action

- **Step 29 (Model Input Adapter and Training Track Alignment)**:
  1. Define `smplh_wearable_topology_5link_star.json` mapping the 5 measured star links to the 6 wearable joints.
  2. Implement an input adapter in `workspace/models/` that ingests the causal 60 Hz aligned schema generated in Step 28.
  3. Pre-train the 150 ms predictive model on AMASS synthetic sequences subsampled to 5-link star topology, and benchmark its inference stability on the real-world Step 19 hardware capture.

---

**EXECUTION HALTED**: All deliverables completed offline in strict read-only mode without code edits to embedded firmware, compilation, or hardware contact.

