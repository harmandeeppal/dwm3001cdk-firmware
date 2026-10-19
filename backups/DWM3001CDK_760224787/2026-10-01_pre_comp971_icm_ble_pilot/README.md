# PELVIS current official-UCI recovery backup

This is a read-only recovery copy of the exact PELVIS state immediately before
the COMP971 official-Qorvo BLE and ICM pilot.

| Item | Value |
| --- | --- |
| Board | DWM-02-PELVIS |
| J-Link serial | 760224787 |
| Current firmware | Official Qorvo QM33 SDK UCI baseline |
| Flash reads | Two full 512 KiB reads, compared after acquisition |
| UICR | One 4 KiB read-only copy |
| Not performed | Programming, erase, UICR write, OTP write, calibration write |

## Acquisition result

Completed on 2026-10-01. Both full-flash reads match exactly.

| Copy | SHA-256 |
| --- | --- |
| Flash read A | `36264901CE4817052702D0A828346D33B98136F1E593F042F75044ABC85BF913` |
| Flash read B | `36264901CE4817052702D0A828346D33B98136F1E593F042F75044ABC85BF913` |
| UICR | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |

The board was reset and run after the read. This backup can restore the exact
official-UCI state that preceded the future COMP971 PELVIS pilot.
