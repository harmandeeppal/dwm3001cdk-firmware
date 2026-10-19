# Phase-A transport-remediation build record

## Scope

This record covers source-only Phase-A remediation. **No board or dongle was
flashed before 2026-09-29.** It is not yet a hardware acceptance result.

Implemented items:

- deadline-based collection of a remote DS-TWR result at PELVIS;
- binary, per-sample USB forwarding at the gateway;
- ATT MTU 77 minimum enforcement for the 74-byte maximum IMU batch;
- wearable and gateway transport-loss counters;
- compile-time protocol ABI assertions.

## Compile evidence

Both builds used the locally installed, official NCS 3.4.0 / Zephyr 4.4.0
toolchain and completed with exit code 0.

| Image | Target | SHA-256 | Build-only path |
|---|---|---|---|
| Gateway | `nrf52840dongle/nrf52840` | `38C5AC5E2A1474E0F53D114C59A3DD432FA5AC7F582CD5A6100216DE5F76EAF0` | `C:\tmp\comp971_gateway_phase_a_build_20260929_v6\zephyr\zephyr.hex` |
| HEAD | `nrf52833dk/nrf52833`, mesh index 0, `COMP971-MOTION-HEAD` | `D1C474F13C4D7482063C1175DC4EAA7387689ED563DDF002191847B846CE077A` | `C:\tmp\comp971_phase_a_head_build_20260929_v2\zephyr\zephyr\zephyr.hex` |
| PELVIS | `nrf52833dk/nrf52833`, mesh index 1, `COMP971-MOTION-PELVIS` | `651C7823A73A4F15B5454F77FAA0B64C85FE0A29DABB6F2FA0157C0FC0EBDC04` | `C:\tmp\comp971_phase_a_pelvis_build_20260929_v1\zephyr\zephyr\zephyr.hex` |
| LEFT-WRIST | `nrf52833dk/nrf52833`, mesh index 2, `COMP971-MOTION-LEFT-WRIST` | `CE060C9D96CBF7B8608F4DEE2772D530F5DD61264856B5859FDD0108C2A743E5` | `C:\tmp\comp971_phase_a_left_wrist_build_20260929_v1\zephyr\zephyr\zephyr.hex` |
| RIGHT-WRIST | `nrf52833dk/nrf52833`, mesh index 3, `COMP971-MOTION-RIGHT-WRIST` | `CB40531D43368D5AF1C4FA874EECFA08F269146FB897A2B7755CE050AE50FF90` | `C:\tmp\comp971_phase_a_right_wrist_build_20260929_v1\zephyr\zephyr\zephyr.hex` |
| LEFT-ANKLE | `nrf52833dk/nrf52833`, mesh index 4, `COMP971-MOTION-LEFT-ANKLE` | `A1F019E2B53E3D3D8D068532BED4DDAA41235E00E03868A97FBD12A061C19014` | `C:\tmp\comp971_phase_a_left_ankle_build_20260929_v1\zephyr\zephyr\zephyr.hex` |
| RIGHT-ANKLE | `nrf52833dk/nrf52833`, mesh index 5, `COMP971-MOTION-RIGHT-ANKLE` | `3C9A7CDAC23785D76D804B361141E43D78010AD2E15C27CFD4DDF590C4A0DB32` | `C:\tmp\comp971_phase_a_right_ankle_build_20260929_v1\zephyr\zephyr\zephyr.hex` |

All six wearable images were built from the same Phase-A source. They differ
only in their permanent Bluetooth name and UWB mesh index. This remains a
compile check, not a hardware acceptance result.

## Permanent identity review

| Role | DWM serial / J-Link ID | Bluetooth name | UWB mesh index |
|---|---:|---|---:|
| HEAD | 760225805 | `COMP971-MOTION-HEAD` | 0 |
| PELVIS | 760224787 | `COMP971-MOTION-PELVIS` | 1 |
| LEFT-WRIST | 760225868 | `COMP971-MOTION-LEFT-WRIST` | 2 |
| RIGHT-WRIST | 760225748 | `COMP971-MOTION-RIGHT-WRIST` | 3 |
| LEFT-ANKLE | 760225018 | `COMP971-MOTION-LEFT-ANKLE` | 4 |
| RIGHT-ANKLE | 760225934 | `COMP971-MOTION-RIGHT-ANKLE` | 5 |

## Flash log

