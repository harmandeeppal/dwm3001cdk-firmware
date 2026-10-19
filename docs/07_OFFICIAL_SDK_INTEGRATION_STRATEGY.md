# Official-SDK-first firmware strategy

## 1. Decision

The final wearable firmware will be **one custom application built on the
official Qorvo DW3/QM33 SDK for the DWM3001CDK**.

It will not be a custom application running beside Qorvo's prebuilt firmware:
one DWM3001C has one nRF52833 MCU and runs one application image at a time.
The official SDK source and its calibration/configuration behaviour will form
the low-level layer. The COMP971 code will provide the motion-system features
above it.

## 2. Responsibility split

| Layer | Owner in the final firmware |
|---|---|
| DW3110 initialisation, radio driver, timestamps, antenna paths, TX-power handling and radio calibration/configuration | Official Qorvo QM33 SDK |
| Six-node, 15-pair mesh schedule | COMP971 custom code |
| External ICM-20948 at 60 Hz | COMP971 custom code |
| Custom BLE packets to the nRF52840 dongle | COMP971 custom code |
| Laptop viewer, recording, filtering and motion fusion | COMP971 laptop-side code |

## 3. Why this change is needed

The prior firmware uses a Qorvo DW3xxx driver, but implements its own
ranging configuration and calibration handling. It does not use the full
DWM3001CDK QM33 calibration/configuration layer.

The official DWM3001CDK Developer Manual says that the Qorvo SDK calibration
and configuration layer includes antenna-path selection, TX power, antenna
delay and related settings. It is kept in non-volatile memory by the Qorvo
SDK. The existing custom UWB results prove that radio frames can exchange, but
do not prove that this full configuration has been reproduced correctly.

This is a change of engineering approach, not evidence that any board is
damaged.

## 4. Official SDK evidence

The locally archived package is:

`../official_docs/DW3_QM33_SDK_1.1.1.zip`

It contains the DWM3001CDK-specific Developer Manual, API Guide v4.12,
prebuilt DWM3001CDK UCI/CLI firmware, Qorvo UWB Explorer for Windows,
DWM3001CDK calibration/configuration JSON, and UCI tools such as `get_cal`,
`set_cal` and `load_cal`.

The supplied `load_cal` tool is **not** a read-only diagnostic: it sends a
calibration JSON to a board through UCI. The manual states that this
configuration is non-volatile. It must not be run on a wearable board without
a fresh backup and an explicit, recorded test purpose.

## 5. Work sequence

### Phase 0 - protect the existing work

- [x] Keep the original and recent full-flash/UICR backups under `../backups/`.
- [x] Archive the official SDK unchanged under `../official_docs/`.
- [x] Do not write OTP. OTP reads remain safe and read-only.
- [x] Pause acceptance of the old UWB calibration captures.

### Phase 1 - inspect without boards

- [x] Confirm the package contains the DWM3001CDK target and its official
  UCI/CLI binaries.
- [x] Confirm that Qorvo supplies a DWM3001CDK calibration/configuration file.
- [x] Replacement archive integrity verified on 1 October 2026. The user
  supplied `DW3_QM33_SDK_1.1.1.zip` is 623,516,600 bytes, has a valid ZIP
  central directory with 17,284 entries, and contains the 139,459,794-byte
  nested official source archive (17,241 entries). The earlier incomplete
  download remains rejected and is not used.
- [x] Open the official **source** release in a controlled review area and
  identify its DWM3001CDK target, board files, UWB configuration flow and
  calibration load path. No board is involved in this inspection.
- [x] Produce a direct configuration comparison: current COMP971 firmware
  versus official Qorvo DWM3001CDK source.
- [x] Build the unmodified official `DWM3001CDK-UCI-FreeRTOS` Release target
  in a no-space workspace. The build record and artifact hashes are in
  `../outputs/records/firmware/2026-10-01_official_qm33_sdk_build/`.

### Phase 2 - official two-board baseline

