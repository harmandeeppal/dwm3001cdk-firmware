# Official UWB remediation plan

Date: 2 October 2026  
Status: active investigation; no new pilot image is approved for flashing.

## 1. Decision

Do not flash the prepared QANI pilot yet.

The QANI example is a real Qorvo DWM3001CDK example, but its documented use is a DWM3001CDK paired with an iPhone U1 device. It is not Qorvo's documented DWM3001CDK-to-DWM3001CDK wearable-mesh example. Using its internals for two boards would be an adaptation that still needs proof. It must not be described as a proven final solution.

The correct reference for a DWM3001CDK-to-DWM3001CDK range is the official UCI firmware and PC host workflow. This project has already obtained good Channel 9 two-board UCI results, so the radio hardware and Qorvo's UWB stack are proven separately from COMP971's BLE/IMU application.

## 2. Official evidence reviewed

| Official source | Fact used in this plan |
|---|---|
| Qorvo DWM3001CDK Quick Start Guide | The CDK contains an on-board J-Link programmer; J9 is the board USB used for flashing. Qorvo identifies UCI firmware as the firmware to flash for evaluation. It also warns that an NVM/chip erase requires calibration/configuration to be reapplied. |
| Qorvo DWM3001C Data Sheet, Rev F | DWM3001C contains DW3110 UWB and nRF52833 BLE. It is factory calibrated; antenna delay is important to ranging and can be stored in OTP. The board's antenna orientation and multipath can affect accuracy. |
| Qorvo QM33 SDK 1.1.1 root README | The SDK supplies separate UCI and QANI projects for DWM3001CDK and specifies `nrf52833_xxaa` for the CDK. |
| Qorvo QM33 SDK 1.1.1 QANI README/source | QANI is documented as an iPhone Nearby Interaction example. Its UWB lifecycle is valuable reference code, but it does not establish a two-CDK final architecture. |
| Existing official UCI records | PELVIS-to-LEFT-WRIST Channel 9 UCI ranges were stable: about 52 cm at 50 cm, 74 cm at 75 cm, and 106-108 cm at 100 cm. This proves the boards and official radio configuration can range. |

## 3. What earlier results really mean

| Result | Meaning | Does it prove the final BLE wearable design? |
|---|---|---|
| Official UCI range measurements | Radio boards, their factory calibration, and Qorvo's official UWB stack work. | No. UCI has a PC host controlling the session through J20. |
| COMP971 IMU + BLE telemetry | ICM-20948 reads and BLE gateway transport work. | No. It does not start UWB by itself. |
| Direct-FiRa builds v1-v4 | PELVIS showed zero UWB attempts. The application-owned start path did not reach an active FiRa session. | No. They are failed UWB-start experiments. |
| QANI-derived pilots v5-v7 | PELVIS and HEAD were programmed and read-back verified. Read-only debugging reached Qorvo's `fira_niq_app_process_init()` and found a bad success test around `fira_helper_add_controlee()`. | No. QANI is documented for an iPhone peer and is not the clean DWM-to-DWM reference for the final wearable design. |

The available gateway summaries from 2 October provide one unambiguous result:
seven PELVIS--HEAD sessions (30--60 seconds each) reported **zero** PELVIS
UWB attempts, valid ranges, invalid ranges, pair-0 reports, and radio-stage
failure reports. Both IMU streams remained live. This eliminates geometry,
battery distance, J20, radio accuracy, and BLE receipt as explanations for
these sessions: the ranging state machine is not reaching its first attempt.

### Firmware-identity finding - 2 October 2026

A fresh read-only SWD read of PELVIS serial `760224787` captured application
flash `0x00000000`--`0x00054FFF` to
`outputs/records/diagnostics/2026-10-02_pelvis_qani_read_only_watch/`
(`SHA-256 7DB033A50347081921531CE3800376159D2F973A9A525B5ADFFF930F8B488D40`).
It does **not** match the recorded v7 QANI post-flash read-back. There are
220,845 differing bytes; the first difference is at `0x0001C004`.

