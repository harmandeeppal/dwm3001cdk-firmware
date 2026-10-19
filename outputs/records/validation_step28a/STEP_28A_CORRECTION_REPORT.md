# STEP 28A: SENSOR TIMING, EPOCH INTEGRITY AND CAUSAL DATASET CORRECTION REPORT

**Project:** AUT COMP971 — 150 ms Ahead Human Motion Prediction  
**Mode:** Offline software corrections and tests only  
**Firmware Baseline:** Step 17 unified release (`FB584C95...` preserved)  
**Capture Dataset:** Step 19 raw gateway capture (`2A9BD73C...` preserved)  
**Execution Status:** Quiescent, untouched, strictly offline  

---

## 1. Exact Step 28 Defects Identified

1. **False Pelvis Duplicate Classification**:
   Step 28 reported 36,302 duplicate samples for Node 1 (Pelvis) by erroneously evaluating global sequence uniqueness across the entire 30-minute session. Because the Pelvis experienced two software resets (`NVIC_SystemReset()`), its 32-bit local sequence counter restarted from low sequence numbers (`seq = 372` in Boot 8, `seq = 294` in Boot 9). Treating sequence reuse across distinct boots as physical duplicates corrupted the loss accounting.
2. **Conflation of Physical Acquisition Time and Gateway Reception Time**:
   Step 28 treated `gateway_receive_timestamp_us` as if it were the physical sensor sampling instant. In reality, BLE notifications batch 6 IMU samples together, introducing 16.6 to 100 ms of arrival transport jitter.
3. **UWB Cache Persistence Across Coordinator Resets**:
   In Step 28, when the coordinator reset, stale UWB distance estimates remained in memory and could theoretically be sampled if within the 350 ms timeout. Coordinator reboot boundaries must explicitly invalidate all cached UWB ranging links.
4. **150 ms Horizon UWB Arrival Probability Inversion**:
   Step 28 incorrectly stated that a 150 ms window contains a 25% chance of a new UWB update. In reality, for a uniformly phased 200 ms block interval, an ideal 150 ms future window covers $\frac{150}{200} = 75\%$ of the phase interval. However, a causal model evaluated at $t_k$ must strictly never consume that future update.
5. **Absence of Separate Alignment Modes**:
   Step 28 lacked an explicit decoupling between offline signal analysis (Mode A) and causal streaming simulation (Mode B).

---

## 2. Corrected Six-Node IMU Accounting

Segmentation by verified boot boundaries (detected by local microsecond timestamp discontinuities $> 500 \text{ ms}$) yields clean per-epoch statistics without false sequence-reuse duplicates:

| Node Index | Anatomical Placement | Total Received Samples | Boot Epoch Count | Genuine MCU Resets | Within-Epoch Transport Duplicates | Sequence Jumps | Net Missing Samples |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Node 0** | Head | **105,882** | 1 | 0 | 0 | 106 | 2,124 |
| **Node 1** | Pelvis (Coordinator)| **106,868** | **3** | **2** | **0** (Corrected from 36,302) | 2 | **381** |
| **Node 2** | Left Wrist | **107,586** | 1 | 0 | **6** (Confirmed repeated packets)| 4 | **420** |
| **Node 3** | Right Wrist | **107,080** | 1 | 0 | 0 | 28 | 926 |
| **Node 4** | Left Ankle | **107,602** | 1 | 0 | 0 | 3 | 404 |
| **Node 5** | Right Ankle | **105,906** | 1 | 0 | 0 | 142 | 2,070 |
| **Total** | **All 6 Nodes** | **640,924** | 8 | 2 | 6 | 285 | 6,325 |

### Pelvis Multi-Boot Epoch Breakdown

- **Epoch 0 (Boot 7)**: Indices 0 to 17,776 (17,777 samples, `seq = 22,903 .. 40,679`, span = 17,777). Duplicates = 0, Jumps = 0, Missing = 0.
- **Epoch 1 (Boot 8)**: Indices 17,777 to 88,108 (70,332 samples, `seq = 372 .. 70,859`, span = 70,488). Duplicates = 0, Jumps = 1, Missing = 156.
- **Epoch 2 (Boot 9)**: Indices 88,109 to 106,867 (18,759 samples, `seq = 294 .. 19,277`, span = 18,984). Duplicates = 0, Jumps = 1, Missing = 225.

