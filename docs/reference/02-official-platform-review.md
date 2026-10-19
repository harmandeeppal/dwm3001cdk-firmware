# Official platform review before firmware deployment

Review date: 2026-09-25. This review covers the official platform material
relevant to the next hardware action. It does not turn an unverified feature
into a verified one.

## Sources reviewed

- [Qorvo DWM3001CDK product and Quick Start Guide index](https://www.qorvo.com/products/ek/DWM3001CDK)
- [Qorvo DWM3001C product page and document index](https://www.qorvo.com/products/p/DWM3001C)
- [Qorvo DW3110 product page and document index](https://www.qorvo.com/products/p/DW3110)
- [Nordic nRF52833 Product Specification](https://docs.nordicsemi.com/r/bundle/ps_nrf52833)
- [Zephyr UART documentation](https://docs.zephyrproject.org/latest/hardware/peripherals/uart.html)

The Qorvo product pages additionally identify the current relevant Qorvo
documents: the DWM3001CDK Quick Start Guide (08/2024), DWM3001C Product Data
Sheet (10/2025), DW3000 API Software and API Guide (08/2022), Production Tests
for DW3000-Based Products (APS312, 06/2024), Antenna Delay Calibration (APS014,
2024), and TWR error sources (APS011, 2024). Where a document download could
not be text-read through the browser, this review relies only on facts exposed
by Qorvo's official product/document indexes.

## Confirmed facts and their project consequences

| Official fact | Consequence for this project |
|---|---|
| The DWM3001CDK is based on the DWM3001C/DW3110 and has on-board J-Link SWD/UART plus a separate DWM3001C USB interface. | J-Link serial is an appropriate physical board key; a Windows COM port is not. J-Link and application UART checks remain distinct. |
| The platform supports UWB channels 5 and 9, 850 kbps and 6.8 Mbps, and TWR/TDoA use cases. | `channel`, `data_rate`, preamble, timing, and ranging topology must stay JSON-owned. Hardware support for TWR does **not** provide a six-node schedule automatically. |
| DWM3001C integrates an nRF52833 and an on-board LIS2DH12 motion sensor. | The external ICM-20948 work is a separate bench diagnostic, not a property of every wearable board. Never report ICM telemetry unless that physical sensor is connected and detected. |
| DW3110 has one antenna port and supports TWR/TDoA; Qorvo publishes production-test, antenna-delay, and TWR-error guidance. | Every device needs its own recorded calibration status. The historic DWM-01/DWM-02 values cannot transfer to the re-identified DWM-05 board. |
| nRF52833 provides 512 KB flash, 128 KB RAM, UART/UARTE with EasyDMA, SPI, TWI, and USB. | A small framed-telemetry node is feasible. This does not justify unmeasured data rate, CPU, radio-airtime, or battery-life claims. |
| Zephyr distinguishes polling, interrupt-driven, and asynchronous UART APIs; interrupt-driven and asynchronous APIs must not be mixed on one UART. Polling TX blocks. | The current polling COBS transmitter is acceptable only for a small diagnostic. The six-node collector firmware must use one chosen buffered UART model and prove it does not compromise timing. |

## Design decisions resulting from the review

1. **First firmware action:** a one-board, configuration-derived identity and
   DW3110 health probe. It will not depend on the external ICM-20948, UWB
   ranging peer, or an unverified COM-to-board mapping.
2. **Before every flash:** record the J-Link serial selected from the registry,
   preserve/recover the prior image where technically possible, store the build
   configuration hash, and emit the permanent configured identity at startup.
3. **Six-node implementation:** use a scheduled TWR state machine. Every link
   needs a unique timeslot, timeout, peer ID, sequence number, range result,
   and validity/error reason. Do not let six boards independently transmit in
   the same time window.
4. **UWB calibration:** retain `unvalidated_per_device` until a controlled
   multi-distance procedure records fresh evidence for the current physical
   serials. Do not copy historical pair values to other devices.
5. **Telemetry:** include device identity, firmware/configuration hash, device
   sequence, device timestamp, IMU availability, and UWB validity. This lets
   later training and visualisation distinguish a missing measurement from a
   numerical zero.

## Explicitly out of scope for the first probe

- a full six-node UWB matrix;
- an accuracy or calibration claim;
- external-ICM verification;
- a fabricated NLOS classification; and
- any firmware flash without a configuration-selected target.

## Pre-flash acceptance gate

The first flash may proceed only after the probe profile specifies a registered
device ID, derives its J-Link serial from `device_registry.json`, and validates
the selected radio/serial settings. Success requires a post-flash startup
identity frame and DW3110 health result from that exact board. A present COM
port alone is insufficient.

## Build-verification status

The configuration generator was executed for `DWM-01` and produced a header
with the current registry identity `DWM-01-HEAD`, J-Link serial `760225805`,
and a reproducible configuration SHA-256. Python syntax and all JSON/registry
checks passed. After removing the incompatible current-user Python path
injection—with a registry backup—the probe compiled successfully against the
official `decawave_dwm3001cdk` board definition. The unflashed binary and its
checksums are recorded in
[`../../outputs/diagnostics/dwm3001cdk_uwb_check/2026-09-25_single_node_probe_DWM-01/build_manifest.json`](../../outputs/diagnostics/dwm3001cdk_uwb_check/2026-09-25_single_node_probe_DWM-01/build_manifest.json).

The initial build was blocked because the installed Nordic Python was forcibly
given `C:\tmp\py312\Lib` and `C:\tmp\py312\DLLs` before its own standard
library, causing an incompatible `ctypes` extension import in Zephyr child
processes. The current-user registry value was backed up under
`outputs/environment_backups/` and removed; Nordic Python then correctly loaded
its own standard library and the build passed. No board interaction occurred;
the binary remains unflashed pending the next explicit deployment gate.
