# Six-node UWB mesh and six-axis ICM design

Status: the clean production mesh-fix images are flashed on PELVIS, HEAD, and
LEFT-WRIST and J-Link verified. Pair 0 (HEAD—PELVIS) and pair 5
(PELVIS—LEFT-WRIST) return valid UWB ranges. Pair 1 (HEAD—LEFT-WRIST) now also
returns valid UWB ranges. The earlier pair-0 test was
incorrectly labelled as 40 cm; the user later confirmed its separation was
only approximately 30 cm, so it is a functional test rather than calibration
evidence. Full six-board validation and calibration have not started yet.

This document is the hardware implementation plan for the COMP971 wearable
system. It covers six DWM3001CDK boards, six external ICM-20948 sensors, and
the complete set of 15 direct UWB body-to-body distances.

## Project input contract

The active model contract is
[`workspace/configs/experiments/prediction_contract_v0.json`](../../../workspace/configs/experiments/prediction_contract_v0.json).
It requires 12 IMU features for each of the six body locations:

| Feature group | Values | Hardware source |
|---|---:|---|
| Acceleration | 3 | ICM-20948 accelerometer |
| Angular velocity | 3 | ICM-20948 gyroscope |
| Orientation | 6 | First two columns of an orientation matrix estimated from accelerometer and gyroscope |

The selected project sampling rate is 60 Hz. The canonical sensor order and
the canonical order of the 15 UWB pairs are owned by
[`workspace/configs/topology/smplh_wearable_topology_v0.json`](../../../workspace/configs/topology/smplh_wearable_topology_v0.json).

Current validation note: a temporary three-board schedule has delivered 599
samples in 10 seconds from each of HEAD, PELVIS, and LEFT-WRIST, with no
missing sequence numbers. This meets the 60 Hz requirement for that active
three-board configuration. The final six-board schedule still needs its own
end-to-end 60 Hz validation.

## Magnetometer decision

The magnetometer is not used.

- It is not initialized through the ICM-20948 internal I2C master.
- It is not read.
- It is not included in UWB packets or laptop telemetry.
- It is not a neural-network input.
- The six orientation values will be derived by a six-axis attitude estimator
  using accelerometer and gyroscope data only.

This decision avoids indoor magnetic interference. A six-axis estimator cannot
make yaw globally absolute; its yaw drift must be handled by calibration,
motion modelling, and the geometric constraints supplied by the UWB mesh.

The production type `0x03` ICM packet is:

```text
source_node:u8 | sequence:u32 | timestamp_ms:u32 |
accel_x/y/z:i16 | gyro_x/y/z:i16 | temperature:i16
```

The temperature is diagnostic data only; it is not a model feature.

## Permanent board identities

| Node index | Permanent name | Body position | J-Link serial |
|---:|---|---|---:|
| 0 | DWM-01-HEAD | Head | 760225805 |
| 1 | DWM-02-PELVIS | Pelvis | 760224787 |
| 2 | DWM-03-LEFT-WRIST | Left wrist | 760225868 |
| 3 | DWM-04-RIGHT-WRIST | Right wrist | 760225748 |
| 4 | DWM-05-LEFT-ANKLE | Left ankle | 760225018 |
| 5 | DWM-06-RIGHT-ANKLE | Right ankle | 760225934 |

J-Link serial number is the physical board identity. A Windows COM port is
only a temporary connection detail and must not be used as an identity.

## Full-mesh UWB design

There are six body nodes and therefore fifteen undirected direct distances.
PELVIS is the timing coordinator, not a radio relay for the other links.

PELVIS sends one explicit command before each transaction. The command names
the pair (or the remote IMU source) and is broadcast so the two addressed
boards can prepare. Then exactly one point-to-point exchange runs. A board may
initiate some transactions and respond in others; no two independent
exchanges are allowed to begin together. This avoids relying on clocks that
are not shared between boards.

| Pair index | Direct link | Initiator |
|---:|---|---|
| 0 | HEAD — PELVIS | PELVIS |
| 1 | HEAD — LEFT-WRIST | HEAD |
| 2 | HEAD — RIGHT-WRIST | HEAD |
| 3 | HEAD — LEFT-ANKLE | HEAD |
| 4 | HEAD — RIGHT-ANKLE | HEAD |
| 5 | PELVIS — LEFT-WRIST | PELVIS |
| 6 | PELVIS — RIGHT-WRIST | PELVIS |
| 7 | PELVIS — LEFT-ANKLE | PELVIS |
| 8 | PELVIS — RIGHT-ANKLE | PELVIS |
| 9 | LEFT-WRIST — RIGHT-WRIST | LEFT-WRIST |
| 10 | LEFT-WRIST — LEFT-ANKLE | LEFT-WRIST |
| 11 | LEFT-WRIST — RIGHT-ANKLE | LEFT-WRIST |
| 12 | RIGHT-WRIST — LEFT-ANKLE | RIGHT-WRIST |
| 13 | RIGHT-WRIST — RIGHT-ANKLE | RIGHT-WRIST |
| 14 | LEFT-ANKLE — RIGHT-ANKLE | LEFT-ANKLE |

After an exchange not initiated by PELVIS, the initiating board transmits its
result to PELVIS. PELVIS is the only board connected to the laptop and emits
the complete range matrix through its serial port.

## IMU transport to the laptop

All six ICM-20948 devices sample accelerometer and gyroscope data at 60 Hz.

- PELVIS forwards its own samples directly to the laptop.
- Each remote board queues its samples.
- Each remote board has a dedicated short UWB uplink slot after the ranging
  slots.
- It sends up to five queued six-axis samples in one batch to PELVIS.
- PELVIS forwards every received remote sample to the laptop with its source
  node index, sequence number, and device timestamp.

