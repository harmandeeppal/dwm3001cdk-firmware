# Direct official FiRa startup-dependency correction

## v3 test result

Direct-FiRa v3 was programmed to PELVIS and HEAD and tested on battery at a
stated 100 cm separation. The nRF52840 gateway received both ICM streams, but
the result was still:

| Metric | PELVIS result |
|---|---:|
| UWB attempts | 0 |
| Valid ranges | 0 |
| Invalid ranges | 0 |
| Startup/failure events (`C1`–`C5`, `D1`–`D9`) | 0 |
| UWB status | `not started` |

Therefore v3 did not fix the problem. The v3 stack-size hypothesis is not
accepted as the root cause.

## Source fault corrected in v4

The direct-FiRa v2/v3 source created an optional diagnostic FreeRTOS task
before creating the actual FiRa task. If that diagnostic task allocation
failed, it set a generic startup failure flag. The subsequent guard then
skipped creation of the real FiRa task. Because the diagnostic task itself had
failed to exist, it could not send the error record. This exactly explains the
observed combination of:

- `UWB not started`;
- zero attempts and ranges;
- zero startup/failure records; and
- otherwise healthy BLE/ICM telemetry.

This is an application task-dependency fault, not a J20, antenna, distance,
calibration, UWB-radio, ICM, or gateway issue.

## v4 correction

1. The optional diagnostic task is removed.
2. Diagnostic and progress records are sent from the already-working periodic
   status task.
3. The actual FiRa task has a dedicated static 4,096-byte stack, using the
   same underlying `xTaskCreateStatic` model as Qorvo's `qthread` wrapper.
4. The UWB task starts whenever Qorvo's `fira_uwb_mcps_init()` succeeds; it is
   no longer blocked by telemetry-task allocation.

The radio session remains Qorvo channel 9 in RAM. No antenna delay, OTP, UICR,
calibration, NVM, runtime flash, BLE packet contract, ICM configuration, or
gateway firmware is changed.

## Build verification

The trusted Qorvo CMake configurations previously used for v3 were reused to
compile the v4 source. Fresh empty build directories were attempted but did not
include two private Qorvo CMake path variables; no firmware was produced in
those directories. The successful builds below used the known-good Qorvo
configuration and recompiled both changed source files.

| Board | Role | Image path | SHA-256 | RAM | Flash | Result |
|---|---|---|---|---:|---:|---|
| PELVIS `760224787` | controller | `C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_PELVIS_v3\DWM3001CDK-COMP971-FreeRTOS.hex` | `A193682D01C87959DCEBAB719E597D40DF2B8A7B1A3580437CBD1D5E35374AC0` | 74,304 B / 112 KiB | 338,600 B / 504 KiB | passed |
| HEAD `760225805` | responder | `C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_HEAD_v3\DWM3001CDK-COMP971-FreeRTOS.hex` | `420AB330BE84FE6764849EF99379BF219727B37917E89DEE65154C9FB40B7BE7` | 74,304 B / 112 KiB | 337,496 B / 504 KiB | passed |

The source revision is v4 even though the trusted pre-existing build directory
names retain `v3`. The SHA-256 values above, not the build-directory name,
identify the v4 images.

## Flash boundary

PELVIS v4 was programmed after explicit approval through J9 on 2026-10-02.
J-Link selected serial `760224787`, confirmed a 3.3 V target reference,
checked the PELVIS SHA-256 above, then performed normal application programming
and verification. It reported one application-flash range at `0x00000000`
(`229,376` bytes), followed by `Program & Verify O.K.` and a normal reset.
No chip erase, UICR, OTP, antenna-delay, calibration, NVM, or runtime-flash
operation was used.

HEAD v4 was programmed after separate explicit approval through J9 on
2026-10-02. J-Link selected serial `760225805`, confirmed a 3.3 V target
reference, checked the HEAD SHA-256 above, then performed normal application
programming and verification. It reported one application-flash range at
`0x00000000` (`225,280` bytes), followed by `Program & Verify O.K.` and a
normal reset. No chip erase, UICR, OTP, antenna-delay, calibration, NVM, or
runtime-flash operation was used.

## v4 battery test result

PELVIS and HEAD were then run on battery with the nRF52840 gateway connected,
at a stated 100 cm separation. Both boards delivered live ICM data at about
58.8 Hz. The direct-FiRa result was nevertheless:

| Metric | Result |
|---|---:|
| PELVIS UWB attempts | 0 |
| Valid ranges | 0 |
| Invalid ranges | 0 |
| Setup progress records (`C1`–`C5`) | 0 |
| Failure records (`D1`–`D9`) | 0 |
| Board status | `ICM ready; UWB not started` |

This disproves the v4 task-dependency hypothesis. It is not a range,
antenna-delay, calibration, J20, battery, or gateway-reception problem: the
custom direct-FiRa task is not observably starting after the scheduler starts.
The build cache verifies that PELVIS used node index `1` and UWB role `1`, and
that the v4 direct-FiRa object was linked into the flashed image.

## Next source strategy

Do not make another direct-task timing or stack adjustment. The next pilot
will retain the same board roles and RAM-only channel-9 configuration, but
will call Qorvo's own QANI FiRa task creation and Qorvo start signal. COMP971
will only supply the role/address configuration and forward Qorvo's results to
the existing BLE telemetry format. This removes the application-created UWB
task from the radio-start path. No board flash is authorised by this record;
the rebuilt image must be inspected and approved separately.

The post-flash test remains PELVIS and HEAD on battery at 100 cm with the
nRF52840 gateway. A successful start must report progress code `C1` followed
by `C2`, `C3`, `C4`, and `C5`, then non-zero PELVIS UWB attempts and pair 0
range reports.