Therefore the application actually running on PELVIS is not identifiable from
the recorded v7 source/image record. A brief read-only debug attachment also
observed an instruction `BKPT #0` at `0x0004D560` in this unknown image. This
may be an assertion/debug trap, but without a matching ELF file it cannot be
attributed to a source line. Do not infer a radio fault from it.

This is a firmware configuration-control failure. The immediate rule is: no
new range result is accepted unless the board read-back is compared with the
exact named image that produced it.

## 4. Safety rules

1. Keep existing board backups. Do not use chip erase.
2. Never write antenna delay, OTP, UICR, or runtime calibration values during development.
3. A normal J-Link application `loadfile`, reset, and run is the only approved flash operation after explicit board-by-board approval.
4. Keep all temporary UWB session settings in RAM. A compile definition alone is **not** enough because the supplied Qorvo UWB stack is prebuilt. The COMP971 official-FiRa candidate now replaces only the SDK application's `qflash_write()` implementation with a RAM-only equivalent. The final PELVIS and HEAD images were disassembled: their `qflash_write()` increments a RAM audit counter and returns success; no `nrf_nvmc_page_erase`, `nrf_nvmc_words_write`, `sd_flash_page_erase`, or `sd_flash_write` symbol is linked. Qorvo radio, UWB-MAC, FiRa, BLE, and board code are unchanged.
5. Do not use UWB results for motion modelling until a controlled distance test passes the defined quality gate.

## 5. Correct technical approach

The final firmware cannot be entirely stock UCI because it must also sample the external ICM-20948, schedule six body nodes, and send data to the BLE gateway. That application logic is necessarily COMP971-specific.

The lower UWB layer must remain Qorvo-owned:

```
COMP971 high-level code
  - fixed body identity
  - BLE packets
  - IMU sampling
  - explicit range schedule
          |
          v
Qorvo official FiRa / UWB-MAC / DW3110 driver
  - session setup
  - UWB timing and ranging frames
  - factory radio values read from OTP
          |
          v
DWM3001C hardware
```

The next implementation must be based on the official UCI DWM-to-DWM control flow, not on an iPhone-oriented QANI example and not on a new hand-written low-level radio task.

## 6. Staged plan and stop conditions

### Stage A - freeze and audit (now)

- Record v1-v4 as failed direct-FiRa start tests.
- Separate the untouched official source reference from COMP971 adaptation code. Do not make further edits inside the installed SDK without recording the exact change.
- Map the official UCI host commands to the SDK's UCI session-control source and identify the smallest DWM-to-DWM controller/responder path.

The first source audit is complete. The official UCI application owns a UCI server, FiRa backend, session manager, and coordinator. Its host-driven session path is separate from the SDK's `fira_app.c`, which contains the official controller/responder sequence: initialise UWB-MAC, open FiRa helper, configure scheduler and measurement sequence, initialise the session, set parameters, add the controlee, start UWB-MAC, then start the session. The range callback is `FIRA_HELPER_CB_TYPE_TWR_RANGE_NTF`.

The audit also found a decisive limitation in the earlier QANI proposal: QANI is an iPhone Nearby Interaction example and its `fira_niq.c` in the local SDK has already been adapted for COMP971 hooks. QANI-derived versions v5-v7 were programmed and v7 reached its add-controlee step; the code had incorrectly treated the documented positive return value `8` as an error. That finding is useful, but QANI is still not a clean DWM-to-DWM reference and will not be the basis of the final mesh implementation.

### Stage A addendum - QANI startup-contract check (2 October 2026)

A source comparison with Qorvo's QANI `main.c` found one specific missing
startup call in the COMP971 project: Qorvo calls `niq_init(StartUWB, StopUWB,
crypto_init, crypto_deinit, crypto_get_random_vector)` before
`CreateQaniTask()`. The COMP971 adapter had created the task but had not made
that callback registration. A source-only PELVIS/HEAD candidate containing
that call was compiled successfully and recorded at
`outputs/records/firmware/2026-10-02_qani_niq_startup_contract_build/`.

This candidate was a controlled startup diagnostic, not a final mesh design
and not a reason to change antenna calibration. It was subsequently flashed
only after fresh, matching full-flash/UICR backups and explicit approval for
each board. Independent post-flash reads matched every defined image byte.

