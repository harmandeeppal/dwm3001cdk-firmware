# Direct official FiRa startup-order correction

## Result that caused this correction

The first direct-FiRa pilot was flashed to PELVIS and HEAD and tested for 59
seconds. Both external ICM streams and BLE transport were live, but both nodes
reported `UWB not started`; PELVIS reported zero UWB attempts, results and
diagnostics. Therefore no radio ranging transaction had begun.

This is a firmware start-order issue, not a distance or antenna-calibration
failure.

## Cause

The first direct-FiRa implementation called Qorvo's
`fira_uwb_mcps_init()` in a FreeRTOS task after the Nordic SoftDevice/BLE stack
had started.

Qorvo's own DWM3001CDK QANI main program calls `persistent_time_init(0)` and
`fira_uwb_mcps_init()` before its BLE initialisation. The Qorvo source comment
states that this is required to handle UWB calibration and flash transactions
before the SoftDevice is up.

## Correction

The corrected application now:

1. calls `persistent_time_init(0)` before BLE initialisation;
2. calls Qorvo's `fira_uwb_mcps_init()` before BLE/SoftDevice initialisation;
3. starts the direct FiRa session later, after FreeRTOS begins.

The direct FiRa session and the radio settings are otherwise unchanged from
the first pilot. The change does not write or alter calibration values. It
only corrects the order in which the official UWB software is started.

## Build verification

| Board | Image | SHA-256 | Result |
|---|---|---|---|
| PELVIS `760224787` | `DWM-02-PELVIS__direct-fira-controller-v2__760224787.hex` | `CD3FB55800384D8553E11936F01FD620A9CC6DABD96967F564D83355CF8EA0BE` | compiled successfully |
| HEAD `760225805` | `DWM-01-HEAD__direct-fira-responder-v2__760225805.hex` | `4BA5A0527E6FACD613E87DA7F57A7895D5A40743AA181E58862E1934E0AB8324` | compiled successfully |

Both corrected images have been programmed and verified through J9.

## Programming record

### PELVIS v2 — complete

- Board: DWM-02 PELVIS, J-Link serial `760224787`
- Image SHA-256 checked before programming:
  `CD3FB55800384D8553E11936F01FD620A9CC6DABD96967F564D83355CF8EA0BE`
- J-Link result: **Program & Verify succeeded**.
- Affected normal flash: one range at `0x00000000`, `225,280` bytes.
- No chip erase command, UICR write, OTP write, antenna-delay write,
  calibration write, NVM write or runtime-flash write was issued.

### HEAD v2 — complete

- Board: DWM-01 HEAD, J-Link serial `760225805`
- Image SHA-256 checked before programming:
  `4BA5A0527E6FACD613E87DA7F57A7895D5A40743AA181E58862E1934E0AB8324`
- J-Link result: **Program & Verify succeeded**.
- Affected normal application flash: two ranges totalling `221,184` bytes at
  `0x00000000`.
- No chip erase command, UICR write, OTP write, antenna-delay write,
  calibration write, NVM write or runtime-flash write was issued.

## Required next test

Run the same 100 cm two-board test with the nRF52840 gateway. The acceptance
condition is that PELVIS reports a non-zero UWB attempt count and pair 0
produces range reports.

Normal application programming is permitted only with explicit approval. It
must not use chip erase, UICR programming, OTP programming, or calibration
writes.