The collector UART is configured for 460800 baud. This is needed for the six
60 Hz IMU streams plus UWB range results; 115200 baud leaves too little
headroom.

## Rate and fusion policy — agreed project decision

The project uses **60 Hz accelerometer and gyroscope samples on every ICM**.
It does **not** use 30 Hz as its primary IMU rate.

This is a design decision made before model training.  The reasons are:

- the gyroscope captures fast segment rotation, which is especially important
  because no magnetometer will correct yaw;
- 60 Hz gives a useful balance between movement detail, battery use, and the
  available serial/UWB transport capacity across all six nodes;
- 30 Hz is appropriate for some low-power activity-classification systems,
  but gives less time detail for full-body motion estimation; and
- the final training data, validation data, and deployed model must use the
  same rate.  Changing rate later requires a controlled comparison and a new
  training/validation run.

The UWB mesh deliberately has a different rate.  It is not necessary, and is
not realistic for the first six-node full-mesh implementation, for all 15 UWB
distances to refresh at the same 60 Hz rate as the IMUs.

The first mesh firmware uses conservative 4 ms ranging slots and 1.5 ms IMU
uplink slots. It has no accepted all-six matrix-rate claim yet. The only
validated physical link is HEAD—PELVIS; all fifteen links must be measured
before a matrix rate is reported. Qorvo confirms that a valid ranging system
must choose block duration, slot duration, and slots per ranging round
together; see [Qorvo timing guidance](https://forum.qorvo.com/t/getting-fastest-distance-reading-on-dmw3001cdk/19322).
After live validation, the target is to test a stable **20–30 Hz** complete
UWB matrix rate. A rate is accepted only when all 15 links retain acceptable
validity, range error, loss rate, and timing margin with all six boards on the
body.

The collector creates a 60 Hz fusion timeline.  At each IMU timestamp it uses
the newest available 15-distance UWB matrix and stores, for every range:

- its measurement timestamp and age;
- a valid/missing status; and
- a body-NLOS-suspected status where applicable.

Therefore, several consecutive 60 Hz IMU frames may use the same latest UWB
range value.  This is expected.  The collector must never label a repeated
UWB value as a new measurement or invent a 60 Hz UWB stream by copying values
without its age and validity information.

Before final model training, record a small controlled pilot dataset at 30,
60, and 100 Hz IMU rates using identical movements.  Compare model accuracy,
dropouts, power use, and orientation stability.  60 Hz remains the deployment
default unless that experiment provides clear evidence that another rate is
better for this specific system.

## Implementation and verification status

Implemented in source:

- magnetometer removal from the ICM-20948 reader;
- 60 Hz six-axis ICM sample stream with source identity;
- unique UWB short addresses;
- deterministic 15-pair mesh schedule;
- explicit PELVIS-issued range and IMU slot commands;
- range-result forwarding to PELVIS;
- remote ICM batching and uplink to PELVIS;
- 460800-baud collector UART configuration.
- the DWM3001CDK-specific board overlay `nrf52833dk_nrf52833.overlay`, which
  selects the real DW3110 chip-select pin P1.06 (the generic nRF52833 DK
  default points to P0.16 and does not reach the UWB device).

Build verification completed:

- a RIGHT-ANKLE mesh image compiled successfully;
- a PELVIS mesh image compiled successfully and contains the permanent name
  `DWM-02-PELVIS`.

Verified on physical hardware:

- the clean production two-board mesh exchange for pair 0 (HEAD—PELVIS). The
  final 10-second capture produced 35 valid ranges, median 38 cm, 599 PELVIS
  ICM samples, and no corrupt serial frames. The physical separation was later
  corrected from the recorded 40 cm to approximately 30 cm, so this test does
  not establish range accuracy. The
  saved result is
  `hardware/outputs/dwm3001cdk_motion_node/2026-09-28_mesh_production_fix/pelvis_head_40cm_test.json`.
- the production mesh exchange for pair 5 (PELVIS—LEFT-WRIST). A 10-second
  functional test at approximately 25 cm produced 54 valid ranges with a
  23.5 cm median. It is not used for calibration.

The following still require physical validation:

- all-six timing and range validity;
- remote IMU sample delivery to PELVIS;
- six-axis orientation collector;
- measured UWB matrix rate;
- fresh per-device antenna-delay calibration with final body mounting.

PELVIS and HEAD are flashed with the clean production images in
`hardware/outputs/dwm3001cdk_motion_node/2026-09-28_mesh_production_fix/` and
both passed J-Link program-and-verify. PELVIS is COM6 (J-Link `760224787`)
and HEAD is COM13 (J-Link `760225805`). The earlier failed mesh images,
temporary debug images, and complete flash backups remain retained in their
dated folders.

On 2026-09-28, a temporary direct initiator/responder diagnostic was flashed
to these same two boards. At a known 40 cm separation it produced 200 valid
UWB samples with a 39 cm median, 39.02 cm mean, 2.80 cm standard deviation,
and -1 cm median error. The diagnostic was then removed and the explicit mesh
images were restored and verified. This isolates the remaining failure to the
mesh exchange firmware, not the radios, power, or physical spacing.

## Deployment order

1. Complete and test the laptop collector, including six-axis orientation.
2. Flash PELVIS and one remote board using their verified, backed-up J-Link
   identities.
3. Verify a direct range and remote ICM sample delivery.
4. Flash and verify the remaining four remote boards one at a time.
5. Measure all 15 links with all six boards powered.
6. Calibrate antenna delays and validate distances at controlled separations.
7. Record the measured UWB rate and align data to the 60 Hz fusion timeline.

The mesh profile is deliberately still disabled in
[`hardware/config/profiles/six_node_telemetry_v1.json`](../../config/profiles/six_node_telemetry_v1.json)
until these physical acceptance checks pass.
