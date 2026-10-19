# Live hardware readiness

Status date: 2026-09-25. This record separates physical USB presence from
verified application telemetry. A device is not considered ready merely
because Windows creates a COM port for it.

## Observed state

- Six J-Link CDC UART ports were enumerated and healthy: `COM6`, `COM9`,
  `COM10`, `COM11`, `COM12`, and `COM13`.
- The six permanent J-Link identities have previously been observed together:
  `760225805`, `760224787`, `760225868`, `760225748`, `760225018`, and
  `760225934`.
- Short passive reads and one controlled reset/read did not yield complete
  COBS application frames, startup identity frames, IMU samples, or valid UWB
  range reports from any port.
- The controlled reset caused normal temporary COM-port re-enumeration. All
  six ports returned. No application firmware was flashed or modified.

## Honest readiness conclusion

Physical USB/J-Link connection is confirmed for all six boards. Live firmware
identity, IMU telemetry, UWB telemetry, and per-device antenna calibration are
not yet verified. The project must not claim a live six-node system.

## Configuration and implementation gates

The official-platform basis for these gates is recorded in
[`02-official-platform-review.md`](02-official-platform-review.md). In
particular, the first firmware action must be a configuration-derived,
single-board identity/DW3110 probe; the current two-board application is not a
valid substitute for six-node readiness.

The probe source and JSON generator now exist, and the first configuration-
derived `DWM-01` image has compiled successfully after repairing a local Nordic
Python-path injection. Its manifest is linked from the official-platform
review. No probe image has been flashed.

The configuration contract is
[`../../config/profiles/six_node_telemetry_v1.json`](../../config/profiles/six_node_telemetry_v1.json).
It is deliberately disabled because the current firmware implements only a
two-board initiator/responder exchange. The next implementation must provide:

1. a configuration-derived identity frame from every node;
2. a configuration-derived uploader target using the registered J-Link serial;
3. a per-node IMU stream at the configured rate;
4. a collision-free scheduled six-node UWB ranging protocol that records each
   pair's value and validity state; and
5. a collector that verifies identity before accepting telemetry, then stores
   configuration hashes and device sequence numbers with each capture.

COM ports are discovery details only. They must never be persisted as a board
identity or used as a firmware upload target.
