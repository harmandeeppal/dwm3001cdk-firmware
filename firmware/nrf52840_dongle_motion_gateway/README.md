# nRF52840 Dongle motion gateway

This firmware is the BLE central for up to six `COMP971-MOTION-*` wearable
nodes. It performs ATT MTU exchange, refuses an insufficient MTU, discovers
the telemetry characteristic, subscribes to notifications, and sends framed
binary records over its USB CDC port.

For every received BLE IMU batch, it emits one USB record per IMU sample. It
does not keep only the newest sample. It also emits UWB, wearable status, and
gateway status records so losses are measurable.

The laptop capture and decoder are:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ".\hardware\tools\capture_gateway_raw.ps1" -Port COM16 -DurationSeconds 60
python .\hardware\tools\decode_gateway_capture.py ".\hardware\outputs\records\acquisition\gateway_raw_YYYY-MM-DD_HHMMSS.bin"
```

See `../../docs/TRANSPORT_REMEDIATION_PHASE_A.md` for the binary record
format, acceptance tests, and the work that remains before research data
collection.

Build target: `nrf52840dongle/nrf52840`.