| Time | Target | Result | Method |
|---|---|---|---|
| 2026-09-29 | DWM-01-HEAD, J-Link 760225805 | Phase-A HEAD image written and verified successfully; target reset | Nordic `nrfjprog --program --sectorerase --verify` |
| 2026-09-29 | nRF52840 Dongle gateway | Phase-A gateway installed successfully through the factory USB DFU bootloader. Normal binary stream enumerated on COM16. | Nordic `nrfutil nrf5sdk-tools dfu usb-serial` |
| 2026-09-29 | DWM-02-PELVIS, J-Link 760224787 | Phase-A PELVIS image written and verified successfully; target reset | Nordic `nrfjprog --program --sectorerase --verify` |
| 2026-09-29 | DWM-03-LEFT-WRIST, J-Link 760225868 | Phase-A LEFT-WRIST image written and verified successfully; target reset | Nordic `nrfjprog --program --sectorerase --verify` |
| 2026-09-29 | DWM-04-RIGHT-WRIST, J-Link 760225748 | Phase-A RIGHT-WRIST image written and verified successfully; target reset | Nordic `nrfjprog --program --sectorerase --verify` |
| 2026-09-29 | DWM-05-LEFT-ANKLE, J-Link 760225018 | Phase-A LEFT-ANKLE image written and verified successfully; target reset | Nordic `nrfjprog --program --sectorerase --verify` |
| 2026-09-29 | DWM-06-RIGHT-ANKLE, J-Link 760225934 | Phase-A RIGHT-ANKLE image written and verified successfully; target reset | Nordic `nrfjprog --program --sectorerase --verify` |
| 2026-09-29 | nRF52840 Dongle gateway, transport update attempt | Installed, but its unsupported async CDC method prevented the gateway from starting. No wearable firmware was changed. | Nordic `nrfutil nrf5sdk-tools dfu usb-serial` |
| 2026-09-29 | nRF52840 Dongle gateway, interrupt-CDC correction | Installed successfully. Uses the CDC driver’s supported interrupt TX method and increased SoftDevice Controller RX/TX capacity. | Nordic `nrfutil nrf5sdk-tools dfu usb-serial` |

## First transport observation

Immediately after the gateway installation, COM16 contained valid framed
protocol-v1 IMU records from mesh node 0 (HEAD). This confirms the first
automatic BLE connection and the binary USB stream. It does **not** yet verify
the five other wearable nodes, UWB operation, timing, or calibration.

After PELVIS was flashed, COM16 contained framed IMU/status records from both
mesh nodes 0 (HEAD) and 1 (PELVIS), without restarting HEAD. PELVIS also
emitted UWB result records for all 15 scheduled links. With only those two
wearables powered, results for absent nodes are expected to be invalid. This
proves scheduling and transport only; it is not a ranging-accuracy result.

After LEFT-WRIST was flashed, the gateway received valid IMU and health records
from nodes 0 (HEAD), 1 (PELVIS), and 2 (LEFT-WRIST) concurrently. This is the
second automatic BLE reconnection check.

After RIGHT-WRIST was flashed, the gateway received valid IMU records from
nodes 0 through 3 concurrently. This confirms another automatic BLE connection.

After LEFT-ANKLE was flashed, the gateway received valid IMU records from
nodes 0 through 4 concurrently. This confirms another automatic BLE connection.

After RIGHT-ANKLE was flashed, the gateway received valid IMU and health
records from all six nodes concurrently. This verifies the six-board BLE
automatic-connection path and binary USB transport. UWB range accuracy remains
an open hardware-verification item.

The first six-node live view revealed USB queue drops because the initial
gateway sent each USB byte through `uart_poll_out()`. The first attempted
gateway correction used an unsupported async CDC API and was replaced without
touching the wearable images. The installed correction uses the CDC driver's
interrupt TX API plus 12 controller RX and TX packets (previously two RX and
three TX). In a 12-second live test with COM16 open, the USB-drop counter did
not increase. The non-zero value reflects data produced before the laptop port
was opened. BLE sequence-gap stability is still a separate acceptance test.

### Latest six-node live observation

With all six wearables powered, the Phase-A viewer reported each IMU stream at
approximately 60 Hz and a recent valid raw range for every one of the 15 UWB
pairs.  After the viewer's three-second USB backlog-clearing period, it
reported `USB drops +0` and `bad +0` for the live session.  This confirms that
the gateway-to-laptop USB link keeps up while COM16 is actively read.

The same observation reported `sequence gaps +114` over 3,284 gateway packets.
Those are missing source samples before the laptop receives them, so this is
not yet an acquisition-acceptance result.  Investigate and reduce BLE sequence
gaps before relying on the streams for a training dataset.  The UWB values are
also raw and still require controlled-distance calibration.

## Basis for MTU enforcement

Zephyr documents that a GATT notification value can contain at most
`ATT_MTU - 3` bytes and that `bt_gatt_get_mtu()` reports the negotiated value.
The protocol assertion fixes the maximum IMU batch at 74 bytes, so the gateway
requires `ATT_MTU >= 77` before subscribing.

## Next verification

1. Review the six identity-specific wearable images against the permanent
   device registry.
2. Flash the gateway and all six wearable images only after review.
3. Run the documented one-minute raw capture, decode it, and check every node
   for sequence gaps, queue drops, notification failures, and MTU compliance.
4. Only then test all 15 UWB links and resume controlled range calibration.