The battery test at stated 100 cm did **not** pass: both boards reported `UWB
started` and sent about 58.8 Hz IMU data, but PELVIS recorded zero UWB
attempts, zero valid results, zero invalid results, and zero UWB diagnostic
events. The evidence is recorded in
`outputs/records/firmware/2026-10-02_qani_niq_startup_contract_build/` and
`outputs/records/acquisition/live_gateway_viewer/session_20261002_092405/`.
This means the QANI session accepted a start request but never scheduled a
radio transaction. QANI will not receive further timing/configuration patches.

The next pilot will use the official `fira_app.c` lifecycle as the reference.
The COMP971 adapter will call the same documented FiRa/UWB-MAC APIs from a
clean, locally-owned application layer and report every lifecycle stage. In
particular, it will check `fira_prepare_measurement_sequence(...)`, which the
normal Qorvo application calls before session creation and which QANI calls
without checking. It must not copy an undocumented QANI start signal or create
radio frames itself. The exact API-level comparison with v1-v4 is a required
review item before any image is built.

Before building that candidate, use a read-only SWD inspection of the
identified PELVIS image to determine the last reached function: `StartUWB`,
`qani_task`, `fira_niq_app_process_init`, `uwbmac_start`, or
`fira_helper_start_session`. The first inspection found that the current image
is not v7, so this step must be repeated only after a named, verified image is
installed. It only reads execution state and RAM; it does not erase, program,
reset, or modify calibration data.

This is the precise official path that the next minimal wearable pilot must expose. It will not use QANI as its primary reference.

**Pass condition:** a source-level map states exactly who creates a session, who starts it, and which callback produces a range.  
**Stop condition:** if the path requires undocumented assumptions, return to official UCI before any new wearable flash.

### Stage B - repeatable official two-board baseline

- Use official UCI firmware on PELVIS and HEAD.
- Use J20 only for the official PC-host UCI test; it is not part of battery operation.
- Test at 50 cm, 75 cm, and 100 cm on Channel 9 with both antennas facing each other.
- Save raw results and the exact official configuration used.

**Pass condition:** high valid-report rate and low variation similar to the earlier official UCI measurements.  
**Stop condition:** if this fails, investigate physical orientation, battery/case effects, or board configuration before changing application code.

### Stage C - minimal official-stack wearable pilot

- Start from the documented UCI DWM-to-DWM session flow.
- Add only two high-level adapters: fixed PELVIS/HEAD identities and a callback that sends the official range result over existing BLE.
- No IMU batching and no six-node mesh at this stage.
- Add explicit startup-stage diagnostics so a failure says whether it occurred at stack initialisation, session creation, session start, or range callback.

**Pass condition:** PELVIS reports non-zero attempts and valid `HEAD <-> PELVIS` ranges over BLE at a stated 100 cm.  
**Stop condition:** any zero-attempt result is a startup/control-path problem; do not change antenna calibration or add timing guesses.

### Stage D - combine IMU and two-board UWB

- Enable 60 Hz IMU telemetry alongside the proven two-board UWB path.
- Measure BLE packet loss, IMU sequence gaps, UWB attempt/valid/invalid counts, and range distribution in one viewer/capture format.

**Pass condition:** IMU remains near 60 Hz, UWB attempts continue, and BLE does not create new UWB failure.

### Stage E - six-node, 15-pair schedule

- Add one pair at a time to an explicit time-division schedule.
- Keep the official ranging transaction duration intact. A faster full matrix is not achieved by shortening radio round-trip time without evidence; it is achieved only after measured scheduling capacity is known.
- Calibrate and validate each link class after the common schedule is reliable.

**Pass condition:** every pair has a measured report rate and invalid rate.  
**Stop condition:** do not train a motion model on unvalidated raw ranges.

## 7. Immediate next work

No hardware action is needed from the user yet.

Next, complete the API-level comparison between the official `fira_app.c`
lifecycle and the failed v1-v4/QANI code. Then produce one clean two-board
PELVIS/HEAD firmware candidate with a stage-by-stage startup report and retain
its matching ELF, HEX, and read-back comparison script together. Only after a
successful local build review will a new PELVIS flash be proposed, with a
single clear reason and a specific expected viewer result.

