# Pelvis standard-firmware restore

**Date:** 29 September 2026  
**Board:** DWM-02-PELVIS, J-Link serial `760224787`

## Action

The temporary Pelvis-only UWB diagnostic image was replaced with the compatible
standard recovery image. The operation used `nrfjprog` with sector erase,
program, verify, and reset, all of which completed successfully.

## Image evidence

- Source image: `C:\tmp\comp971_ds_txrf_ble_advertising_retry_pelvis_build_20260929_v1\zephyr\zephyr\zephyr.hex`
- Size: 406,360 bytes
- SHA-256: `851F176748B38A1A17C2B597CC7E1523F7F92BBE65343BC51D3D69342CD8AE55`

## Important scope

This restores compatible telemetry format to Pelvis. It is not yet the final
six-node release.

## Battery-powered verification

After USB was removed and Pelvis was powered by its battery, the gateway on
COM16 received Pelvis six-axis IMU telemetry at 60 Hz. Accelerometer Z was
approximately 16,000, so the earlier `32767` saturation did not recur. Head
also transmitted at 60 Hz in the same live-viewer session. No UWB range is
claimed from this verification; controlled UWB testing remains next.
