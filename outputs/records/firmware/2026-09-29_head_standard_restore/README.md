# Head standard-firmware restore

**Date:** 29 September 2026  
**Board:** DWM-01-HEAD, J-Link serial `760225805`

## Action

The Head was restored to the matching standard recovery build after the
Pelvis-Head 100 cm UWB test returned only invalid attempts. The source used for
this Head build is identical to the restored Pelvis source; the intended
difference is the compiled node identity only.

`nrfjprog` completed sector erase, program, verification, and reset without
error.

## Image evidence

- Source image: `C:\tmp\comp971_head_recovery_build_20260929_v1\zephyr\zephyr\zephyr.hex`
- Size: 414,595 bytes
- SHA-256: `FA6EE676F38DAB8004507F2BABD8663B48F39F204460876CA310071FCB5D5F0D`

## Next check

Run the same battery-powered Pelvis-Head 100 cm UWB test. A valid range is
required before any accuracy or calibration conclusion is made.
