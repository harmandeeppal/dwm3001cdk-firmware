# Official QANI positive-result correction (v7)

## Status

PELVIS and HEAD v7 are programmed and read-back verified.

| Board | J-Link serial | Image | SHA-256 |
|---|---:|---|---|
| PELVIS | `760224787` | `DWM-02-PELVIS__official-qani-v7-positive-add-controlee__760224787.hex` | `254B944DC96142B14FF9980A21ADC12400EC7DE48D8931A26B70645AB464E0F8` |
| HEAD | `760225805` | `DWM-01-HEAD__official-qani-v7-positive-add-controlee__760225805.hex` | `A8681F512132C16AEFCD8AE78B1EBDE6722ED01CDE0CC59C8B595422519884DE` |

## Evidence and correction

The v6 PELVIS read-only SWD inspection reached Qorvo's official QANI function
`fira_niq_app_process_init()`. All earlier UWB setup stages completed. The
call that adds HEAD to the PELVIS FiRa session returned `8`.

Qorvo's installed API header, `fira_helper.h`, documents
`fira_helper_add_controlee()` as returning **zero or a positive value on
success**, and a negative value on error. The QANI source instead rejected
every non-zero value:

```c
if (err != QERR_SUCCESS) { return _ERR; }
```

Version 7 changes only this test to:

```c
if (err < QERR_SUCCESS) { return _ERR; }
```

Therefore the observed `8` is accepted as the documented successful result.
This change is confined to the Qorvo QANI application glue source. It does not
alter the Qorvo UWB radio driver, UWB-MAC library, FiRa parameters, antenna
delay, UICR, OTP, factory calibration, Bluetooth transport, or ICM sampling.

## Why PELVIS and HEAD both have v7 images

The corrected condition runs only on the PELVIS controller, which adds HEAD
to its session. HEAD has been built from the same source revision so the two
images have a recorded, matching firmware baseline; its fixed role and short
address remain the only intended functional difference.

## Next hardware operation

After explicit instruction to flash:

1. Program PELVIS on J9, serial `760224787`, with its v7 image using normal
   J-Link `loadfile` programming.
2. Program HEAD on J9, serial `760225805`, with its v7 image.
3. Read back the defined application bytes and compare them with each HEX
   image.
4. Power both on battery at 100 cm, with the gateway dongle connected, and
   run the unified viewer. Acceptance is a non-zero PELVIS UWB-attempt count
   and valid pair-0 (`HEAD <-> PELVIS`) reports.

Normal J-Link programming affects application flash pages only. This plan does
not include a chip erase, UICR/OTP write, antenna-delay/calibration write, or
runtime flash write.

## PELVIS v7 programming record

- Board / J-Link serial: `DWM-02-PELVIS` / `760224787`.
- Programmed: 2026-10-02 with
  `flash_pelvis_controller_v7_qani_positive_result.jlink`.
- J-Link completed normal program-and-verify successfully. It affected one
  application-flash range of `8,192` bytes; it did not use a chip erase.
- Read-back file: `pelvis_controller_v7_qani_postflash_0x00000000_0x00054fff.bin`.
- Image-defined bytes compared: `342,136`; mismatches: `0`.
- No UICR, OTP, antenna delay, calibration, or runtime flash data was written.

## HEAD v7 programming record

- Board / J-Link serial: `DWM-01-HEAD` / `760225805`.
- Programmed: 2026-10-02 with
  `flash_head_responder_v7_qani_positive_result.jlink`.
- J-Link completed normal program-and-verify successfully. It affected one
  application-flash range of `8,192` bytes; it did not use a chip erase.
- Read-back file: `head_responder_v7_qani_postflash_0x00000000_0x00054fff.bin`.
- Image-defined bytes compared: `341,288`; mismatches: `0`.
- No UICR, OTP, antenna delay, calibration, or runtime flash data was written.