---

## 3. Timestamp Synchronization and Uncertainty Assessment

1. **Source Sensor Acquisition Time (`local_timestamp_us`)**:
   Derived from nRF52833 RTC1 (32.768 kHz LFCLK crystal). Operates at **60.0000 Hz** nominal sampling rate ($16,666.67 \text{ us/sample}$). Preserves intra-node sample spacing with $< 2 \text{ us}$ relative variance.
2. **Transport Latency & Batching**:
   BLE notifications transmit IMU samples in batches of 6. This introduces a transport delay between physical measurement and gateway arrival of **16.6 to 100 milliseconds**.
3. **Clock Drift Relative to Gateway**:
   - Node 0: -30.38 ppm
   - Node 2: -23.69 ppm
   - Node 3: -47.27 ppm
   - Node 4: -29.77 ppm
   - Node 5: -85.71 ppm
4. **Synchronization Uncertainty**:
   - Intra-node jitter: $< 10 \text{ us}$.
   - Cross-node arrival jitter: $16.6 \text{ to } 100 \text{ ms}$.
   - Sub-frame (< 16 ms) cross-node phase synchronization **cannot be established** from the asynchronous BLE transport capture. This is explicitly documented as a physical dataset constraint.

---

## 4. Dual Timing Modes Defined

### Mode A — Offline Acquisition-Time Alignment

- Maps samples by estimated physical acquisition instant ($t_{\text{acq}}$) using source local timestamps and linear drift compensation.
- Appropriate for offline kinematic analysis, signal conditioning, and body-model trajectory smoothing.
- **Limitation**: Marked *unsupported* across Pelvis MCU reboot boundaries where local clock continuity is lost.

### Mode B — Causal Gateway-Availability Streaming Alignment (Default for Model Inference)

- At each simulated inference instant $t_k$, the model has access only to packets whose gateway receive timestamp satisfies:
  $$t_{\text{receive}} \le t_k$$
- Exposes native transport arrival age, sequence metadata, and validity flags.
- Prevents data snooping and strictly simulates real-time wearable streaming constraints.

---

## 5. Corrected Causal 60 Hz Feature Schema

Each frame in [`step28a_causal_60hz_aligned_mode_b.jsonl`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/hardware/outputs/records/validation_step28a/step28a_causal_60hz_aligned_mode_b.jsonl) implements the following schema:

```json
{
  "schema_version": "v1.0-step28a",
  "timing_mode": "mode_b",
  "frame_index": 120,
  "t_rel_seconds": 2.0000,
  "grid_timestamp_us": 872408477,
  "pelvis_boot_epoch": 0,
  "imu": {
    "head":        {"sequence": 47141, "local_timestamp_us": 785602966, "ax": 612, "ay": -288, "az": 16320, "gx": -12, "gy": 44, "gz": -8, "transport_age_us": 4200, "valid": true},
    "pelvis":      {"sequence": 23027, "local_timestamp_us": 383719604, "ax": 196, "ay": -744, "az": 16408, "gx": -2, "gy": 18, "gz": 4, "transport_age_us": 6100, "valid": true},
    "left_wrist":  {"sequence": 53634, "local_timestamp_us": 893902343, "ax": 240, "ay": -16, "az": 16712, "gx": 8, "gy": -12, "gz": -4, "transport_age_us": 3800, "valid": true},
    "right_wrist": {"sequence": 46877, "local_timestamp_us": 781202026, "ax": 598, "ay": -312, "az": 16296, "gx": 14, "gy": 22, "gz": 10, "transport_age_us": 5100, "valid": true},
    "left_ankle":  {"sequence": 46667, "local_timestamp_us": 777702423, "ax": 544, "ay": -272, "az": 16960, "gx": -6, "gy": 14, "gz": -18, "transport_age_us": 4900, "valid": true},
    "right_ankle": {"sequence": 46524, "local_timestamp_us": 775403259, "ax": 560, "ay": -256, "az": 16840, "gx": 4, "gy": -8, "gz": 12, "transport_age_us": 7200, "valid": true}
  },
  "uwb_latest_attempt": {
    "block_index": 1919,
    "attempt_age_us": 82000,
    "is_recent": true
  },
  "uwb_5link": {
    "pelvis_head":        {"range_cm": 51, "valid_mask": 1, "source_block_index": 1919, "last_valid_age_us": 82000, "is_fresh": true},
    "pelvis_left_wrist":  {"range_cm": 55, "valid_mask": 1, "source_block_index": 1919, "last_valid_age_us": 82000, "is_fresh": true},
    "pelvis_right_wrist": {"range_cm": -1, "valid_mask": 0, "source_block_index": null, "last_valid_age_us": null, "is_fresh": false},
    "pelvis_left_ankle":  {"range_cm": 48, "valid_mask": 1, "source_block_index": 1919, "last_valid_age_us": 82000, "is_fresh": true},
    "pelvis_right_ankle": {"range_cm": 54, "valid_mask": 1, "source_block_index": 1919, "last_valid_age_us": 82000, "is_fresh": true}
  }
}
```

