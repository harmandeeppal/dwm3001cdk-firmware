# First custom build: exact acceptance rules

## Purpose

The first custom image will prove that one DWM3001CDK can operate from its
battery, advertise, connect to the existing nRF52840 gateway, and recover
advertising after a disconnect. It deliberately does **not** start UWB or the
external ICM-20948 yet.

## Why begin this small

It separates Bluetooth/FreeRTOS integration from UWB and sensor work. If it
fails, we know precisely which layer to inspect. It also avoids putting a
half-integrated UWB scheduler on a wearable board.

## Required behaviour

| Check | Passing result |
|---|---|
| Battery boot | Board boots on battery; no USB/J20 cable needed |
| Advertising | Gateway discovers the advertised `COMP971-MOTION-...` name |
| Service | Gateway finds the COMP971 service and telemetry characteristic |
| MTU | Negotiated ATT MTU is at least 209 bytes |
| Heartbeat | Gateway's one-byte `0xA5` heartbeat write is accepted |
| Telemetry | Board sends a short node-status notification once per second |
| Recovery | After gateway unplug/replug, the board reconnects without a board restart |
| Safety | Flash/UICR backup, J-Link verify, and a post-reset identity check all pass |

## Explicit non-goals

- No UWB range claims.
- No UWB antenna-delay or calibration writes.
- No internal-flash logging at runtime.
- No ICM stream yet.

## Promotion rule

The image will remain a one-board pilot until it passes the complete list
above. Only then will ICM-20948 acquisition at 60 Hz be added and validated.
