# STEP 28B: FINAL 60 HZ GRID AND SENSOR TIMING VALIDATION REPORT

**Project:** AUT COMP971 — 150 ms Ahead Human Motion Prediction  
**Mode:** Offline dataset corrections and testing only  
**Firmware Baseline:** Step 17 unified release (`FB584C95...` preserved)  
**Capture Dataset:** Step 19 raw gateway capture (`2A9BD73C...` preserved)  
**Execution Status:** Quiescent, untouched, strictly offline  

---

## 1. Exact Defects Identified and Corrected

1. **Cumulative Floating-Point Rounding Drift in Time Grid**:
   In Step 28/28A, the 60 Hz grid was advanced by iteratively adding `16667` microseconds. Over 108,000 frames, integer truncation of $16,666.\bar{6} \text{ us}$ accumulated over **36.0 milliseconds of phase drift**.
   - *Correction*: Replaced with exact integer arithmetic:
     $$t_k = t_0 + \left\lfloor \frac{k \times 1,000,000 + 30}{60} \right\rfloor$$
     This guarantees zero accumulated phase drift ($0.0 \text{ ppm}$) across the entire 30-minute horizon.
2. **Ambiguity in Timestamp Semantics**:
   The field previously labeled `transport_age_us` was ambiguous, conflating physical sample age with BLE transmission latency.
   - *Correction*: The schema now explicitly decouples:
     - `source_local_timestamp_us`: On-node microsecond clock derived from FreeRTOS RTC1.
     - `gateway_receive_timestamp_us`: Gateway packet arrival timestamp.
     - `availability_age_us`: Model transport availability age ($t_k - t_{\text{gateway}}$).
     - `sync_status`: Explicit flag (`"async_ble_transport"`) indicating cross-node sub-frame synchronization uncertainty.
3. **Reconciliation of Session Frame Count (107,999 vs 108,000)**:
   A nominal 1,800-second session at 60 Hz contains 108,000 frames. The physical Star Ranges stream spanned from $t_0 = 870,408,477 \text{ us}$ to $t_{\text{last}} = 2,670,378,540 \text{ us}$ ($1,799.970063 \text{ s}$). The exact integer grid terminates at frame $k = 107,998$ ($t_k = 2,670,375,144 \text{ us}$, which is $3,396 \text{ us}$ prior to the final block). Frame $k = 107,999$ falls at $2,670,391,810 \text{ us}$ ($13,270 \text{ us}$ after acquisition ceased). Exactly **107,999 frames** are physically supported without fabricating data outside the capture horizon.
4. **Physical Clock Characteristics Verified**:
   - Firmware uses FreeRTOS tick scheduling with prescaler 32: RTC frequency $= \frac{32,768 \text{ Hz}}{33} \approx 992.97 \text{ Hz}$.
   - Sampling period $= \frac{32,768}{33 \times 60} = \frac{32,768}{1,980} \text{ ticks/sample}$.
   - The reported $< 10 \text{ us}$ intra-node variance reflects integer RTC timestamp formula consistency, not sub-microsecond hardware crystal resolution ($1 \text{ RTC tick} \approx 1,007 \text{ us}$).

---

## 2. Corrected Offline Preprocessing Implementation

The pipeline was completely updated in [`hardware/tools/prepare_step28b_dataset.py`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/hardware/tools/prepare_step28b_dataset.py):
- **Exact Grid Formula**: Enforces exact integer arithmetic without floating-point accumulation.
- **Cache Flushes**: Explicitly invalidates all 5 UWB ranging caches across coordinator software reboots (`NVIC_SystemReset()`).
- **Late Packet Rejection**: Prevents packets from older boot epochs from reactivating stale UWB or IMU states.
- **Independent Channels**: Tracks `attempt_availability_age_us` separately from `last_valid_availability_age_us`.

---

## 3. Expanded Automated Test Suite Execution Results

Automated test suite [`hardware/tools/test_step28b_pipeline.py`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/hardware/tools/test_step28b_pipeline.py) executed to completion:

```text
Starting Step 28B Test Suite execution...
  [PASS] test_exact_60hz_grid_arithmetic
         - Verified 60 frames = exactly 1,000,000 us (0 ppm drift).
         - Verified 108,000 frames = exactly 1,800,000,000 us (0 ppm drift).
         - Verified 150 ms offset = exactly 9 frames (150,000 us).
  [PASS] test_epoch_segmentation_and_loss_accounting
         - Verified Pelvis 3 boot epochs, 0 within-epoch duplicates, 381 missing.
         - Verified Node 2 1 boot epoch, exactly 6 transport duplicates, 420 missing.
         - Verified 8,871 blocks, 35,389 valid measurements, 79.79% validity.
  [PASS] test_causal_no_future_leakage_and_cross_epoch_invalidation
         - Verified zero future data leakage across temporal boundaries.
         - Verified coordinator reset completely invalidates cached UWB measurements.
  [PASS] test_full_session_row_count_and_span
         - Verified exactly 107,999 frames generated.
         - Verified boundary timestamps strictly bounded by raw capture horizon.

=== ALL 4 TEST SUITES PASSED (4/4 passed, 0 failed, Exit Code 0) ===
```

