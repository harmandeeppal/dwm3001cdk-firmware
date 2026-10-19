# Left-Wrist standard-firmware restore

**Date:** 29 September 2026  
**Board:** DWM-03-LEFT-WRIST, J-Link serial `760225868`

## Action

Left-Wrist was restored to the shared standard source used for the 29 September
Pelvis and Head recoveries. Its intentional compiled difference is node 2 and
the permanent name `DWM-03-LEFT-WRIST`.

`nrfjprog` completed sector erase, program, verification, and reset.

## Image evidence

- Source image: `C:\tmp\comp971_ds_txrf_ble_advertising_retry_left_wrist_build_20260929_v1\zephyr\zephyr\zephyr.hex`
- Size: 414,624 bytes
- SHA-256: `C343A25E63037EF136E500165BC09D566BD14C9C103BF697E56F015B688D4DD9`

## Next check

Run battery-powered Pelvis–Left-Wrist UWB captures at 50, 75, and 100 cm.
Use the generic capture script with pair index 5; it now records the correct
pair name and unique timestamp.
