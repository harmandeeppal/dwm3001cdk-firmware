# Two-board human-motion hardware ranging test

Date: 30 August 2026

| Permanent device | Body position | J-Link serial | Temporary role |
|---|---|---:|---|
| DWM-01-HEAD | Head | 760225018 | Range initiator; reports distance to laptop |
| DWM-02-PELVIS | Pelvis | 760224787 | Range responder; standalone power |

Both firmware environments compiled successfully from
`hardware/firmware/dwm3001cdk_motion_node`.

SHA-256:

- `DWM-01-HEAD__range-initiator__760225018.hex`:
  `0FEAEF6FC2102BF950346CEC7DA9D500E8E660C8B936ADF3F5A5F3DB04A46E0E`
- `DWM-02-PELVIS__range-responder__760224787.hex`:
  `FBFD10601B2C20F0469FC3868CDFFD5E5941FCB6BF67DFE7B34135340B18B64E`

The validated symmetric pair settings are:

- TX antenna delay: `16378` on both devices
- RX antenna delay: `16378` on both devices
- responder delay: `1000 UWB microseconds`

## Multi-distance calibration result

Each result is a separate 10-second capture with fixed orientation and clear
line of sight.

| True distance | Median | Median error | Raw standard deviation | Samples |
|---:|---:|---:|---:|---:|
| 50 cm | 48 cm | -2 cm | 4.53 cm | 1188 |
| 75 cm | 76 cm | +1 cm | 3.97 cm | 1246 |
| 150 cm | 151 cm | +1 cm | 4.32 cm | 1247 |

The antenna-delay offset passes the project limit of 5 cm absolute median
error at all three distances. Raw noise is near the 4 cm target and must be
timestamped and filtered when attached to the configured 60 Hz fusion stream.
This is a line-of-sight pair calibration; body-worn and obstructed-path tests
remain separate validation work.

Earlier artifacts using the temporary names `DWM-RANGER` and `DWM-ANCHOR`
are retained only under `archive/pre_permanent_identity_names/`.
