# 2026-09-28 production mesh fixes

This is the clean build for the current HEAD/PELVIS verification. It includes:

- DW3110 preamble-detection timeout disabled for mesh receive;
- remote command resynchronisation, so missed startup commands cannot leave a
  node permanently out of step;
- correct DW3110 FCS-inclusive RX length handling;
- a larger initiator response buffer for the FCS bytes; and
- a 2 ms delayed response, which gives the responder safe timing margin.
- PELVIS filtering of the POLL and RESP frames it overhears before a
  remote-initiator's final result frame.

Debug frames are disabled. The first pair-0 capture was originally labelled
as 40 cm, but the user later confirmed that the separation was approximately
30 cm. It produced 35 valid readings in 10 seconds (median 38 cm), so it
proves the link works but is not a calibration result. The latest three-board
test reports pairs 0, 1, and 5: HEAD—PELVIS, HEAD—LEFT-WRIST, and
PELVIS—LEFT-WRIST. It is functional evidence only because the boards were very
close and their separation was not measured precisely. All six production
images are prepared here; HEAD, PELVIS, and LEFT-WRIST are currently flashed
and physically verified.

Images:

- `DWM-01-HEAD__mesh-production-fix__760225805.hex`
- `DWM-02-PELVIS__mesh-production-fix__760224787.hex`
- `DWM-03-LEFT-WRIST__mesh-production-fix__760225868.hex`
- `DWM-04-RIGHT-WRIST__mesh-production-fix__760225748.hex`
- `DWM-05-LEFT-ANKLE__mesh-production-fix__760225018.hex`
- `DWM-06-RIGHT-ANKLE__mesh-production-fix__760225934.hex`
