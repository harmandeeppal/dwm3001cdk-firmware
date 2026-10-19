# Six-node, 15-pair UWB mesh design

## Goal

Measure every unique body-to-body distance from six nodes:

| Pair index | Direct UWB link |
|---:|---|
| 0 | HEAD ↔ PELVIS |
| 1 | HEAD ↔ LEFT-WRIST |
| 2 | HEAD ↔ RIGHT-WRIST |
| 3 | HEAD ↔ LEFT-ANKLE |
| 4 | HEAD ↔ RIGHT-ANKLE |
| 5 | PELVIS ↔ LEFT-WRIST |
| 6 | PELVIS ↔ RIGHT-WRIST |
| 7 | PELVIS ↔ LEFT-ANKLE |
| 8 | PELVIS ↔ RIGHT-ANKLE |
| 9 | LEFT-WRIST ↔ RIGHT-WRIST |
| 10 | LEFT-WRIST ↔ LEFT-ANKLE |
| 11 | LEFT-WRIST ↔ RIGHT-ANKLE |
| 12 | RIGHT-WRIST ↔ LEFT-ANKLE |
| 13 | RIGHT-WRIST ↔ RIGHT-ANKLE |
| 14 | LEFT-ANKLE ↔ RIGHT-ANKLE |

There are `6 × 5 ÷ 2 = 15` links. A matrix update is complete only after all
15 slots have run once.

## Plain-language behaviour

PELVIS is the **schedule coordinator**. It tells the network which pair has
the next turn. During that turn, the two named boards exchange their UWB
messages directly. For example, LEFT-ANKLE and RIGHT-ANKLE exchange messages
with each other; PELVIS does not estimate their distance from two other
distances.

Only one pair is allowed to transmit at a time. This prevents radio-message
collisions. Nodes not named in the active slot listen or sleep.

## Roles are temporary

No board is permanently only an initiator or only a responder. For each slot:

1. PELVIS issues/advances the slot schedule.
2. The first named node is the TWR initiator for that one exchange.
3. The second named node is the responder for that one exchange.
4. The range result and a quality/status value are sent through the existing
   direct BLE link to the nRF52840 gateway.
5. The schedule advances to the next pair.

The laptop reconstructs the 15-value matrix from those BLE records. This uses
BLE for data transport and UWB for distance measurement, without spending
extra UWB airtime forwarding sensor data.

## Timing rule

The slot length must be measured on the official Qorvo stack before choosing a
final update rate. A slot contains:

`schedule guard + UWB poll/response/final/result + radio-processing guard`

The guard is intentional breathing room. If the next pair starts before the
previous hardware transaction has fully finished, the messages can overlap and
create invalid ranges.

The full-matrix rate is:

`matrix rate = 1 ÷ (15 × measured slot length)`

Example only: if a proven safe slot is 10 ms, a full 15-pair matrix takes
150 ms, or about 6.7 complete matrices per second. This does **not** reduce
the separate 60 Hz IMU rate. We will measure a two-node slot first, choose a
safe guard from its worst normal timing, then calculate the real matrix rate.

## Build and test order

1. Complete the official-stack BLE heartbeat and 60 Hz ICM stream.
2. Implement a channel-9 official-stack two-node TWR exchange.
3. Measure valid-rate, invalid-rate, transaction time and 95th-percentile
   duration at 50, 75 and 100 cm.
4. Choose the slot time from that measurement, not a guess.
5. Add the 15 pair entries above, one at a time in the defined order.
6. Verify that each pair reports to the gateway and that a new slot never
   starts early.
7. Run controlled distance and moving-body tests before accepting the mesh.

## Safety and calibration boundary

- The mesh reads the normal official Qorvo configuration/calibration path.
- It does not write antenna delay, OTP, or flash during operation.
- Controlled calibration is a later, separately recorded action; it is not
  hidden inside mesh firmware.
