# DWM3001CDK protected backup index

These folders contain pre-flash readbacks and recovery artefacts for the six
physical boards. Their names are the J-Link serial numbers. They are protected
recovery evidence and must not be overwritten by build output.

| Board | Body position | J-Link serial | Backup folder |
|---|---|---:|---|
| DWM-01 | Head | 760225805 | `DWM3001CDK_760225805/` |
| DWM-02 | Pelvis | 760224787 | `DWM3001CDK_760224787/` |
| DWM-03 | Left wrist | 760225868 | `DWM3001CDK_760225868/` |
| DWM-04 | Right wrist | 760225748 | `DWM3001CDK_760225748/` |
| DWM-05 | Left ankle | 760225018 | `DWM3001CDK_760225018/` |
| DWM-06 | Right ankle | 760225934 | `DWM3001CDK_760225934/` |

Latest protected reads for the current official-SDK transition are stored at:

- HEAD: `DWM3001CDK_760225805/2026-10-01_pre_comp971_official_port_pilot/`
- PELVIS: `DWM3001CDK_760224787/2026-10-01_pre_official_uci_baseline/`
- LEFT-WRIST: `DWM3001CDK_760225868/2026-10-01_pre_official_uci_baseline/`

Each folder may contain a flash image (`.hex` or `.bin`), UICR/FICR readback,
and J-Link evidence. Multiple readbacks are deliberately retained where they
prove repeatability. These are not compiler artefacts and are not redundant.

Some saved `.jlink` command files contain the original pre-clean-up path. They
are historical evidence only; do not run them. Use a newly reviewed command
when making another backup.

To identify a connected board, use its J-Link serial rather than its COM port.
The authoritative mapping is `../config/device_registry.json`.
