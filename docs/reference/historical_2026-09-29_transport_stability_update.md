# Six-node Bluetooth stability update

## Why this update exists

The cleaned 60-second capture showed that USB CDC has no live-session drops,
but some wearable boards restarted after missing a gateway heartbeat. Those
restarts made their IMU sequence counters return near zero and prevented a
continuous six-node capture.

## Shared update

All six wearables now use the same transport behaviour:

1. IMU stays sampled at 60 Hz.
2. Nine samples are sent in one BLE notification every 150 ms. This reduces
   BLE notification load while retaining every sample.
3. Bluetooth ATT/L2CAP capacity is raised to 247 bytes; a nine-sample message
   is 206 bytes and needs ATT MTU 209.
4. A missed gateway heartbeat no longer reboots a wearable. The gateway's
   normal stale-link watchdog remains responsible for reconnecting a dead link.
5. PELVIS limits each pair's UWB reports to at most 2 Hz, not 10 Hz. This is a
   per-pair cap, not a promise that all 15 pairs complete twice per second.
6. The gateway requests a 100 ms connection interval for all six links.

## Safety and verification

This changes firmware only; it does not change ICM wiring, UWB radio settings,
or calibration values. The firmware was built for the gateway and all six
wearables, and each was flashed to its serial-number-identified board. The
30 September 2026 one-minute capture is recorded below. The result supports
the IMU/BLE transport improvement, but does not pass UWB reliability or
calibration.

## Hardware test — 30 September 2026

All six battery-powered boards and the USB gateway ran together during a
60-second raw capture. The capture was taken after discarding three seconds of
stale USB-buffered records. Raw evidence:

- Binary capture: `../../outputs/records/acquisition/gateway_raw_2026-09-30_091515.bin`
- Decoded evidence: `../../outputs/records/acquisition/gateway_raw_2026-09-30_091515_decoded/`
- Firmware source is in `../firmware/`; device identities are in
  `../config/device_registry.json`.

### IMU and BLE result

| Board | IMU records | Sequence gaps | Wearable queue drops during capture |
|---|---:|---:|---:|
| HEAD | 3,582 | 1 | 1 |
| PELVIS | 3,582 | 8 | 8 |
| LEFT-WRIST | 3,581 | 9 | 9 |
| RIGHT-WRIST | 3,582 | 7 | 7 |
| LEFT-ANKLE | 3,582 | 16 | 16 |
| RIGHT-ANKLE | 3,582 | 21 | 21 |

The total is 21,491 received IMU records and 62 sequence gaps across the six
streams (about 59.7 records/s per node). No IMU BLE notification failures were
added during the capture. The gateway reported zero bad packets, and its
cumulative USB-drop counter did not increase during the capture. The 56,625
USB drops shown in the counter were historical, not drops during this test.
This is a strong improvement, but not a zero-loss pass: 62 samples were
accounted as missing at the wearable queues.

### UWB result

The gateway recorded 969 UWB pair reports (about 16.2 reports/s total, or
about 1.08 complete 15-pair sweeps/s on average). Of these, 584 had a valid
range and 385 were invalid. Five pairs had no valid
range in this capture: HEAD–PELVIS, PELVIS–LEFT-WRIST, LEFT-WRIST–RIGHT-WRIST,
RIGHT-WRIST–LEFT-ANKLE, and LEFT-ANKLE–RIGHT-ANKLE. UWB is therefore still
not accepted, and no UWB calibration should be applied from this capture.

## Repeat check — boards grouped together, 30 September 2026

A second 60-second capture was made with the boards placed next to each other.
The exact antenna-to-antenna distances were not measured, so these ranges are
not a calibration dataset.

- Binary capture: `../../outputs/records/acquisition/gateway_raw_2026-09-30_092234.bin`
- Decoded evidence: `../../outputs/records/acquisition/gateway_raw_2026-09-30_092234_decoded/`
- No malformed frames were found.
- HEAD provided no IMU records and no node-health reports. All five UWB pairs
  involving HEAD had zero valid ranges. This points to HEAD being offline or
  not connected to the gateway during this test; it does not show that those
  five ranging links work.
- For the other five boards, all ten UWB pairs produced some valid reports:
  552 valid and 293 invalid reports overall (845 reports). This is evidence of
  UWB exchange among those nodes, not distance accuracy.
- IMU counts: HEAD 0; PELVIS 3,582; LEFT-WRIST 3,590; RIGHT-WRIST 3,078;
  LEFT-ANKLE 3,582; RIGHT-ANKLE 3,582. RIGHT-WRIST had 521 sequence gaps and
  20 failed BLE notification attempts. Its link was not stable during this
  capture. The other visible streams had 20–21 sequence gaps each.
- The gateway recorded no malformed frames and its USB-drop counter did not
  increase. Per-connection packet counters can reset when a BLE link reconnects,
  so their overall first-to-last difference is not meaningful in this run.

