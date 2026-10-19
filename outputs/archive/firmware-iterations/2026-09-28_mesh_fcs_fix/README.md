# 2026-09-28 mesh FCS-length fix

This build includes both earlier mesh fixes and corrects DW3110 receive-frame
length handling. The DW3110 reports the received length including its 2-byte
FCS; the mesh reader now removes that FCS length before checking the application
frame size. The command listener also remains resynchronising, so a missed
startup command cannot permanently desynchronise a remote node.

Only HEAD and PELVIS are included. The other four node images were not changed.

Images:

- `DWM-01-HEAD__mesh-fcs-fix__760225805.hex` — node index 0
- `DWM-02-PELVIS__mesh-fcs-fix__760224787.hex` — node index 1

Flash only the matching J-Link script, then test the two boards at 40 cm before
adding another node.
