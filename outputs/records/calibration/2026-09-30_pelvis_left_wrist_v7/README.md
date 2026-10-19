# PELVIS–LEFT-WRIST raw range checks at stated 50 cm

## Setup

- Date: 2026-09-30
- Firmware: UWB diagnostic wearable firmware with gateway timestamp v7
- Boards: PELVIS and LEFT-WRIST on battery; other wearable streams were absent
- Pair index: 5 (`PELVIS <-> LEFT-WRIST`)
- Distance: 50 cm as reported by the user. The exact measurement reference
  points on the two enclosures/antennas were not independently confirmed.
- Two captures, each 45 seconds, plus a 1-second buffer-clear period
- No calibration correction was applied.

## Result

The first decoded capture contains 48 range reports: 43 valid and 5 invalid.
The repeat contains 45 reports: 34 valid and 11 invalid. Across both runs,
77 of 93 reports were valid (82.8%). Among the 77 valid ranges:

| Measure | Result |
|---|---:|
| Minimum | 59 cm |
| Median | 64 cm |
| Mean | 63.97 cm |
| Sample standard deviation | 1.99 cm |
| Maximum | 69 cm |

The run medians were 64 cm and 63.5 cm. Compared with the stated 50 cm, the
combined median is 14 cm high. The similar medians suggest a repeatable
positive offset in this particular setup, but two runs at one distance cannot
tell whether the error is a fixed offset or a scale error. Do not subtract
14 cm or apply any correction yet.

Pair-5 diagnostic event counts were 15 `response_wait_failed`, 15
`responder_response_tx_failed`, 11 `responder_final_not_received`, and 10
`final_tx_failed`. These are event counts, not a count of unique ranging
attempts: one attempt can produce events at more than one stage. Do not use
their sum as the failure-rate denominator.

The same capture contained 2,682 PELVIS and 2,690 LEFT-WRIST IMU samples, with
17 and 13 sequence gaps respectively. Median per-sample timing remains near
60 Hz. No invalid USB frames were found.

## Retained data

- Raw capture: `../../acquisition/gateway_raw_2026-09-30_125750.bin`
- Decoded records: `../../acquisition/gateway_raw_2026-09-30_125750_decoded/`
- Repeat raw capture: `../../acquisition/gateway_raw_2026-09-30_130058.bin`
- Repeat decoded records: `../../acquisition/gateway_raw_2026-09-30_130058_decoded/`

## Next test

The 75 cm measurement is recorded in
`../2026-09-30_pelvis_left_wrist_75cm_v7/README.md`; the two 100 cm runs are in
`../2026-09-30_pelvis_left_wrist_100cm_v7/README.md`. The 100 cm values were
scattered, so no correction is justified yet. Check the RF setup, then repeat
the distance series under a controlled, consistent layout.