---

## 4. Corrected Dataset Manifest

| Artifact Name | Path | Size | SHA-256 Checksum |
| :--- | :--- | :--- | :--- |
| **Raw Input Capture** | [`gateway_raw_2026-10-12_101320.bin`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/hardware/outputs/records/acquisition/gateway_raw_2026-10-12_101320.bin) | 30,240,440 B | `2A9BD73C324DB0CFE237BA4A1EDE147059AFA1FE3BCAD06CEA211615300A9EB5` |
| **Epoch & Loss Statistics** | [`step28b_epoch_and_loss_statistics.json`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/hardware/outputs/records/validation_step28b/step28b_epoch_and_loss_statistics.json) | 5,566 B | `A4736DFB69A5E4E300B4AE318CE5C727DDA6680481F4CD38EE5CDEE308FCE508` |
| **Full Causal 60 Hz Dataset** | [`step28b_causal_60hz_aligned_mode_b.jsonl`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/hardware/outputs/records/validation_step28b/step28b_causal_60hz_aligned_mode_b.jsonl) | 313,520,635 B | `5B88996ADBE1883FA9ADA1A4BCBF0C3C12F82FE4ED9CFD876DD5D9A44842D5AD` |

Total Validated Frames: **107,999 frames** ($t = 0.000000 \text{ s}$ to $1,799.966667 \text{ s}$).

---

## 5. Dataset Schema and Timestamp Contract

Sample JSON structure from [`step28b_causal_60hz_aligned_mode_b.jsonl`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29)/hardware/outputs/records/validation_step28b/step28b_causal_60hz_aligned_mode_b.jsonl):

```json
{
  "schema_version": "v1.1-step28b",
  "timing_mode": "mode_b",
  "frame_index": 120,
  "t_rel_seconds": 2.0,
  "grid_timestamp_us": 872408477,
  "pelvis_boot_epoch": 0,
  "imu": {
    "head": {
      "sequence": 47141,
      "source_local_timestamp_us": 785602966,
      "gateway_receive_timestamp_us": 872404277,
      "availability_age_us": 4200,
      "ax": 612, "ay": -288, "az": 16320,
      "gx": -12, "gy": 44, "gz": -8,
      "valid": true,
      "sync_status": "async_ble_transport"
    },
    ...
  },
  "uwb_latest_attempt": {
    "block_index": 1919,
    "attempt_gateway_receive_us": 872326477,
    "attempt_availability_age_us": 82000,
    "is_recent": true
  },
  "uwb_5link": {
    "pelvis_head": {
      "range_cm": 51,
      "valid_mask": 1,
      "source_block_index": 1919,
      "last_valid_gateway_receive_us": 872326477,
      "last_valid_availability_age_us": 82000,
      "is_fresh": true
    },
    ...
  }
}
```

---

## 6. Step 29 Compatibility Gate

```text
+-----------------------------------------------------------------------------------------+
| Feature Contract Dimension     | Hardware Dataset (Step 28B)  | Synthetic Baseline      |
+--------------------------------+------------------------------+-------------------------+
| IMU Node Count                 | 6 nodes                      | 6 nodes                 |
| IMU Raw Channels per Node      | 6 channels (ax,ay,az,gx,gy,gz| 12 channels (acc+rotmat)|
| Total IMU Input Dimension      | 36 channels                  | 72 channels             |
| Measured Physical UWB Links    | 5 star links                 | 15 pairwise mesh links  |
| UWB Validity & Age Channels    | 5 masks + 5 age values       | 15 binary masks         |
| Total Physical Multimodal Dims | 51 dimensions                | 102 dimensions          |
+--------------------------------+------------------------------+-------------------------+
```

### **RECOMMENDATION: GO FOR STEP 29**

The dataset preparation pipeline is mathematically robust, causal, and verified against all established hardware evidence. In Step 29, the model adapter can safely map this 51-dimensional hardware schema to the neural predictive backbone without modifying the synthetic AMASS pre-training track.

---

**EXECUTION HALTED**: All deliverables completed offline in strict read-only mode without code edits to embedded firmware, compilation, or hardware contact.

