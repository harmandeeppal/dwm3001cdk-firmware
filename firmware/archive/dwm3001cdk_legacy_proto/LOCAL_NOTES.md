# Human-motion DWM3001CDK wearable-node firmware

This project contains the archived two-board UWB hardware test for the
dissertation's six-position IMU+UWB human-motion system. It is not the
complete six-node motion-capture firmware, and its historical profile is
disabled after the old DWM-01 serial was re-identified as DWM-05 left ankle.

## Permanent identities and temporary test roles

| Permanent name | J-Link serial | Body position | Current test role | Laptop during range test |
|---|---:|---|---|---|
| DWM-01-HEAD | 760225018 | Head | Historic initiator identity only; serial is now DWM-05 left ankle | Historic |
| DWM-02-PELVIS | 760224787 | Pelvis | Responder | Standalone power |

Permanent identity is owned by:

- `../../config/body_positions.json`
- `../../config/device_registry.json`

Temporary roles and UWB settings are owned by:

- `../../config/profiles/two_board_range_test.json`

The PlatformIO environment names include both permanent identity and temporary
test role. Each firmware sends its permanent name over serial when it starts.
COM ports are last-seen details only; J-Link serial is the hardware identity.

The application was adapted from
`https://github.com/MytraAI/DIY-UWB-AI-SWAG` at commit
`becb8401fecede655b467c6128fc74dc14446cf8`. The bundled Qorvo driver retains
its original `LicenseRef-QORVO-2` license file.

The test uses single-sided two-way ranging (SS-TWR) on UWB channel 5. Antenna
delays currently use the uncalibrated example value `16390`. This can verify
radio ranging, but it is not yet research-grade hardware calibration.
