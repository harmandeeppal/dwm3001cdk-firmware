# DWM3001CDK full-mesh capability and validation status

## Short answer

The DWM3001CDK hardware is capable of multi-device UWB ranging. It is **not**
a ready-made six-device, 15-pair mesh product. The supplied FiRa tooling
supports one-to-one and one-to-many sessions, not a ready-made many-to-many
session. A wearable system that needs all 15 distances must schedule its own
direct ranging exchanges one after another.

This is the design used by this project: PELVIS sends one command, exactly one
pair ranges, then the next pair ranges. The six radios must not all transmit
at the same time.

## Official Qorvo evidence

1. Qorvo describes the DWM3001CDK as a DW3110-based kit for TWR/TDoA and RTLS
   evaluation: [DWM3001CDK product page](https://www.qorvo.com/products/ek/DWM3001CDK).
2. Qorvo staff show a DWM3001CDK TWR **one-to-many** configuration, with one
   initiator and separately addressed responders:
   [Qorvo support answer](https://forum.qorvo.com/t/how-can-i-measure-distance-using-multible-tags-and-anchors/17727).
3. Qorvo staff state that FiRa sessions are one-to-one or one-to-many, and
   that many devices can be tested through multiple one-to-many sessions:
   [Qorvo support answer](https://forum.qorvo.com/t/dwm3001cdk-multiple-tags-and-battery-charging/19309).
4. Qorvo explains that range rate depends on a valid combination of block
   duration, slot duration, and slots per ranging round:
   [Qorvo timing guidance](https://forum.qorvo.com/t/getting-fastest-distance-reading-on-dmw3001cdk/19322).

Therefore, the correct claim is not “the DWM3001CDK has a built-in 15-pair
mesh mode.” The correct claim is “the DWM3001CDK can take the required direct
ranges when firmware schedules and addresses the exchanges correctly.”

## What has succeeded

Three of the fifteen links have now been functionally validated:

| Item | Result |
|---|---|
| Boards | DWM-01-HEAD (`760225805`) and DWM-02-PELVIS (`760224787`) |
| Pair | Pair 0: HEAD—PELVIS |
| Test separation | Approximately 30 cm; the earlier 40 cm label was incorrect |
| Valid UWB results | 35 in 10 seconds |
| Median measured distance | 38 cm |
| PELVIS IMU stream | 599 samples in 10 seconds, about 60 Hz |
| Corrupt serial frames | 0 |

This confirms communication, but it is **not** an accuracy/calibration result
because the physical separation was only approximate. The raw capture retains
its original, now-known-to-be-wrong 40 cm input label:
`hardware/outputs/dwm3001cdk_motion_node/2026-09-28_mesh_production_fix/pelvis_head_40cm_test.json`.

| Item | Result |
|---|---|
| Boards | DWM-02-PELVIS (`760224787`) and DWM-03-LEFT-WRIST (`760225868`) |
| Pair | Pair 5: PELVIS—LEFT-WRIST |
| Test separation | Approximately 25 cm |
| Valid UWB results | 54 in 10 seconds |
| Median measured distance | 23.5 cm |
| Purpose | Functional link test only; not calibration |

The saved capture is
`hardware/outputs/dwm3001cdk_motion_node/2026-09-28_mesh_production_fix/pelvis_left_wrist_25cm_functional_test.json`.

| Item | Result |
|---|---|
| Boards | HEAD, PELVIS, and LEFT-WRIST |
| Pair | Pair 1: HEAD—LEFT-WRIST |
| Valid UWB results | 41 in 12 seconds |
| Median measured distance | 30 cm |
| Purpose | Functional three-node mesh test only; not calibration |

The three-node capture also contains valid results for pair 0 and pair 5.
It is saved as
`hardware/outputs/dwm3001cdk_motion_node/2026-09-28_mesh_production_fix/three_node_remote_result_receive_test.json`.
All three boards were very close together, so none of its distances are
accuracy evidence.

## Remote IMU forwarding status

Remote IMU forwarding meets the 60 Hz requirement in the current temporary
three-board schedule. This schedule enables only HEAD, PELVIS, and LEFT-WRIST
so it does not waste time waiting for the three unpowered boards. After PELVIS
receives a remote IMU command, the remote node waits 5 ms so PELVIS can safely
enter receive mode; PELVIS allows a 10 ms receive window. The verified capture
contains:

| IMU source | Node | Samples in 10 seconds | Current status |
|---|---:|---:|---|
| HEAD | 0 | 599 | 60 Hz; no missing sequence numbers |
| PELVIS | 1 | 599 | 60 Hz; no missing sequence numbers |
| LEFT-WRIST | 2 | 599 | 60 Hz; no missing sequence numbers |

Capture: `hardware/outputs/dwm3001cdk_motion_node/2026-09-28_mesh_production_fix/three_node_60hz_imu_verify.json`.

The earlier all-six schedule reached only 20 Hz in this three-board setup
because it waited for the three unpowered nodes. The six-node production
profile still requires an end-to-end 60 Hz validation with all six boards
powered.

The same capture proves that UWB results for all three active pairs reach
PELVIS. However, some range attempts are invalid (79 for pair 0, 51 for pair
1, and 46 for pair 5 in 10 seconds), so UWB reliability and calibration remain
separate required work before the distances can be used as a high-quality
motion feature.

### 28 September 2026: 3 ms responder timing re-test

HEAD and LEFT-WRIST were each flashed with a **temporary 3 ms UWB response
timing test image**. PELVIS retained the temporary three-node, 60 Hz schedule.
The test used the following named boards:

| Node | Board | J-Link serial |
|---:|---|---:|
| 0 | HEAD | `760225805` |
| 1 | PELVIS | `760224787` |
| 2 | LEFT-WRIST | `760225868` |

The 15-second live capture was collected from PELVIS and saved as
`hardware/outputs/dwm3001cdk_motion_node/2026-09-28_mesh_production_fix/three_node_uwb_3ms_all_test.json`.

| UWB pair | Boards | Valid reports | Invalid reports |
|---:|---|---:|---:|
| 0 | HEAD—PELVIS | 210 | 3 |
| 1 | HEAD—LEFT-WRIST | 214 | 0 |
| 5 | PELVIS—LEFT-WRIST | 214 | 0 |

This is a clear reliability improvement for the two links involving
LEFT-WRIST. It proves the active three-board UWB telemetry reaches PELVIS, but
it is **not calibration evidence**: the boards were close together and their
separation was not measured precisely.

The same capture contains continuous 60 Hz IMU forwarding with no missing
device sequence numbers:

| IMU source | Samples in 15 seconds | Missing sequence numbers |
|---|---:|---:|
| HEAD | 899 | 0 |
| PELVIS | 898 | 0 |
| LEFT-WRIST | 899 | 0 |

The 3 ms timing remains a test setting. It has not yet been declared the
final six-node production timing; that decision requires a six-board
end-to-end test.

## What failed before, and why

The earlier two-board mesh code failed because of firmware mistakes, not
because the two radios could not range.

1. A remote board could become permanently out of step if it missed one
   startup command.
2. The mesh code treated the received DW3110 frame length as application data
   length, even though the reported value includes the two-byte FCS.
3. The delayed response was scheduled only 1 ms after reception. Debug data
   showed that HEAD received the poll but sometimes could not start its delayed
   response in time. The production build uses 2 ms.
4. PELVIS initially accepted the first frame it heard during a remote-initiated
   exchange. It therefore treated the remote POLL or RESP frame as if it were
   the final range result. PELVIS now discards those frames and waits for the
   addressed result frame.

The production firmware fixes these three issues. The earlier direct two-board
test also proved the two boards and their antennas/power paths can range.

## What has not succeeded yet

The project has **not yet passed a 15-pair test**. Three links work; the
remaining twelve links are unverified. LEFT-WRIST is now flashed, while
RIGHT-WRIST and both ankles have production images prepared but are not yet
flashed or physically validated. We must not claim a full mesh or a 15-pair
rate until every board is flashed and the PELVIS laptop stream shows valid
values for all pair indices 0 through 14.

## Safe next validation order

1. Add RIGHT-WRIST, then both ankles, one at a time.
2. Run a full capture and require valid reports for all 15 pair indices.
3. Only then perform final antenna-delay calibration and evaluate the complete
   matrix rate.