---

## 6. Complete Versioned Dataset Manifest

| Artifact Name | Path | Size | SHA-256 Checksum |
| :--- | :--- | :--- | :--- |
| **Raw Input Capture** | [`gateway_raw_2026-10-12_101320.bin`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/hardware/outputs/records/acquisition/gateway_raw_2026-10-12_101320.bin) | 30,240,440 B | `2A9BD73C324DB0CFE237BA4A1EDE147059AFA1FE3BCAD06CEA211615300A9EB5` |
| **Epoch & Loss Statistics** | [`step28a_epoch_and_loss_statistics.json`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/hardware/outputs/records/validation_step28a/step28a_epoch_and_loss_statistics.json) | 5,566 B | `A4736DFB69A5E4E300B4AE318CE5C727DDA6680481F4CD38EE5CDEE308FCE508` |
| **Full Causal 60 Hz Dataset** | [`step28a_causal_60hz_aligned_mode_b.jsonl`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/hardware/outputs/records/validation_step28a/step28a_causal_60hz_aligned_mode_b.jsonl) | 216,673,835 B | `AF0DA2212268336722B1EBA08ED09D8360A05CD9C3C48F6FF1F7A514FD396713` |

Total Frames in Full Aligned Dataset: **107,997 frames** spanning 1,799.970 seconds at 60.0 Hz.

---

## 7. Automated Test Suite Execution Results

Automated test suite [`hardware/tools/test_step28a_pipeline.py`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/hardware/tools/test_step28a_pipeline.py) executed to completion:

```text
Executing Step 28A Test Suite...
  [PASS] test_epoch_segmentation_and_loss_accounting
         - Verified Pelvis has 3 boot epochs and 0 within-epoch duplicates.
         - Verified Node 2 has 1 boot epoch, 6 confirmed transport repeats, and 420 missing.
         - Verified 8,871 blocks, 35,389 valid measurements, and 79.79% validity.
  [PASS] test_causal_no_future_leakage_and_cross_epoch_invalidation
         - Verified zero future data leakage across temporal boundaries.
         - Verified coordinator reset completely invalidates cached UWB measurements.
  [PASS] test_150ms_prediction_horizon_dynamics
         - Verified 9 IMU sample intervals in a 150 ms window.
         - Verified 75% theoretical UWB phase coverage.

=== ALL STEP 28A TESTS PASSED (3/3 passed, 0 failed, Exit Code 0) ===
```

---

## 8. Clear GO / NO-GO Recommendation for Step 29

### **RECOMMENDATION: GO FOR STEP 29**

- **Justification**:
  1. The Step 19 hardware dataset has been fully validated, segmented into verified boot epochs, and aligned into a clean 60 Hz causal streaming representation (`107,997 frames`).
  2. The false duplicate anomaly on Pelvis has been resolved (0 within-epoch duplicates; 381 net missing packets).
  3. Causal guarantees have been verified by unit tests: future data is never leaked, stale UWB values are masked, and coordinator reboots flush spatial caches.
  4. The 5-link star topology is explicitly defined and decoupled from synthetic 15-link mesh assumptions.

### **Next Action in Step 29**:
Implement the hardware-compatible model input adapter in `workspace/models/` mapping the 5-link star schema to the neural network backbone without altering the existing 15-link synthetic research configuration.

---

**EXECUTION HALTED**: All deliverables completed offline in strict read-only mode without code edits to embedded firmware, compilation, or hardware contact.

