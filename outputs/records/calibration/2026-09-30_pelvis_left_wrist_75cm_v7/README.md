# PELVIS–LEFT-WRIST raw range check at stated 75 cm

- Date: 2026-09-30
- Pair: index 5 (`PELVIS <-> LEFT-WRIST`)
- Distance: 75 cm between antenna centers, as stated for this run
- Capture: 45 seconds plus 1 second clearing old buffered data
- Firmware: diagnostic wearable build and gateway timestamp v7
- No calibration correction was applied.

## Results

The capture contains 46 pair reports: 32 valid and 14 invalid (69.6% valid).
For the 32 valid readings, the range was 88–93 cm, median 90 cm, mean 90 cm,
and sample standard deviation 1.57 cm. The median is 15 cm above the stated
75 cm.

This is one run, not a final calibration. The 50 cm runs had a combined median
of 64 cm, about 14 cm high. The similar error at 50 and 75 cm suggests a
roughly fixed offset, but the 100 cm point and a later validation run are
needed before choosing any correction.

Pair-5 diagnostic event counts: 26 `response_wait_failed`, 25
`responder_response_tx_failed`, 11 `final_tx_failed`, and 11
`responder_final_not_received`. These are events, not unique attempts; one
range attempt may create more than one event.

## Data files

- Raw: `../../acquisition/gateway_raw_2026-09-30_130438.bin`
- Decoded: `../../acquisition/gateway_raw_2026-09-30_130438_decoded/`

## Next

The 100 cm tests are recorded in
`../2026-09-30_pelvis_left_wrist_100cm_v7/README.md`. They were scattered, so
do not apply an offset. Check the controlled setup and RF path before
repeating the distance series.
