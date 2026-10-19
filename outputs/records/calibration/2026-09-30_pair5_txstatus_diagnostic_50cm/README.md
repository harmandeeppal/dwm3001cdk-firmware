# Pair-5 transmit-status diagnostic capture at 50 cm

## Result

The 60-second raw capture was taken on 2026-09-30 at stated 50 cm with PELVIS
and LEFT-WRIST on battery and the other wearables off. PELVIS had the new
transmit-status diagnostic firmware. LEFT-WRIST still had the regular
failure-stage diagnostic firmware. This is the pre-LEFT-WRIST-diagnostics
capture; the repeat with both endpoints instrumented is pending.

| Measure | Result |
|---|---:|
| Valid range reports received by gateway | 115 (~1.9/s) |
| Reported range median | 74 cm |
| Reported range min / max | 65 / 88 cm |
| PELVIS UWB attempts during capture | 3,300 |
| Valid / invalid attempts | 1,991 / 1,309 |
| Invalid attempt rate | 39.7% |
| Failed UWB BLE notifications during capture | 0 |
| Invalid framed gateway records | 0 |

The range median was 24 cm above the stated 50 cm. Accuracy remains
unaccepted. The attempt invalid rate is measured from PELVIS's counters; it is
not the percentage of BLE reports that failed.

## What the failure events show

| Stage | Events | Node reporting it |
|---|---:|---|
| `response_wait_failed` | 1,181 | PELVIS did not receive LEFT-WRIST's response in its receive window |
| `responder_response_tx_failed` | 1,036 | LEFT-WRIST's reply transmit did not start successfully |
| `final_tx_failed` | 148 | PELVIS's final transmit did not start successfully |
| `responder_final_not_received` | 148 | LEFT-WRIST did not receive the final frame |

These are stage events from both ends, not separate unique range attempts, so
do not add them to get a failure count. The event mix localizes the problem to
the UWB exchange rather than BLE delivery, but it does not yet prove the radio
root cause.

The PELVIS image recorded `SYS_STATUS` detail `0` on all 148
`final_tx_failed` events. That value alone does not tell us why those
transmits failed. The 1,036 LEFT-WRIST reply-failure events had detail `-1`:
that board did not yet have the new status logger. Therefore the most common
failure still lacks the needed radio status.

## Files

- Raw capture: `../../acquisition/gateway_raw_2026-09-30_151245.bin`
- Metadata: `../../acquisition/gateway_raw_2026-09-30_151245.json`
- Decoded files: `../../acquisition/gateway_raw_2026-09-30_151245_decoded/`
- Diagnostic events: `../../acquisition/gateway_raw_2026-09-30_151245_decoded/uwb_diag.ndjson`
- Node counters: `../../acquisition/gateway_raw_2026-09-30_151245_decoded/node_status.ndjson`
- Summary: `../../acquisition/gateway_raw_2026-09-30_151245_decoded/summary.json`

## Next

The repeat with both endpoints instrumented is now recorded at
`../2026-09-30_pair5_txstatus_both_50cm/README.md`. LEFT-WRIST's reply-TX
failures also reported raw status detail `0`, so this post-return value still
does not identify the cause. The next diagnostic must save the radio state
inside the driver before its error-recovery command; do not change UWB timing
yet.
