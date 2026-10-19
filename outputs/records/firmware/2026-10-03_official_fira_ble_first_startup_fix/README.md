# Official-FiRa BLE-first startup fix

## Status

Built and safety-checked on 3 October 2026. Both PELVIS and HEAD images have
been installed and independently verified.

## Why this small change exists

The prior RAM-guard pilot proved that its build and flash verification were
correct, but the two-board battery test produced no BLE telemetry. Therefore,
it did not produce a valid UWB result either.

The earlier startup order began official UWB setup before BLE. If UWB setup
waited indefinitely, BLE could never advertise or provide diagnostics. This
pilot changes only the order:

1. Start BLE and the normal IMU task.
2. Wait two seconds in the already-existing status task.
3. Start the same official FiRa UWB setup.

This is a diagnostic improvement, not a claim that UWB is fixed. If official
UWB setup stalls, BLE and IMU telemetry should still arrive at the gateway and
show that clearly.

## Safety boundaries

- No UICR, OTP, antenna delay, or radio-calibration values are written.
- The RAM-only persistence guard remains enabled. The candidate application
  does not link Nordic NVMC, SoftDevice flash erase/write, or Qorvo persistent
  `qflash_write` routines.
- No extra UWB FreeRTOS task was added. The SDK has a 4 KiB dynamic-task heap;
  reusing the existing status task avoids making its task allocation less safe.
- The normal flash step, if approved, replaces only the defined application
  pages in the verified HEX image. It is not a chip erase.

## Candidate images

| Board | J-Link serial | HEX SHA-256 |
|---|---:|---|
| PELVIS | `760224787` | `C6BAA64914121E3A87823DFBC5E413FBC0BFB6C961CAE1D0DAD2F6C33F1C9411` |
| HEAD | `760225805` | `AFB8187480D48DA9E788E0C14924962C0A6AB2E2117C62F0EC234185A1E6DD9F` |

## Required verification sequence

1. Take two matching full-flash reads and two matching UICR reads before each
   board is flashed.
2. Flash exactly the approved board and image.
3. Read all internal flash again and compare every HEX-defined byte.
4. Read UICR again and prove it is unchanged.
5. Test PELVIS and HEAD on battery, one metre apart, with the BLE dongle.

PELVIS backup step 1 is complete at:

`hardware/backups/DWM3001CDK_760224787/2026-10-03_pre_ble_first_startup_fix/`

## PELVIS installation result

The approved PELVIS image was installed through J9 using J-Link serial
`760224787`.

| Check | Result |
|---|---|
| J-Link program and verify | Passed |
| Normal application flash range reported by J-Link | One range, 225,280 bytes |
| Chip erase used | No |
| Defined HEX bytes checked against independent 512 KiB readback | 336,240 bytes |
| Defined-byte mismatches | 0 |
| Readback SHA-256 | `5825E84F0BF47EAE74E6A2B472C6100D6084F00A2A462DE127502933547AB119` |
| UICR unchanged from the pre-flash read | Yes |
| Post-flash UICR SHA-256 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |

## HEAD installation result

The approved HEAD image was installed through J9 using J-Link serial
`760225805`.

| Check | Result |
|---|---|
| J-Link program and verify | Passed |
| Normal application flash ranges reported by J-Link | Four ranges, 192,512 bytes total |
| Chip erase used | No |
| Defined HEX bytes checked against independent 512 KiB readback | 335,104 bytes |
| Defined-byte mismatches | 0 |
| Readback SHA-256 | `4D6E769C3D6D49E9D87C8F32AAFC2FBC670E3DDAADD1C3B9659A9D3C413327DD` |
| UICR unchanged from the pre-flash read | Yes |
| Post-flash UICR SHA-256 | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |

The exact J-Link program and readback command files, along with the full
readback files, are in this folder. The next step is the battery test: PELVIS
and HEAD one metre apart with the BLE gateway connected.

## First battery test — not accepted

PELVIS and HEAD were powered by battery, placed at an operator-stated 50 cm,
and observed for 59 seconds through the nRF52840 gateway on COM16. The gateway
received **zero packets**: no BLE status, no IMU data, no UWB setup event, and
no range result from either board. Therefore this is not a UWB-distance result
and must not be interpreted as a 50 cm ranging failure. The failure occurs
before a usable BLE telemetry link exists.

The next action is read-only J-Link debugging of one board to locate its
startup state; no repeat test, calibration attempt, or further flash is
justified until that is understood.

## Correction to an earlier engineering assumption

The official DWM3001CDK QM33SDK FreeRTOS configuration sets
`configTOTAL_HEAP_SIZE` to **50 KiB**. The previously stated 4 KiB
dynamic-task heap was incorrect. The source review and subsequent task design
will be corrected accordingly; this does not change either already-installed
board.
