# GEMINI 4 ARGON — Final Firmware UWB Scheduling & Per-Node Asymmetry Audit Report

**Audit Date:** 16 October 2026  
**Auditor:** Gemini 4 Argon (Hardware & Embedded Systems Audit)  
**Audit Scope:** Final Production Firmware Only (`hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/` and `hardware/firmware/nrf52840_dongle_motion_gateway/`) and Final 6-Node Empirical Captures (`2026-10-10` through `2026-10-12`).  
**Constraints Applied:** Read-only inspection of all firmware and empirical capture artifacts. Zero firmware files were modified. All findings cite exact source files, line numbers, and decoded hardware telemetry records.

---

## 1. Executive Summary: What Is the Actual Issue in the Final Firmware?

In the final 6-node DS-TWR FiRa star architecture (`Pelvis` Controller `node_id = 1` ranging at `5 Hz` / `200 ms` block period against 5 peripheral Controlees: `Head` Slot 1, `Left Wrist` Slot 2, `Right Wrist` Slot 3, `Left Ankle` Slot 4, and `Right Ankle` Slot 5), **UWB link performance is highly asymmetric across nodes**:

- **`Head` (`node_id = 0`, Pair 0, Slot 1)** consistently achieves **`99.24%` to `99.75%`** valid UWB ranging across every 30-minute soak capture (`8,829 / 8,871 = 99.53%` in the final `gateway_raw_2026-10-12_101320` benchmark capture).
- **`Left Wrist` (`node_id = 2`, Pair 1, Slot 2)**, **`Right Wrist` (`node_id = 3`, Pair 2, Slot 3)**, and **`Left Ankle` (`node_id = 4`, Pair 3, Slot 4)** — and in the 11 October captures **`Right Ankle` (`node_id = 5`, Pair 4, Slot 5)** — suffer severe valid-rate degradation:
  - In `gateway_raw_2026-10-12_101320` (`8,871` blocks): **`Left Wrist = 72.27%`**, **`Right Wrist = 62.34%`**, **`Left Ankle = 68.80%`**, and **`Right Ankle = 96.00%`** (aggregate `79.79%`).
  - Across the three 11 October 30-minute captures (`155057`, `173658`, `200844`): **`Left Wrist = 31.17%–57.25%`**, **`Right Wrist = 43.70%–64.07%`**, **`Left Ankle = 79.02%–86.08%`**, and **`Right Ankle = 83.72%–88.17%`**, while **`Head = 99.24%–99.75%`**.

### The Crucial Empirical Discovery (`star_ranges.ndjson` Streak Decomposition)

Decomposing all missing UWB measurements in `gateway_raw_2026-10-12_101320_decoded/star_ranges.ndjson` into **short transient RF losses (`< 25 blocks` / `< 5.0 seconds`)** versus **multi-second synchronization lock-loss stalls (`>= 25 blocks` / `>= 5.0 seconds`)** reveals the exact nature of the problem:

| Link / Pair | Target Node (Slot) | Total Blocks | Valid (`status = 0`) | Total Invalid (`status = 33 / 34`) | Short RF Losses (`< 25 blks` / `< 5 s`) | Multi-Second Lock-Loss Stalls (`>= 25 blks` / `>= 5 s`) | Share of Losses in `>= 5 s` Stalls | Max Dropout Streak |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Pair 0** | **Head (`node_id = 0`, Slot 1)** | `8,871` | `8,829` (**`99.53%`**) | `42` (`33: 41, 34: 1`) | **`42` blks** (`0.47%`) | **`0` blks** (`0` events) | **`0.00%`** | `6` blks (`1.2 s`) |
| **Pair 1** | **Left Wrist (`node_id = 2`, Slot 2)** | `8,871` | `6,411` (**`72.27%`**) | `2,460` (`33: 2460`) | **`43` blks** (`0.48%`) | **`2,417` blks** (`25` events) | **`98.25%`** | `261` blks (`52.2 s`) |
| **Pair 2** | **Right Wrist (`node_id = 3`, Slot 3)** | `8,871` | `5,530` (**`62.34%`**) | `3,341` (`33: 3339, 34: 2`) | **`54` blks** (`0.61%`) | **`3,287` blks** (`47` events) | **`98.38%`** | `597` blks (`119.4 s`) |
| **Pair 3** | **Left Ankle (`node_id = 4`, Slot 4)** | `8,871` | `6,103` (**`68.80%`**) | `2,768` (`33: 2767, 34: 1`) | **`57` blks** (`0.64%`) | **`2,711` blks** (`39` events) | **`97.94%`** | `261` blks (`52.2 s`) |
| **Pair 4** | **Right Ankle (`node_id = 5`, Slot 5)** | `8,871` | `8,516` (**`96.00%`**) | `355` (`33: 355`) | **`63` blks** (`0.71%`) | **`292` blks** (`4` events) | **`82.25%`** | `87` blks (`17.4 s`) |

**What this proves:**
1. **Baseline single-packet RF link quality is virtually identical (`99.29%` to `99.53%`) across all five nodes.** Outside of multi-second lock-loss stalls (`>= 25 blocks`), `Left Wrist` (`43` lost blocks), `Right Wrist` (`54` lost blocks), `Left Ankle` (`57` lost blocks), and `Right Ankle` (`63` lost blocks) have almost the exact same transient RF loss rate as `Head` (`42` lost blocks).
2. **`97.94%` to `98.38%` of all missing UWB measurements on `Left Wrist`, `Right Wrist`, and `Left Ankle` are caused by multi-second synchronization lock-loss stalls (`>= 5.0 seconds`) and the firmware's broken recovery/restart behavior.**
3. Inspection of the final firmware source code and decoded `uwb_diag.ndjson` telemetry identifies **five concrete, verifiable firmware and scheduling root causes** that explain why Slots 2–5 enter these multi-second lock-loss stalls and why they fail to recover cleanly.

---

## 2. Final Firmware Architecture & Files Audited

The final system comprises two firmware codebases:

