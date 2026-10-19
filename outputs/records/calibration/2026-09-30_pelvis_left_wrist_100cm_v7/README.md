# PELVIS–LEFT-WRIST raw range checks at stated 100 cm

- Date: 2026-09-30
- Pair: index 5 (`PELVIS <-> LEFT-WRIST`)
- Distance: 100 cm between antenna centers, as stated for these runs
- Captures: two 45-second runs, each preceded by a 1-second buffer clear
- Firmware: diagnostic wearable build and gateway timestamp v7
- No calibration correction was applied.

## Results

| Run | Reports | Valid | Invalid | Valid rate | Valid range | Median | Mean | Sample SD |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 43 | 25 | 18 | 58.1% | 91–149 cm | 99 cm | 106.20 cm | 17.35 cm |
| 2 | 45 | 31 | 14 | 68.9% | 90–149 cm | 101 cm | 103.97 cm | 13.41 cm |
| Combined | 88 | 56 | 32 | 63.6% | 90–149 cm | 99 cm | 104.96 cm | 15.19 cm |

The medians are close to the stated 100 cm, but the readings are not stable:
many are around 90–103 cm while a smaller number reach 111–149 cm. Only 13 of
the first run's 25 valid results were between 95 and 105 cm. Do not treat the
near-correct median as proof of accuracy, and do not fit a correction from
these data.

Together with the earlier 50 cm (median 64 cm) and 75 cm (median 90 cm) results,
these measurements do not support one simple fixed offset. The failure rate
and large spread—especially at 100 cm—must be investigated before calibration
is accepted.

## Retained data

- Run 1 raw: `../../acquisition/gateway_raw_2026-09-30_130808.bin`
- Run 1 decoded: `../../acquisition/gateway_raw_2026-09-30_130808_decoded/`
- Run 2 raw: `../../acquisition/gateway_raw_2026-09-30_130941.bin`
- Run 2 decoded: `../../acquisition/gateway_raw_2026-09-30_130941_decoded/`

## Clear-path follow-up at 100 cm

Two additional 45-second captures were made after the user adjusted the setup
for a clear path, keeping the stated 100 cm spacing:

| Run | Reports | Valid | Invalid | Valid rate | Valid range | Median | Mean | Sample SD |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 3 | 43 | 26 | 17 | 60.5% | 88–100 cm | 92 cm | 93.15 cm | 3.70 cm |
| 4 | 38 | 23 | 15 | 60.5% | 91–135 cm | 98 cm | 101.61 cm | 11.90 cm |
| Combined | 81 | 49 | 32 | 60.5% | 88–135 cm | 95 cm | 97.12 cm | 9.50 cm |

The spread changed between the two runs and about 39.5% of reports were still
invalid. These measurements are not suitable for calibration.

The v7 capture also revealed that this is not actually an isolated pair test.
The PELVIS coordinator firmware loops through all 15 mesh pairs every sweep
(`run_mesh()` in `hardware/firmware/archive/dwm3001cdk_legacy_proto/src/main.c`). In the
fourth capture, only PELVIS and LEFT-WRIST IMU streams appeared, while PELVIS
status counters increased by 971 mesh range attempts, 944 invalid results,
646 result timeouts, and 76 failed UWB BLE notifications during the recorded
window (counter differences between its first and last status messages). This
likely creates substantial diagnostic traffic and is associated with IMU
queue/notification drops in the same capture; it does not, by itself, prove
the cause of the range bias. The pair measurements must not be treated as a
clean pair-only calibration until the coordinator schedule is restricted or
all peers participate reliably.

- Run 3 raw: `../../acquisition/gateway_raw_2026-09-30_131602.bin`
- Run 3 decoded: `../../acquisition/gateway_raw_2026-09-30_131602_decoded/`
- Run 4 raw: `../../acquisition/gateway_raw_2026-09-30_131739.bin`
- Run 4 decoded: `../../acquisition/gateway_raw_2026-09-30_131739_decoded/`

## Next

Do not change calibration constants yet. First run a true pair-only test (or
ensure all peers are participating reliably), because the current PELVIS
firmware attempts all 15 pairs even when the experiment is measuring only
pair 5. Then repeat the known-distance series with fixed antenna-center
spacing, alignment, and clear line of sight.