This repeat test was **not** a six-node pass. HEAD's connection was restored
in the following recovery check. The other boards and firmware were left
unchanged.

## HEAD recovery check — 30 September 2026

After HEAD was power-cycled, a 30-second capture confirmed that it rejoined:

- Binary capture: `../../outputs/records/acquisition/gateway_raw_2026-09-30_093040.bin`
- Decoded evidence: `../../outputs/records/acquisition/gateway_raw_2026-09-30_093040_decoded/`
- HEAD sent 1,799 IMU records (about 60 Hz) and had no sequence gaps in this
  short capture.
- Every one of the 15 UWB pairs produced at least one valid range. Overall,
  490 of 545 UWB reports were valid and 55 were invalid. This shows that all
  pairs can exchange range results in this setup; it does not establish their
  accuracy because the antenna spacing was not measured.
- RIGHT-WRIST sent 1,521 IMU records with 279 sequence gaps; RIGHT-ANKLE sent
  1,584 records with 208 gaps. Their node-status records were also less
  frequent than the other nodes, consistent with interrupted BLE links.
- PELVIS, LEFT-WRIST, and LEFT-ANKLE were near 60 Hz in this short sample.

HEAD is recovered, but six-node BLE continuity is not yet accepted. The next
step is to investigate the RIGHT-WRIST and RIGHT-ANKLE connection gaps without
changing their firmware or moving the other nodes.

## UWB report rate and failure accounting

The decoded UWB `range_cm` field is valid when it is non-negative and the
`quality_flags` valid bit is set. An invalid result is `range_cm = -1` with the
valid bit clear. That is a delivered BLE message reporting “no usable range”;
it is not a BLE message loss. In the 30-second recovery capture, all 545 UWB
records agreed between the range and valid flag.

The firmware publishes UWB results from PELVIS over BLE, one report at a time
with a pair index. It does not send one packet containing the whole matrix.
Each pair has an independent rate limit: valid reports are capped at one every
500 ms (2 Hz), and invalid reports at one every 1,000 ms (1 Hz). The report
timestamp is PELVIS uptime in milliseconds represented in microseconds; it is
not wall-clock time. `pelvis_timestamp_us` allows the receiver to reconstruct
relative send times.

Observed in the 30-second recovery capture:

- 545 reports over a 29.942-second timestamp span: **18.2 reports/s total**.
- Each pair produced 34–40 reports, averaging about **1.1–1.3 reports/s per
  pair**. That is about **1.21 complete 15-pair sweeps/s**, not 2 full matrices
  per second.
- 490/545 reports were valid (**89.9%**); 55/545 were invalid (**10.1%**).
- Per-pair report outcomes:

| Pair | Valid | Invalid | Invalid rate |
|---|---:|---:|---:|
| HEAD–PELVIS | 30 | 4 | 11.8% |
| HEAD–LEFT-WRIST | 29 | 5 | 14.7% |
| HEAD–RIGHT-WRIST | 36 | 2 | 5.3% |
| HEAD–LEFT-ANKLE | 32 | 4 | 11.1% |
| HEAD–RIGHT-ANKLE | 37 | 3 | 7.5% |
| PELVIS–LEFT-WRIST | 30 | 6 | 16.7% |
| PELVIS–RIGHT-WRIST | 33 | 4 | 10.8% |
| PELVIS–LEFT-ANKLE | 31 | 4 | 11.4% |
| PELVIS–RIGHT-ANKLE | 34 | 1 | 2.9% |
| LEFT-WRIST–RIGHT-WRIST | 34 | 4 | 10.5% |
| LEFT-WRIST–LEFT-ANKLE | 33 | 4 | 10.8% |
| LEFT-WRIST–RIGHT-ANKLE | 36 | 1 | 2.7% |
| RIGHT-WRIST–LEFT-ANKLE | 29 | 5 | 14.7% |
| RIGHT-WRIST–RIGHT-ANKLE | 31 | 6 | 16.2% |
| LEFT-ANKLE–RIGHT-ANKLE | 35 | 2 | 5.4% |

These are rates among the **rate-limited BLE reports**, not among every
underlying radio attempt. PELVIS health counters cover every ranging result but
are currently aggregate across all pairs. In this same capture they increased
by 1,226 attempts: 821 valid, 405 invalid (**33.0% invalid**). Of the 405
invalid attempts, 272 were marked as result timeouts (**67.2%**). There were
zero added BLE UWB-notification failures. The different 10.1% report-invalid
rate and 33.0% attempt-invalid rate are expected because the firmware
rate-limits the reports, especially invalid reports.

## What the invalid-result data does and does not identify

One cause is confirmed in the earlier 60-second grouped-board capture: HEAD had
no IMU or health packets, and all 236 reports for its five pairs were invalid.
After HEAD was restarted, all 15 pairs returned valid ranges at least once.
This links the earlier HEAD-pair failures to HEAD being offline for that run.