Purpose: distinguish an issue in COMP971 firmware from an issue affecting the
physical setup or boards.

1. Select only two boards, initially PELVIS (`760224787`) and LEFT-WRIST
   (`760225868`).
2. Make a **new** full-flash and UICR backup of each immediately before any
   official-firmware write.
3. Flash the official DWM3001CDK UCI firmware to those two boards.
4. Use Qorvo's own DWM3001CDK configuration/test flow at its recommended
   approximately two-metre known distance.
5. Save the exact firmware, configuration, distance, orientation and result.
6. Keep the two boards on the verified official UCI image only while it is
   useful as a reference baseline. Restore or replace it only with a recorded,
   verified image.

This phase changes normal application flash and may change Qorvo SDK
configuration NVM. It does **not** authorise OTP programming, chip erase, or
changes to the other four wearables.

### Phase 3 - final firmware integration

The official baseline behaved correctly on channel 9. The custom application
will be maintained in `../firmware/qorvo_qm33_motion_node/` and built against
the frozen official SDK working copy, without modifying Qorvo's low-level
driver or calibration/configuration interfaces. The unmodified official QANI
DWM3001CDK reference build is recorded in
`../outputs/records/firmware/2026-10-01_official_qani_dwm3001cdk_build/`.
QANI is a build reference only: its Apple Nearby Interaction application is
not the COMP971 wearable application.

**Direct-FiRa pilot — controlled correction in progress:** source review found
that the QANI sample successfully starts a session but follows the Apple
Nearby Interaction path. The PELVIS/HEAD direct-FiRa v2 build followed the
normal Qorvo lifecycle (`fira_uwb_mcps_init`, scheduler setup, normal
measurement sequence, session configuration and session start), but its
two-board test did not reach `UWB started`. The official QANI source uses a
4,096-byte, normal-priority Qorvo task while v2 used a 1,536-byte raw FreeRTOS
task. v3 corrects that task deployment and sends setup-progress records. It
retains RAM-only channel-9 settings, Qorvo's board/L1/calibration layers, and
the same BLE and ICM logic. v3 is built but not installed. See the
[`task-runtime correction record`](../outputs/records/firmware/2026-10-02_direct_official_fira_task_runtime_fix/README.md).

Add and test one custom feature at a time:

1. external ICM-20948 acquisition at 60 Hz;
2. custom BLE telemetry to the nRF52840 gateway;
3. two-node custom ranging with official configuration retained;
4. six-node sequential mesh;
5. the complete 15-pair schedule;
6. controlled accuracy, transport, and battery tests.

The 15 direct links, temporary initiator/responder roles, coordinator rule,
and measured-slot timing rule are defined in
`../firmware/qorvo_qm33_motion_node/docs/UWB_15_PAIR_MESH_DESIGN.md`.

No feature is promoted to all six boards until it passes on two named boards.

## 6. What is not the final design

- Do not use Qorvo's UCI/CLI prebuilt image as the final human-motion system.
  It does not implement the external ICM-20948 stream, the COMP971 BLE
  protocol, or the 15-pair body mesh.
- Do not run a second Qorvo firmware "under" the custom one. That is not
  technically possible on the single nRF52833 application processor.
- Do not copy one factory OTP antenna-delay number into all boards.
- Do not use the earlier custom range captures for model training or final
  calibration.

## 7. Current state and next action

**Current state:** the two-board official UCI baseline is complete and the
unmodified official QANI DWM3001CDK BLE/UWB reference project builds cleanly.
The QANI build has not been flashed because it is an Apple Nearby Interaction
application, not the COMP971 gateway/IMU application. The direct low-level
findings remain in [`08_QORVO_SDK_SOURCE_REVIEW.md`](08_QORVO_SDK_SOURCE_REVIEW.md).

**Next action:** prepare and build the first custom, battery-safe BLE
heartbeat application above the official project. No board is written until
that build and its gateway packet contract are reviewed.
