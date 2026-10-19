# PELVIS coordinator and LEFT-ANKLE responder

This is the current two-board development pair for the six-position wearable system.

| Device | J-Link serial | UWB role | ICM telemetry |
|---|---:|---|---|
| `DWM-02-PELVIS` | `760224787` | Initiator/coordinator; laptop-connected | External ICM-20948, 60 Hz |
| `DWM-05-LEFT-ANKLE` | `760225018` | Responder; battery-powered | External ICM-20948, 60 Hz |

Both images were programmed and verified on 2026-09-26. A live pair check confirmed distance reports on PELVIS and its ICM telemetry at approximately 60 Hz.

The antenna delays are the historical development values (`16378` TX/RX, responder delay `1000 us`). Full physical-mount calibration is deliberately pending until all six boards are configured.