In the later recovery capture, the 272 `uwb_result_timeout` increments mean
PELVIS did not receive the expected responder `RESULT` frame within the
firmware's 40 ms collection window. This is evidence of a ranging-exchange
timeout, but it does not identify whether the cause was a missed command,
POLL/RESP/FINAL, radio interference, node timing, or a responder-side failure.
The other 133 invalid attempts are also not classified by pair or stage.
Current BLE reports carry only valid/invalid, not a failure reason. Therefore
the exact causes and per-pair underlying attempt rates are **not yet known**.

The next improvement is an event-only diagnostic record for failed ranging
stages. Source changes now send the pair, node, failure stage, local timestamp,
and driver/detail value to the gateway, then save them as `uwb_diag.ndjson`.
All six identity-specific wearable builds and the gateway build compile, and
the decoder self-test passes on a synthetic timeout event. No boards were
flashed. Build hashes and remaining checks are recorded in
`../../outputs/records/firmware/2026-09-30_uwb_failure_diagnostics_source_build/README.md`.

### UWB timing and what “1.2 matrices/s” means

The observed 1.21 full-matrix sweeps/s means one complete set of 15 links is
refreshed about every 0.83 s. Each individual pair therefore gets a new range
about every 0.8–0.9 s. That is too slow for UWB alone to follow quick body
movement. The six IMUs still provide the fast 60 Hz motion stream; UWB should
act as a slower distance correction unless its schedule is improved.

This is a firmware schedule limit, not a fixed DWM3001CDK hardware update
limit. The current PELVIS coordinator runs the 15 links sequentially and waits
up to 40 ms for each remote result. Qorvo's guidance says refresh depends on
block duration, slot duration, and slots per round; its example uses a
different ranging topology, so its quoted rate cannot be directly promised for
our 15-link schedule. A human upper-limb IMU/UWB study reports about 10 Hz UWB
with a faster IMU stream; this is a useful comparison, not a required standard.
Sources: [Qorvo guidance on DWM3001 ranging refresh](https://forum.qorvo.com/t/getting-fastest-distance-reading-on-dmw3001cdk/19322/2)
and [an IMU/UWB upper-limb motion-fusion study](https://pmc.ncbi.nlm.nih.gov/articles/PMC10422251/).

Recommended approach: keep the 60 Hz IMU stream, first obtain failure-stage
diagnostics, then benchmark the full-matrix schedule at staged targets (about
2 Hz, then 5 Hz if reliable). Do not claim 5 or 10 Hz until measured. If the
full 15-link mesh cannot meet a useful rate reliably, prioritize a smaller set
of anatomically useful links for live fusion and retain the other links for
slower diagnostics/calibration.

Failure-stage codes in the proposed source change are: `0x20` initiator POLL
transmit failure; `0x21` response wait failure (detail is DW status); `0x22`
bad response length; `0x23` wrong response source/type; `0x25` FINAL transmit
failure; `0x30` responder did not receive its POLL; `0x32` responder RESPONSE
transmit failure; `0x33` responder did not receive FINAL; `0x40` PELVIS timed
out waiting up to 40 ms for RESULT; `0x41` PELVIS could not send the range
command; `0x42` responder sent an invalid range; `0x43` responder's range
calculation was invalid. Success events are not sent. A timeout event narrows
down the failed stage but does not by itself identify RF interference versus
timing, packet loss, or a responder fault.

### Firmware artefacts

Wearable image SHA-256 values:

| Board | Image SHA-256 |
|---|---|
| HEAD | `607F529C64CF03BA747ABA6FEEB4D03FDE50521301CD60A1F88BA2DB53FA306F` |
| PELVIS | `E6622CA02B7428D0C282861428CCC649C247DAE2D881D90A53B398C5D3006864` |
| LEFT-WRIST | `CAD620E005E0973AF63ED193664284DF7E93780BD028219F7EC505470F85DF46` |
| RIGHT-WRIST | `68BB1B1E91564518FBA1E8B01C335C3092FFD9E77CF7BB0336245CEEAA940E13` |
| LEFT-ANKLE | `5F76C1A4EC7F1FDDA9C97570D647B5FF367B4D61A3ADAC816E949088353AF161` |
| RIGHT-ANKLE | `7486F6D2180FD069ACA30727C5292AC3104A217FC9002F3C78E02597B138F3BD` |

The flashed gateway package is
`../../outputs/records/firmware/2026-09-29_phase_a_transport_remediation/comp971_gateway_buffer_capacity_v5.zip`
(SHA-256 `9A0B3D5AD93B39291295E85EE322EAA6C2338B49992DBBEACCF76901451E0194`).

## Plan checkpoint

Firmware deployment, six-node live telemetry, the 15-pair UWB reporting path,
and repeatable binary capture/decode are complete as functional bring-up.
Continuous loss-free operation, automatic recovery of every node, UWB
reliability/accuracy acceptance, clock synchronisation, and IMU
bias/orientation calibration remain open. Do not calibrate UWB from the
present grouped-board captures because their exact antenna spacing was not
measured.