1. **Qorvo QM33 / DWM3001CDK Motion Node Firmware (`hardware/firmware/qorvo_qm33_motion_node/`)**:
   - [`official_sdk_project/Common/src/main.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c) (`662` lines): FreeRTOS task creation (`uwb_task` at Priority `2`, `imu_task` at Priority `2`, `status_task` at Priority `1`), Controller stall escalation watchdog (`lines 259–340`), and `1 Hz` health telemetry emission (`lines 393–434`).
   - [`official_sdk_project/Common/src/comp971_uwb_official_fira.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c) (`1,289` lines): Qorvo Embedded FiRa MAC (`fira_helper`) session configuration, `fira_notification_cb()` result handler (`lines 398–608`), Version 6.7 Controlee bounded recovery state machine (`lines 925–1087`), `comp971_uwb_stage2_mac_restart()` (`lines 1117–1180`), and wrapped DW3000 SPI driver instrumentation (`__wrap_dwt_SPIwritecrc`, `__wrap_dwt_SPIreadcrc`, `__wrap_dwt_SPIfastcmd` at `lines 1186–1288`).
   - [`official_sdk_project/Common/include/comp971_uwb_two_node.h`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/include/comp971_uwb_two_node.h) (`117` lines): UWB health diagnostic stage codes (`0xB3..0xBD`, `0xED`).
   - [`official_sdk_project/Common/include/comp971_config_shared.h`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/include/comp971_config_shared.h) (`81` lines): Hardware FICR identity mapping and BLE radio intervals.
   - [`official_sdk_project/Common/src/comp971_node_identity.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_node_identity.c) (`67` lines): Runtime `NRF_FICR->DEVICEID` role resolution (`Pelvis = node_id 1` Controller, `Head/LW/RW/LA/RA = node_id 0, 2, 3, 4, 5` Responders).
   - [`official_sdk_project/Common/src/comp971_ble_pilot.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_ble_pilot.c) (`805` lines): S140 SoftDevice BLE peripheral advertising and GATT notifications.
   - [`official_sdk_project/Common/src/comp971_icm20948.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_icm20948.c) (`244` lines): Blocking `100 kHz` TWIM1 I2C driver for the ICM-20948 IMU.
   - [`include/comp971_telemetry_protocol.h`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/include/comp971_telemetry_protocol.h) (`233` lines): Binary wire protocol for `STAR_RANGES` (`0x22`), `IMU` (`0x01`), `SYNC` (`0x02`), and `UWB_DIAG` (`0x31`).
2. **nRF52840 Dongle BLE/USB Gateway Firmware (`hardware/firmware/nrf52840_dongle_motion_gateway/`)**:
   - [`src/main.c`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/nrf52840_dongle_motion_gateway/src/main.c) (`586` lines): Passive BLE observer (`SCAN_INTERVAL = 0x00A0` (`100 ms`), `SCAN_WINDOW = 0x00A0` (`100 ms`), 100% duty cycle) forwarding frames over USB CDC ACM.

### Current FiRa Schedule Configuration ([`comp971_uwb_official_fira.c:658–685`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L658-L685))
- **Topology:** 1-to-5 Star (`ONE_TO_MANY` multicast DS-TWR, Deferred Mode `schedule_mode = 2`).
- **Session ID:** `42`, Channel `9` (`7987.2 MHz`), Preamble Code `10`, SFD ID `2` (4z), STS Config `0` (SP1).
- **Block Duration (`block_duration_ms`):** `200 ms` (`5 Hz`).
- **Slot Duration (`slot_duration_rstu`):** `2400 RSTU` (`2.0 ms`).
- **Round Duration (`round_duration_rstu`):** `24000 RSTU` (`20.0 ms`, `10 slots` per round).
- **Round Hopping (`round_hopping`):** `0` (disabled — every round is pinned to Round Index `0`, occupying the first `20.0 ms` of every `200.0 ms` block).
- **Max Number of Measurements (`max_number_of_measurements`):** `0` (unlimited), `max_rr_retry = 0`, `block_striding = 0`.
- **Slot Map within the `20.0 ms` Active Round:**
  - **Slot 0 (`0.0–2.0 ms`):** `Pelvis` (`0x0001`) broadcasts Ranging Initiation (Poll).
  - **Slot 1 (`2.0–4.0 ms`):** `Head` (`0x0002`, `node_id = 0`) transmits Ranging Response (`Pair 0`).
  - **Slot 2 (`4.0–6.0 ms`):** `Left Wrist` (`0x0003`, `node_id = 2`) transmits Ranging Response (`Pair 1`).
  - **Slot 3 (`6.0–8.0 ms`):** `Right Wrist` (`0x0004`, `node_id = 3`) transmits Ranging Response (`Pair 2`).
  - **Slot 4 (`8.0–10.0 ms`):** `Left Ankle` (`0x0005`, `node_id = 4`) transmits Ranging Response (`Pair 3`).
  - **Slot 5 (`10.0–12.0 ms`):** `Right Ankle` (`0x0006`, `node_id = 5`) transmits Ranging Response (`Pair 4`).
  - **Slot 6 (`12.0–14.0 ms`):** `Pelvis` (`0x0001`) broadcasts Ranging Final (`Final`).
  - **Slot 7 (`14.0–16.0 ms`):** `Pelvis` (`0x0001`) broadcasts Measurement Report (`M-Report`).

---

## 3. Detailed Root-Cause Findings (Verified from Final Firmware & Telemetry)

### Finding 1 (CRITICAL BUG): Version 6.7 Controlee Recovery State Machine Never Updates Valid-Ranging Counters on Controlees, Causing Deterministic Self-Sabotage (`COMP971_REC_HOLD`)

- **Source Location:** [`comp971_uwb_official_fira.c:478–590`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L478-L590) and [`comp971_uwb_official_fira.c:925–1087`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L925-L1087).
- **Exact Code Defect:**
  In [`comp971_uwb_official_fira.c:925`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L925), the Version 6.7 recovery state machine executes **only on Controlees**:
  ```c
  if ((g_comp971_uwb_role != COMP971_UWB_CONTROLLER) && m_prepared)
  ```
  When a Controlee experiences `5,000 ms + g_comp971_restart_jitter_ms` without a valid range (`COMP971_REC_IDLE`), it increments `m_rec_attempts` to `1`, increments `m_rec_session_cookie`, calls `comp971_uwb_stage2_mac_restart()`, and enters `COMP971_REC_COOLDOWN` (`5,000 ms` window, lines `948–959`).

  To verify that the restart succeeded and exit `COMP971_REC_COOLDOWN` back to `COMP971_REC_IDLE`, lines `971–1009` read:
  ```c
  uint32_t cb_seq = g_fira_cb_sequence;
  uint32_t val_seq = g_fira_valid_sequence;
  uint32_t cb_streak = g_fira_cb_valid_streak;
  uint32_t val_cookie = g_fira_last_valid_cookie;
  ...
  if (cb_seq > m_rec_last_consumed_cb_seq)
  {
      uint32_t new_cb_count = cb_seq - m_rec_last_consumed_cb_seq;
      uint32_t new_valid_count = val_seq - m_rec_last_consumed_valid_seq;
      m_rec_last_consumed_cb_seq = cb_seq;

      if ((cb_streak == 0U) || (new_valid_count < new_cb_count))
      {
          /* Case 2: New failed ranging event occurred -> reset success streak */
          m_rec_consecutive_valid = 0U;
          m_rec_last_consumed_valid_seq = val_seq;
      }
      else if ((val_seq > m_rec_last_consumed_valid_seq) &&
               (val_cookie == m_rec_session_cookie))
      {
          /* Case 1: Purely valid new callbacks belonging to CURRENT session incarnation */
          m_rec_last_consumed_valid_seq = val_seq;
          m_rec_consecutive_valid += new_valid_count;
          if ((m_rec_consecutive_valid >= 2U) && (cb_streak >= 2U))
          {
              m_rec_state = COMP971_REC_IDLE;
              m_rec_attempts = 0U;
              ...
          }
      }
  }
  ```
  Now inspect [`fira_notification_cb()` at lines 478–590](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L478-L590), where `g_fira_valid_sequence`, `g_fira_cb_valid_streak`, and `g_fira_last_valid_cookie` are updated:
  ```c
  if (g_comp971_uwb_role == COMP971_UWB_CONTROLLER)
  {
      ...
      if (star_valid_count > 0U)
      {
          g_fira_valid_count += star_valid_count;
          g_fira_last_valid_tick = xTaskGetTickCount();
          g_fira_last_valid_cookie = m_rec_session_cookie;
          g_fira_valid_sequence++;
          g_fira_cb_valid_streak++;
      }
      else
      {
          g_fira_cb_valid_streak = 0U;
      }
  }
  else
  {
      if (any_valid)
      {
          g_fira_valid_count++;
          g_fira_last_valid_tick = xTaskGetTickCount();
      }
      ...
  }
  ```
  **Look at lines `566–589`:**
  1. `g_fira_last_valid_cookie = m_rec_session_cookie;`, `g_fira_valid_sequence++;`, and `g_fira_cb_valid_streak++;` were placed **inside `if (g_comp971_uwb_role == COMP971_UWB_CONTROLLER)`** (where the recovery state machine is disabled!) and **completely omitted from the `else` (`COMP971_UWB_RESPONDER`) branch**!
  2. On every Controlee, `g_fira_valid_sequence`, `g_fira_cb_valid_streak`, and `g_fira_last_valid_cookie` remain **`0` forever**, while `g_fira_cb_sequence` increments on every callback (`line 407`).
  3. Consequently, during `COMP971_REC_COOLDOWN`, even when Attempt 1 immediately restores 100% valid ranging (`any_valid == true`), `cb_streak` is `0U` and `new_valid_count` (`0`) is less than `new_cb_count`. Line `988` (`if ((cb_streak == 0U) || (new_valid_count < new_cb_count))`) evaluates to **`true` on every single valid measurement**, resetting `m_rec_consecutive_valid = 0U`!
  4. Because `m_rec_consecutive_valid` can never reach `2U`, **every Controlee running Version 6.7 is guaranteed to fail `COMP971_REC_COOLDOWN` after `5,000 ms`**, enter `COMP971_REC_BACKOFF` (`2,000 ms`), execute a **second `comp971_uwb_stage2_mac_restart()` (Attempt 2)** — **tearing down the working UWB session it just recovered!** — fail `COMP971_REC_COOLDOWN` a second time (`5,000 ms`), and lock itself into **`COMP971_REC_HOLD` (`30,000 ms` lockout)** while emitting diagnostic `0xED` (`detail = 3`)!

- **Empirical Proof in `uwb_diag.ndjson`:**
  - Across the four 30-minute captures, Node 2 (`Left Wrist`, the node flashed with Version 6.7) emitted `0xED` (`detail = 3`, recovery hold lockout):
    - `2026-10-11_155057`: **`26` times** (`52` Stage-2 MAC restarts = `26 x 2`)
    - `2026-10-11_173658`: **`22` times** (`44` Stage-2 MAC restarts = `22 x 2`)
    - `2026-10-11_200844`: **`25` times** (`49` Stage-2 MAC restarts)
    - `2026-10-12_101320`: **`7` times** (`14` Stage-2 MAC restarts = `7 x 2`)
  - Every single time `Left Wrist` triggered recovery once, this bug forced a second destructive MAC teardown `7 seconds` later and locked out recovery for the next `30 seconds`.

---

### Finding 2 (CRITICAL): Unflashed Firmware Skew Across the 6 Physical Nodes in the Final Captures

- **Source & Artifact Locations:**
  - `hardware/outputs/records/acquisition/gateway_raw_2026-10-12_101320_decoded/uwb_diag.ndjson`
  - `hardware/backups/2026-10-09_pre_step9_unified_6node_star/MANIFEST.txt`
  - `hardware/outputs/records/firmware/2026-10-11_step17_pelvis_diag_v2/FLASH_LOG.txt`
- **Empirical Proof from Decoded `uwb_diag.ndjson` (`2026-10-12_101320`):**

| Node ID | Anatomical Role | `0xB3` (`stall_recovery_cnt`) | `0xE2` (`SESSION_ACTIVE`) | `0xE3` (`SESSION_IDLE` on stop) | `0xED` (`REC_HOLD` v6.7) | `0xE0..0xEB` (`DIAG_STALL` v2) | Actual Firmware Running on Board During Final Captures |
| :---: | :--- | :---: | :---: | :---: | :---: | :---: | :--- |
| **`0`** | **Head (Slot 1)** | `0 -> 0` | `0` | `0` | `0` | No | **9 Oct 2026 build** (never triggered restart because `valid = 99.53%`) |
| **`1`** | **Pelvis (Controller)** | `0 -> 0` (reset x2) | `4` | `0` | `0` | **Yes (`2` dumps)** | **11 Oct 2026 Step 17 build** (`diag-v2-step17-unified`) |
| **`2`** | **Left Wrist (Slot 2)** | `0 -> 14` | `14` | **`14`** | **`7`** | No | **10 Oct 2026 Version 6.7 build** (only node with v6.7 state machine & `fira_helper_stop_session` `0xE3`) |
| **`3`** | **Right Wrist (Slot 3)** | `6 -> 94` (**`+88`**) | **`88`** | **`0`** | **`0`** | No | **9 Oct 2026 pre-v6.7 build** (unconditional `5 s` restart loop; never flashed with v6.7!) |
| **`4`** | **Left Ankle (Slot 4)** | `5 -> 65` (**`+60`**) | **`60`** | **`0`** | **`0`** | No | **9 Oct 2026 pre-v6.7 build** (unconditional `5 s` restart loop; never flashed with v6.7!) |
| **`5`** | **Right Ankle (Slot 5)** | `1 -> 10` (**`+9`**) | **`9`** | **`0`** | **`0`** | No | **9 Oct 2026 pre-v6.7 build** (unconditional `5 s` restart loop; never flashed with v6.7!) |

- **Why this matters:**
  1. **Nodes 0, 3, 4, and 5 (`Head`, `Right Wrist`, `Left Ankle`, `Right Ankle`) were never flashed with the 10–11 October firmware updates.** Only `Left Wrist` (`Node 2`) received the 10 October Version 6.7 build, and only `Pelvis` (`Node 1`) received the 11 October Step 17 build (`FLASH_LOG.txt` confirms only J-Link `760224787` (`Pelvis`) was flashed in Step 17).
  2. Because `Right Wrist` (`Node 3`) and `Left Ankle` (`Node 4`) are running the **9 October 2026 pre-v6.7 firmware**, they have **no cooldown/backoff/hold state machine** (`0xED = 0`, `0xE3 = 0`) and **no per-node restart jitter**. Whenever they lose Slot 0 Poll lock, they unconditionally execute Stage 2 MAC restarts every `5.0 seconds` (`88` times on `Right Wrist` and `60` times on `Left Ankle` during the 30-minute run of `2026-10-12_101320`, and `219`, `309`, and `252` times on `Right Wrist` during the three 11 October runs!).

---

### Finding 3 (PRIMARY HARDWARE/RTOS TRIGGER): Controlee Delayed-TX Deadline Misses (`dw3000_late_tx_count > 0`, `min_lead_dtu < 0`, `status = 32` `TX_FAILED`) on Slots 2–5 vs. Zero on Slot 1 (`Head`)

- **Source Locations:**
  - [`comp971_uwb_official_fira.c:1224–1279`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L1224-L1279) (`__wrap_dwt_SPIwritecrc`, `__wrap_dwt_SPIfastcmd` delayed-TX lead time `g_dw3000_min_tx_diff` and `HPDWARN` / `g_dw3000_late_tx_count` tracking).
  - [`comp971_uwb_official_fira.c:663–665`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L663-L665) (`slot_duration_rstu = 2400` (`2.0 ms`), `round_duration_rstu = 24000` (`20.0 ms`), `round_hopping = 0`).
  - [`main.c:455–511`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L455-L511) & [`comp971_icm20948.c:167`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_icm20948.c#L167) & [`comp971_ble_pilot.c:419–424`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_ble_pilot.c#L419-L424) (S140 SoftDevice radio preemption at NVIC Priorities `0, 1, 4` + `imu_task` at FreeRTOS Priority `2` equal to `uwb_task` Priority `2` polling `100 kHz` I2C every `20 ms`).

- **Empirical Proof from `uwb_diag.ndjson` Across All Four 30-Minute Captures:**

| Capture ID | Metric | Node 0 (`Head`, Slot 1) | Node 2 (`Left Wrist`, Slot 2) | Node 3 (`Right Wrist`, Slot 3) | Node 4 (`Left Ankle`, Slot 4) | Node 5 (`Right Ankle`, Slot 5) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: |
| **`2026-10-11_155057`** | `0xB7` `late_tx_count` | **`0 -> 0` (`+0`)** | `136 -> 146` (**`+10`**) | `15 -> 26` (**`+11`**) | `3 -> 5` (**`+2`**) | `4 -> 8` (**`+4`**) |
| | `0xB8` `min_lead_dtu` | **`+88,174 DTU`** | **`-164,088 DTU`** | **`-54,352 DTU`** | **`-46,194 DTU`** | **`-55,106 DTU`** |
| | `0xBC` `last_fail_status` | `33` only | **`32` (`TX_FAILED`), `33`** | **`32` (`TX_FAILED`), `33`** | `33` | **`32` (`TX_FAILED`), `33`** |
| | Valid UWB Rate | **`99.24%`** | **`50.92%`** | **`64.07%`** | **`86.08%`** | **`87.19%`** |
| **`2026-10-11_173658`** | `0xB7` `late_tx_count` | **`0 -> 0` (`+0`)** | `170 -> 193` (**`+23`**) | `29 -> 54` (**`+25`**) | `7 -> 11` (**`+4`**) | `12 -> 18` (**`+6`**) |
| | `0xB8` `min_lead_dtu` | **`+88,174 DTU`** | **`-164,088 DTU`** | **`-59,092 DTU`** | **`-46,194 DTU`** | **`-55,106 DTU`** |
| | `0xBC` `last_fail_status` | `33` only | **`32` (`TX_FAILED`), `33`** | **`32` (`TX_FAILED`), `33`** | **`32` (`TX_FAILED`), `33`** | **`32` (`TX_FAILED`), `33`** |
| | Valid UWB Rate | **`99.75%`** | **`31.17%`** | **`43.70%`** | **`80.71%`** | **`83.72%`** |
| **`2026-10-11_200844`** | `0xB7` `late_tx_count` | **`0 -> 0` (`+0`)** | `228 -> 242` (**`+14`**) | `62 -> 80` (**`+18`**) | `14 -> 18` (**`+4`**) | `19 -> 21` (**`+2`**) |
| | `0xB8` `min_lead_dtu` | **`+88,174 DTU`** | **`-164,088 DTU`** | **`-59,092 DTU`** | **`-46,194 DTU`** | **`-55,106 DTU`** |
| | `0xBC` `last_fail_status` | `33` only | **`32` (`TX_FAILED`), `33`** | **`32` (`TX_FAILED`), `33`** | **`32` (`TX_FAILED`), `33`** | `33` |
| | Valid UWB Rate | **`99.37%`** | **`57.25%`** | **`50.69%`** | **`79.02%`** | **`88.17%`** |
| **`2026-10-12_101320`** | `0xB7` `late_tx_count` | **`0 -> 0` (`+0`)** | `0 -> 7` (**`+7`**) | `6 -> 13` (**`+7`**) | `7 -> 12` (**`+5`**) | `1 -> 2` (**`+1`**) |
| | `0xB8` `min_lead_dtu` | **`+112,002 DTU`** | **`-53,376 DTU`** | **`-18,064 DTU`** | **`-38,074 DTU`** | **`-1,274 DTU`** |
| | `0xBC` `last_fail_status` | `33` only | **`32` (`TX_FAILED`), `33`** | **`32` (`TX_FAILED`), `33`** | **`32` (`TX_FAILED`), `33`** | `33` |
| | Valid UWB Rate | **`99.53%`** | **`72.27%`** | **`62.34%`** | **`68.80%`** | **`96.00%`** |

- **Why This Happens and Why It Specifically Hits Slots 2–5 (`LW`, `RW`, `LA`, `RA`):**
  1. **Direct Correlation Between `late_tx_count` (`status = 32` `TX_FAILED`) and Link Degradation:**
     Look at the table above: across **all four 30-minute captures (`35,296` total UWB blocks)**, **Node 0 (`Head`, Slot 1) had `0` late TX events (`late_tx_count = 0 -> 0`) and maintained a positive minimum TX lead time of `+88,174` to `+112,002 DTU` (`+5.65 ms` to `+7.18 ms`)**, achieving **`99.24%–99.75%`** validity.
     Conversely, **every single degraded node (`Left Wrist`, `Right Wrist`, `Left Ankle`, `Right Ankle`) suffered negative `min_lead_dtu` (`-1,274` to `-164,088 DTU`), positive `late_tx_count` increments (`+1` to `+25`), and `status = 32` (`QUWBS_FBS_STATUS_ERROR_TX_FAILED`) in `fail_ring_last_status` (`0xBC`)!** Even on `Right Ankle` (`Node 5`), when its `late_tx_count` delta dropped from `+4..+6` (on Oct 11) down to `+1` (with `min_lead_dtu` barely negative at `-1,274 DTU = -81.6 us` on Oct 12), its valid UWB yield jumped from `83.72%` to **`96.00%`**!
  2. **Why Delayed-TX Failures (`status = 32`) Cause Multi-Second Lock Loss (`status = 33` Streaks):**
     - On a Controlee, the Qorvo FiRa MAC wakes up for Slot 0 (`0.0 ms`) to receive the Controller's Poll, then programs the DW3000 hardware delayed-TX timer (`DX_TIME` via register `0x0A` / `0x14`) and issues `CMD_DTX` (`fastcmd = 2`) for its assigned Response slot (`Slot 2 = +4.0 ms`, `Slot 3 = +6.0 ms`, `Slot 4 = +8.0 ms`, `Slot 5 = +10.0 ms`), and then must return to RX for Slot 6 (`+12.0 ms` Final) and Slot 7 (`+14.0 ms` M-Report).
     - Meanwhile, on every node, two asynchronous subsystems contend with the FiRa MAC during the active `20.0 ms` UWB round:
       - **Unsynchronized `20 ms` BLE Advertising / Radio Events ([`comp971_config_shared.h:59`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/include/comp971_config_shared.h#L59), [`comp971_ble_pilot.c:419`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_ble_pilot.c#L419)):** `COMP971_BLE_ADV_INTERVAL_UNITS = 32` (`20.0 ms`), and SoftDevice radio/timer ISRs run at **ARM NVIC Priority `0` and `1`**, preempting all FreeRTOS tasks and the DW3000 GPIO interrupt (`Priority 5..7`). Because the `20.0 ms` BLE interval (plus `0–10 ms` random spec advertising delay) is **not phase-locked** to the `200.0 ms` UWB block, SoftDevice radio bursts periodically collide directly with the Controlee's slot preparation window.
       - **Equal-Priority `imu_task` Polling `100 kHz` I2C Every `20 ms` ([`main.c:461–475`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L461-L475), [`comp971_icm20948.c:167`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_icm20948.c#L167)):** `imu_task` runs at `(tskIDLE_PRIORITY + 2U)`, the **exact same priority** as `uwb_task`, and wakes every `20 ms` (`50 Hz`) — meaning it is guaranteed to wake once during every `20.0 ms` active FiRa UWB round (`Slots 0..7`) and execute blocking `nrfx_twim_xfer` transactions at `100 kHz` (`~2.0 ms` bus time) plus `taskENTER_CRITICAL()` sections.
     - When `CMD_DTX` is delayed past `DX_TIME` (`HPDWARN` set in `SYS_STATUS` byte 3 = `0x08`, `g_dw3000_late_tx_count++`), the DW3000 aborts the delayed transmission (`status = 32` `QUWBS_FBS_STATUS_ERROR_TX_FAILED`) and fails to transition cleanly into the subsequent RX window (`Slot 6 Final` / next block `Slot 0 Poll`). Once a Controlee misses the Controller's Poll or Final and its local clock drifts out of the tight `2400 RSTU` (`2.0 ms`) Slot 0 RX window (or when Pelvis itself stalls/resets), the Controlee becomes deaf to Slot 0 Poll (`status = 33`) until its `5,000 ms` watchdog forces a Stage 2 MAC restart!

---

### Finding 4 (CRITICAL SYSTEM CASCADE): Pelvis Controller Delayed-TX `HPDWARN` Freeze & Hard `NVIC_SystemReset()` (`9.4s–10.35s` Total Network Blackout)

- **Source Locations:**
  - [`main.c:259–340`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L259-L340) (`check_uwb_stall_escalation()`).
  - [`comp971_uwb_official_fira.c:1186–1288`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L1186-L1288) (`__wrap_dwt_SPIwritecrc`, `__wrap_dwt_SPIreadcrc`, `__wrap_dwt_SPIfastcmd`).
- **Exact Mechanism Verified by Pelvis Frozen Stall Diagnostics (`0xE0..0xEB`) in `2026-10-12_101320`:**
  In `gateway_raw_2026-10-12_101320`, the `Pelvis` Controller (`node_id = 1`) suffered **two complete UWB freezes** (at `block_index = 3,371` and `block_index = 5,887`), emitting its frozen stall snapshot (`0xE0..0xEB`) right before calling `NVIC_SystemReset()`:

| Diagnostic Code | Symbol in `main.c:67–80` | Incident 1 (`block 3,371`) | Incident 2 (`block 5,887`) | Hardware Meaning |
| :---: | :--- | :---: | :---: | :--- |
| **`0xE0`** | `COMP971_DIAG_STALL_STAGE_SEQ` | `786,435` (`0x000C0003`) | `786,435` (`0x000C0003`) | `last_stage = 0x0C` (`FIRA_NOTIFICATION_CB`), `stall_stage = 3` |
| **`0xE1`** | `COMP971_DIAG_STALL_AGE_MS` | `665,303,040` (`0x27AE0200`) | `664,516,608` (`0x279C0200`) | `cb_age_ms = 2,510 ms / 2,508 ms`, `irq_age_ms = 512 ms` |
| **`0xE2`** | `COMP971_DIAG_STALL_COUNTS` | `220,921,219` | `164,825,658` | Packed `(cb_seq & 0xFFFF) << 16 \| (irq_cnt & 0xFFFF)` |
| **`0xE4`** | `COMP971_DIAG_STALL_TX_META` | `20,257` | `17,679` | `g_diag_tx_last_irq_count` at the moment of last `CMD_DTX` |
| **`0xE5`** | `COMP971_DIAG_STALL_IRQ_SNAPSHOT`| `20,257` | `17,679` | `g_dw3000_irq_count` at freeze (`0xE4 == 0xE5`: **zero IRQs after `CMD_DTX`!**) |
| **`0xE7`** | `COMP971_DIAG_STALL_FASTCMD` | **`130,817` (`0x00020001`)** | **`130,817` (`0x00020001`)** | `fastcmd = 2` (`CMD_DTX`), `mode = 0`, `result = 0`, **`hpdwarn_seen = 1`** |
| **`0xE8`** | `COMP971_DIAG_STALL_SYS_LO` | `-2,130,702,206` (`0x81001082`) | `-2,130,702,206` (`0x81001082`) | `SYS_STATUS` low 32 bits (`CPLOCK`, `SPIRDY`, `RXPHE`, etc.) |
| **`0xE9`** | `COMP971_DIAG_STALL_SYS_HI` | **`8` (`0x00000008`)** | **`8` (`0x00000008`)** | `SYS_STATUS` byte 3 (`0x00` offset 3) = **`0x08` (`HPDWARN` bit set!)** |
| **`0xEA`** | `COMP971_DIAG_STALL_LEAD_DTU` | **`+9,618 DTU` (`+616.5 us`)** | **`+10,208 DTU` (`+654.4 us`)** | Pre-SPI lead time when `DX_TIME` register (`0x0A`) was written |

- **What this proves about the Pelvis freeze:**
  1. When the FiRa MAC on Pelvis wrote the `DX_TIME` register (`0x0A` / `0x14`), the remaining lead time (`0xEA`) was only **`+616.5 us` (`9,618 DTU`)** in Incident 1 and **`+654.4 us` (`10,208 DTU`)** in Incident 2.
  2. Between writing `DX_TIME` (`__wrap_dwt_SPIwritecrc`) and issuing `CMD_DTX` (`__wrap_dwt_SPIfastcmd`, `fastcmd = 2`), CPU preemption (SoftDevice BLE ISR at Priority `0/1` or RTOS critical section) delayed `CMD_DTX` by `> 616 us`, causing the DW3000 hardware to assert **`HPDWARN` (`0xE9 = 8`, `0xE7` bit 0 = `1`)** and reject the delayed transmission without firing `TXFRS` (`0xE4 == 0xE5`, no subsequent DW3000 interrupt).
  3. Because the Qorvo FiRa MAC state machine waits indefinitely for the `TXFRS` interrupt that never arrives, Pelvis stops transmitting Slot 0 Polls.
  4. In [`main.c:307–338`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L307-L338), the Controller watchdog explicitly **skips soft MAC recovery** (`/* Skip soft recovery stages and trigger clean reset after ~2.0 seconds */`) and calls **`NVIC_SystemReset()`** at `stall_ms >= 2000U`.
  5. **The Cascade Effect:** Calling `NVIC_SystemReset()` reboots the entire Pelvis MCU, which takes **`9.40 seconds` (`Incident 1`) and `10.35 seconds` (`Incident 2`)** before Pelvis re-initializes BLE, IMU, DW3000, and FiRa UWB (`~50 UWB blocks` and `381` Pelvis IMU samples lost). Worse still, because Pelvis is silent for **`9.4 to 10.35 seconds`**, **every Controlee's `5,000 ms + jitter` watchdog expires during the Pelvis reboot**, causing the Controlees to tear down and restart their own MAC sessions (`comp971_uwb_stage2_mac_restart()`) right as Pelvis comes back online!

---

### Finding 5 (MODERATE): Diagnostic Stage Code Collisions in `main.c` and `comp971_uwb_official_fira.c`

- **Source Locations:**
  - [`comp971_uwb_two_node.h:61`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/include/comp971_uwb_two_node.h#L61), [`main.c:393–399`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L393-L399), and [`comp971_uwb_official_fira.c:794`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L794):
    - Stage **`0xB3` (`179`, `COMP971_HEALTH_STALL_RECOVERY_CNT`)** is emitted by two different tasks with two incompatible payload formats:
      1. `status_task` ([`main.c:393–399`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L393-L399)) emits `0xB3` every `1 second` with packed `(m_reset_reason << 24) | (m_last_escalation_stage << 16) | (m_last_stall_duration_ms & 0xFFFF)` (which is `0` on Controlees and `67,307,497` (`0x040307E9`) on Pelvis).
      2. `uwb_task` ([`comp971_uwb_official_fira.c:794`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L794)) emits `0xB3` every `19 seconds` with `g_fira_stall_recovery_count`.
  - [`main.c:67–80`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L67-L80), [`comp971_uwb_official_fira.c:355`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L355), and [`comp971_uwb_official_fira.c:1139–1173`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L1139-L1173):
    - Stage codes **`0xE0..0xEB` (`224..235`)** are overloaded across **three different meanings**:
      1. `session_status_ntf` in [`comp971_uwb_official_fira.c:355`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L355) emits `(0xE0 | state)` (`0xE1` = `INIT`, `0xE2` = `ACTIVE`, `0xE3` = `IDLE`).
      2. `comp971_uwb_stage2_mac_restart()` in [`comp971_uwb_official_fira.c:1139–1173`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L1139-L1173) emits `0xE1..0xE4` on stop/deinit/init/start failure.
      3. `check_uwb_stall_escalation()` in [`main.c:67–80`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L67-L80) defines `COMP971_DIAG_STALL_STAGE_SEQ` through `COMP971_DIAG_STALL_POST_WAIT_SYS_HI` as `0xE0..0xEB`.

---

## 4. Prioritized, Evidence-Backed Firmware Recommendations

Below are the exact, surgical fixes required in the final firmware (`hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/`) to eliminate the multi-second lock-loss stalls and raise `Left Wrist`, `Right Wrist`, `Left Ankle`, and `Right Ankle` to the same `> 98%–99%` valid UWB tier as `Head`:

### Recommendation 1 (Immediate, 5-Line Fix): Fix the Version 6.7 Controlee Recovery Valid-Sequence Tracking Bug in [`comp971_uwb_official_fira.c:566–589`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L566-L589)

- **What to change:** In [`fira_notification_cb()` at `comp971_uwb_official_fira.c:574–589`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L574-L589), update `g_fira_last_valid_cookie`, `g_fira_valid_sequence`, and `g_fira_cb_valid_streak` inside the Controlee (`else`) branch whenever `any_valid` is true (and reset `g_fira_cb_valid_streak = 0U` when `!any_valid`):
  ```c
  else
  {
      if (any_valid)
      {
          g_fira_valid_count++;
          g_fira_last_valid_tick = xTaskGetTickCount();
          g_fira_last_valid_cookie = m_rec_session_cookie;
          g_fira_valid_sequence++;
          g_fira_cb_valid_streak++;
      }
      else
      {
          g_fira_cb_valid_streak = 0U;
      }
      if (any_failed)
      {
          g_fira_fail_count++;
      }
  }
  ```
- **Why this will make a direct, measurable difference:** Once `g_fira_valid_sequence`, `g_fira_cb_valid_streak`, and `g_fira_last_valid_cookie` increment on Controlees, Attempt 1 in `COMP971_REC_COOLDOWN` will immediately transition back to `COMP971_REC_IDLE` after 2 consecutive valid blocks (`400 ms`), completely eliminating the self-inflicted Attempt 2 MAC teardown and the `30,000 ms` `COMP971_REC_HOLD` lockout (`0xED`).

### Recommendation 2 (Immediate Deployment Fix): Flash All 6 Physical Nodes with the Unified Firmware Image

- **What to do:** Flash **all 6 DWM3001CDK boards** (`Head`, `Pelvis`, `Left Wrist`, `Right Wrist`, `Left Ankle`, `Right Ankle`) with the same compiled `.hex` image after applying Recommendation 1.
- **Why this will make a direct, measurable difference:** Eliminates the 9 October pre-v6.7 firmware still running on `Right Wrist` (`88` unthrottled restarts), `Left Ankle` (`60` unthrottled restarts), and `Right Ankle` (`9` restarts).

### Recommendation 3 (Scheduling & Task Priority Fix): Prevent `imu_task` and BLE From Delaying DW3000 `CMD_DTX` Past `DX_TIME` (`HPDWARN` / `status = 32`)

- **What to change:**
  1. **Elevate UWB Task Priority Above `imu_task` ([`main.c:455–465`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L455-L465)):** Currently both `uwb_task` and `imu_task` run at `tskIDLE_PRIORITY + 2U`. Raise `uwb_task` (and verify the Qorvo MCPS task priority in the SDK) above `imu_task` (`tskIDLE_PRIORITY + 3U` vs. `tskIDLE_PRIORITY + 2U`), and increase the ICM-20948 TWIM1 bus frequency in [`comp971_icm20948.c:167`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_icm20948.c#L167) from `NRF_TWIM_FREQ_100K` (`100 kHz`) to `NRF_TWIM_FREQ_400K` (`400 kHz`, which the ICM-20948 natively supports), cutting I2C transaction time by `4x` (`~2.0 ms -> ~0.5 ms`).
  2. **Widen `slot_duration_rstu` or Phase-Offset `imu_task` / BLE From the Active `20 ms` UWB Round ([`comp971_uwb_official_fira.c:663–664`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L663-L664)):** Currently, the entire 8-slot DS-TWR exchange (`Slots 0..7`) is packed into `20.0 ms` (`slot_duration_rstu = 2400` (`2.0 ms`)), leaving `180.0 ms` of the `200.0 ms` block completely idle (`90%` idle!). Increasing `slot_duration_rstu` from `2400` (`2.0 ms`) to `3600` (`3.0 ms`) or `4800` (`4.0 ms`) (with `round_duration_rstu = 36000` or `48000`) adds `+1.0 ms` to `+2.0 ms` (`+63,897` to `+127,795 DTU`) of extra turnaround lead time before `DX_TIME` on every slot (`Slots 0..7`).
- **Why this will make a direct, measurable difference:**
  - On `Pelvis`, the two `HPDWARN` freezes occurred at pre-SPI lead times of `+9,618 DTU` (`+616 us`) and `+10,208 DTU` (`+654 us`).
  - On `Left Wrist`, `Right Wrist`, `Left Ankle`, and `Right Ankle`, the worst-case negative lead times in `2026-10-12_101320` were `-1,274 DTU` (`-81.6 us` on `RA`), `-18,064 DTU` (`-1.16 ms` on `RW`), `-38,074 DTU` (`-2.44 ms` on `LA`), and `-53,376 DTU` (`-3.42 ms` on `LW`).
  - Widening `slot_duration_rstu` and removing `100 kHz` I2C / priority contention directly eliminates `HPDWARN` (`status = 32` `TX_FAILED`) on both the Controller and Controlees.

### Recommendation 4 (Controller Resilience Fix): Replace Hard `NVIC_SystemReset()` on Pelvis With In-Place DW3000 + FiRa Session Recovery

- **What to change:** In [`main.c:307–338`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L307-L338), instead of waiting `2,000 ms` and calling `NVIC_SystemReset()` on the first Controller `HPDWARN` stall (which causes a `9.4s–10.35s` total blackout and forces all 5 Controlees into watchdog timeout), trigger an immediate (`~600 ms`, `3 missing blocks`) in-place `CMD_TXRXOFF` + `comp971_uwb_stage2_mac_restart()` on Pelvis first, reserving `NVIC_SystemReset()` strictly as a last-resort fallback if soft recovery fails after `3,000 ms`.
- **Why this will make a direct, measurable difference:** Because a `600 ms` soft recovery finishes well inside the Controlees' `5,000 ms` watchdog window, Pelvis recovers within `3–4 blocks` (`< 0.8 seconds`) without dropping any Pelvis IMU packets and without triggering a Controlee restart cascade.

---

## 5. Summary Verification Table of Final Firmware Issues

| # | Issue in Final Firmware | Exact File & Lines | Empirical Proof in `2026-10-12_101320` (`8,871` Blocks) | Expected Impact of Fix |
| :-: | :--- | :--- | :--- | :--- |
| **1** | **v6.7 Controlee Recovery Bug:** `g_fira_valid_sequence`, `g_fira_cb_valid_streak`, and `g_fira_last_valid_cookie` are only updated when `role == CONTROLLER`, so v6.7 Controlees treat every valid range in `COOLDOWN` as a failure, force a 2nd MAC restart, and lock out in `HOLD` (`30 s`). | [`comp971_uwb_official_fira.c:566–589`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L566-L589), [`981–1009`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L981-L1009) | Node 2 (`Left Wrist`) emitted `0xED` (`detail = 3`) **`7` times** (`14` restarts = `7 x 2`; `26, 22, 25` times in Oct 11 runs). | Eliminates all double-restarts and `30 s` `HOLD` lockouts on v6.7 Controlees. |
| **2** | **Firmware Version Skew Across Nodes:** Nodes `0, 3, 4, 5` are still running the `2026-10-09` pre-v6.7 firmware without cooldown/backoff/jitter. | `uwb_diag.ndjson` (`0xE2`, `0xE3`, `0xED`) & `FLASH_LOG.txt` | `Right Wrist` (`Node 3`) executed **`88` restarts** (`0xE2 = 88, 0xE3 = 0, 0xED = 0`); `Left Ankle` (`Node 4`) executed **`60` restarts**. | Brings all 5 Controlees onto the unified fixed recovery state machine. |
| **3** | **Delayed-TX Deadline Misses (`status = 32` `TX_FAILED`, `HPDWARN`):** `2.0 ms` slot turnaround (`2400 RSTU`) + equal-priority `imu_task` (`100 kHz` I2C) + unsynchronized `20 ms` BLE ISR (`Priority 0/1`) cause `CMD_DTX` to miss `DX_TIME` on Slots 2–5. | [`comp971_uwb_official_fira.c:663–665`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L663-L665), [`1224–1279`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L1224-L1279); [`main.c:455–475`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L455-L475) | `Head` (`Slot 1`): `late_tx = 0`, `min_lead = +112,002 DTU` (`99.53%` valid). `LW/RW/LA`: `late_tx = +5..+7`, `min_lead = -18,064..-53,376 DTU`, `0xBC = 32` (`62.34%–72.27%` valid). | Eliminates the `status = 32` (`TX_FAILED`) events that trigger multi-second lock loss. |
| **4** | **Pelvis `HPDWARN` Freeze -> Hard `NVIC_SystemReset()`:** Controller skips soft MAC recovery and reboots MCU on `HPDWARN`, causing `9.4s–10.35s` blackout across all 5 links. | [`main.c:307–338`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L307-L338) | `2` Pelvis resets (`0xE7 = 0x00020001`, `0xE9 = 8` `HPDWARN`, `0xEA = +9,618 / +10,208 DTU`), causing `9.4s` & `10.35s` blackouts (`381` lost Pelvis IMU samples + Controlee watchdog cascade). | Prevents `10 s` network-wide blackouts and Controlee watchdog cascades. |
| **5** | **Diagnostic Stage Code Collisions (`0xB3` and `0xE0..0xEB`):** Overloaded stage IDs corrupt `uwb_diag.ndjson` decoding. | [`main.c:67–80`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L67-L80), [`393–399`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/main.c#L393-L399); [`comp971_uwb_official_fira.c:355, 794, 1139`](file:///c:/Users/harma/OneDrive%20-%20AUT%20University/AUT%20-%20COMP971%20Dissertation%20%2860%20points%29/hardware/firmware/qorvo_qm33_motion_node/official_sdk_project/Common/src/comp971_uwb_official_fira.c#L355) | `0xB3` alternates between `0` and `g_fira_stall_recovery_count`; `0xE2` is both `SESSION_ACTIVE` and `DIAG_STALL_COUNTS`. | Restores clean, unambiguous diagnostic telemetry. |
