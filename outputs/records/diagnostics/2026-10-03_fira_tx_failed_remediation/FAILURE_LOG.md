# Failure Log & Remediation Ledger: FiRa TX_FAILED Fix
**Date**: 2026-10-03  
**Target Hardware**: Decawave/Qorvo DWM3001CDK (nRF52833 + DW3110)  
- HEAD: Node 0, SN 760225805, COM13 (Controlee / Responder)
- PELVIS: Node 1, SN 760224787, COM6 (Controller / Initiator)
- Gateway: nRF52840 Dongle, COM16

---

## 1. Problem Statement
PELVIS (Node 1) halts after 4 ranging rounds with `UCI_STATUS_RANGING_TX_FAILED` (32 / 0x20). HEAD (Node 0) runs rounds at 5 Hz but receives no Poll frames, resulting in `UCI_STATUS_RANGING_RX_TIMEOUT` (33 / 0x21).

## 2. Root Cause Analysis
1. **Clock Domain**: When `nrf_sdh_enable_request()` runs, SoftDevice takes exclusive ownership of nRF52 clocks. Between BLE intervals, SoftDevice shuts down the 64 MHz HFXO crystal. The MCU runs on the internal RC oscillator (HFINT), causing SPIM3 and DW3110 delayed transmit timing to drift. DW3110 detects the programmed transmit time has already elapsed and raises `SYS_STATUS_HPDWARN` (bit 27), aborting transmit.
2. **Session Configuration**: On Controller, `n_destination_short_address` must be 0 (destinations are added via `fira_helper_add_controlee`). `max_rr_retry` must be 0 to disable retry abort per FiRa UCI specification.

## 3. Applied Modifications
- `comp971_ble_pilot.c`: Request HFXO via `nrf_drv_clock_hfclk_request(NULL)` inside `ble_stack_init()` *after* `nrf_sdh_ble_enable()`.
- `comp971_uwb_official_fira.c`: Controller `n_destination_short_address = 0`, `max_rr_retry = 0` (unlimited).
- Direct transceiver hardware status logging for instant diagnostics.