## 8. RAM-only official-FiRa candidate — 3 October 2026

The clean direct-FiRa candidate was built from the normal Qorvo `fira_app.c`
lifecycle. Before it was eligible for a flash, final-image inspection found
that the SDK's **prebuilt** `uwbstack_core` calls `qflash_write()` while saving
its L1 configuration. The source-level `CONFIG_L1_CONFIG_VOLATILE` option
would prevent that call if the stack itself were rebuilt, but Qorvo's supplied
library is already compiled and the package does not provide a volatile
prebuilt variant.

The candidate therefore removes only Qorvo's NRFX `qflash.c` from the local
application composition and supplies a COMP971-owned RAM-only replacement.
It matches the official volatile-mode result: configuration stays live in RAM
for this boot and a successful return is given, but **no MCU flash is erased
or written**. It does not alter Qorvo's DW3110 driver, UWB-MAC, FiRa protocol,
BLE stack, factory OTP reading, antenna delay, UICR, or calibration.

| Candidate | SHA-256 of HEX | RAM | Flash | Persistence audit |
|---|---|---:|---:|---|
| PELVIS controller | `84A5CC1CE3AB994024FC805781CE6A48735BB3053A8DB2EB533EF3A8FF51D6AF` | 69,824 B / 112 KiB (60.88%) | 337,696 B / 504 KiB (65.43%) | linked `qflash_write()` is RAM-counter only; no program/erase symbols |
| HEAD responder | `A0DF1C27DF6802153D0822CA81128282885A4AC539F7D775DCE254A71D46C93B` | 69,824 B / 112 KiB (60.88%) | 336,576 B / 504 KiB (65.22%) | same guard; no program/erase symbols |

PELVIS will report diagnostic `C0` after a successful session start; its
detail is the number of persistence requests prevented during that boot. This
is an audit signal, not a calibration or range result.

**Status:** built and safety-reviewed; **not flashed**. The next hardware
action remains a separately approved, named PELVIS-only flash followed by a
read-back comparison, then the equivalent HEAD flash only if PELVIS evidence
is clean.

### PELVIS installation result — 3 October 2026

PELVIS `760224787` is now installed with the RAM-guard candidate. Before the
write, two 512 KiB flash reads and two UICR reads matched. J-Link programmed
and verified one normal application range of 225,280 bytes, then reset and ran
the board. A separate 512 KiB post-flash read matched all 336,224 defined
Intel-HEX bytes; the pre/post UICR read SHA-256 is unchanged. This is accepted
as a correct PELVIS installation. HEAD is unchanged. The complete evidence is
in `outputs/records/firmware/2026-10-03_official_fira_ram_guard_pilot/`.

### HEAD installation result — 3 October 2026

HEAD `760225805` is now installed with the matched RAM-guard responder image.
Its fresh duplicate backup completed before programming. J-Link program and
verify passed for one 225,280-byte normal application range. A separate full
read-back matched all 335,104 defined Intel-HEX bytes, and the pre/post UICR
SHA-256 is unchanged. Both two-board pilot images are now exact and verified;
the next evidence required is the controlled 100 cm battery UWB start test.

### Battery startup result and BLE-first correction — 3 October 2026

The first battery attempt with both verified RAM-guard images did not reach a
UWB test: the gateway received zero Bluetooth packets from either board during
the clean observation window. Thus there are no IMU, status, UWB-start, or
range records to interpret. The installed code initialised UWB before it
started the COMP971 BLE advertiser, so a blocking UWB call removes the only
diagnostic transport.

The next source-only candidate starts BLE and FreeRTOS first, then calls the
official UWB lifecycle from the already-existing status task after a two-second
delay. It deliberately creates no new task because the SDK's FreeRTOS dynamic
heap is 4 KiB; a new 4 KiB UWB task would risk a startup allocation failure.
The PELVIS and HEAD candidates build and retain the RAM-only flash guard, but
are not flashed. Fresh duplicate backups are mandatory before either next
application write.
