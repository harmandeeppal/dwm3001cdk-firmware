# Transport remediation — Phase A

**Status: implemented in source and included in the higher-capacity firmware
flashed to the gateway and all six wearables on 30 September 2026.** The
combined firmware was exercised in a one-minute six-node capture. IMU transport
is substantially improved but still has a small number of wearable queue
drops; UWB remains unreliable. See the
[transport stability update](reference/historical_2026-09-29_transport_stability_update.md) for
measured results and image hashes.

## Purpose

Phase A stops the system from silently losing observations before later work on
clock synchronization, IMU physical configuration, and UWB calibration.

## Changes

1. **UWB result collection is deadline based.** PELVIS now keeps listening for
   a remote pair's `RESULT` until a 40 ms deadline. It no longer stops after
   seeing three frames such as `POLL`, `RESP`, and `FINAL`.
2. **The gateway keeps every IMU sample.** A BLE batch can contain one to three
   samples. The gateway now creates one USB record for each sample, rather than
   discarding all but the last sample.
3. **USB is binary and framed.** Each record is `[C9 71][version][type]
   [payload length, little endian][payload]`. USB record payloads preserve raw
   IMU, UWB, node-health, and gateway-health data.
4. **ATT MTU is enforced.** The maximum BLE IMU payload is 74 bytes; it needs
   ATT MTU 77 or greater. The gateway disconnects rather than subscribes to a
   smaller session. Both projects configure 100-byte L2CAP TX MTUs and 251-byte
   ACL payload buffers.
5. **Loss is counted.** Wearables report acquired samples, successful/failed
   queue puts, queue high-water mark, successful/failed IMU notification
   enqueues, failed UWB report enqueues, and UWB
   attempts/valid/invalid/result-timeout counts. The gateway
   reports bad packets, sequence gaps, USB queue drops, and USB queue
   high-water mark.
   The gateway uses interrupt-driven USB CDC transmission; it does not send a
   record one byte at a time. Its SoftDevice Controller RX/TX buffer counts are
   set to 12 for the six simultaneous BLE links.
6. **Wire format is locked at build time.** Assertions reject an image if a
   future change alters a 22-byte IMU sample, 74-byte maximum batch, 24-byte
   clock status, or 16-byte UWB message.

## Laptop commands after the Phase-A firmware is flashed

Do not run two serial tools on COM16 at the same time.

For a live on-screen view of the binary Phase-A data:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ".\hardware\tools\live_gateway_binary_viewer.ps1" -Port COM16
```

Use `Ctrl+C` to stop the viewer, or pass `-DurationSeconds 60` for a 60-second
recorded session that saves a summary and received UWB reports. Before every
recorded session it drains the gateway stream for 20 seconds. This removes old
buffered records; the countdown is not included in the tables, metrics, or
saved files. Use `-WarmupSeconds 0` only when that preparation delay is not
wanted. The old text-protocol
viewer has been removed; this binary Phase-A viewer is the single supported
live viewer. It shows session min/median/mean/sample-SD/max and valid/invalid
counts for each UWB pair, IMU rates and sequence gaps, wearable health
counters, UWB failure-stage counts, and gateway USB health. Its summary also
records min/mean/SD/max for each raw IMU axis, and separate CSV files retain
the UWB reports and failure-stage records. Statistics restart when the viewer
starts. Add `-Pair 5 -KnownDistanceCm 100` to show error metrics for a
controlled PELVIS–LEFT-WRIST test at a known distance.

The viewer establishes its counter baseline from the first fresh gateway status
record after the 20-second stream drain. This prevents USB drops accumulated
while no viewer was running from being misreported as current live drops.

To retain the complete raw dataset for one minute:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ".\hardware\tools\capture_gateway_raw.ps1" -Port COM16 -DurationSeconds 60
```

The capture tool first clears three seconds of records buffered before COM16
opened. This prevents old, closed-port data from being counted as gaps in the
new capture. No board restart is required.

Then decode and account for all sequence numbers:

```powershell
python .\hardware\tools\decode_gateway_capture.py ".\hardware\outputs\records\acquisition\gateway_raw_YYYY-MM-DD_HHMMSS.bin"
```

The decoder writes `imu.ndjson`, `uwb.ndjson`, `node_status.ndjson`,
`gateway_status.ndjson`, and `summary.json` beside the raw file. The summary
reports sequence gaps and malformed/skipped frames explicitly.

## Verification completed and still required

- Completed: flashed the gateway and all six matched wearable images, then
  recorded a 60-second all-node capture. It contained 21,491 IMU records, 62
  sequence gaps, no bad packets, no new BLE notification failures, and no
  increase in the gateway USB-drop counter.
- Still required: confirm negotiated ATT MTU ≥209 for nine-sample wearable
  batches; the current capture does not contain an MTU trace.
- Still required: a longer stationary IMU capture with zero unexplained loss.
- Still required: diagnose the UWB invalid results and pass a controlled
  15-link test before calibration.

**Not completed:** Phase B (boot/session identity and PELVIS clock mapping) and
Phase C (IMU physical ODR/filter verification and final UWB calibration). The
current streams still have `sync_epoch=0`; range calibration must wait until
known-distance testing is complete.
