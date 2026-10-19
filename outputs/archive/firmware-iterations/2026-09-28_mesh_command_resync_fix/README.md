# 2026-09-28 mesh command-resynchronisation build

This build keeps the preamble-detection fix and adds robust command
resynchronisation on remote boards. A remote now accepts the next valid command
and uses the pair/node index carried in that command. It no longer assumes it
received every earlier command in sequence, so a missed startup frame cannot
leave it permanently out of step with PELVIS.

Only HEAD and PELVIS are included here. The other four node images were not
changed.

Images:

- `DWM-01-HEAD__mesh-command-resync-fix__760225805.hex` — node index 0
- `DWM-02-PELVIS__mesh-command-resync-fix__760224787.hex` — node index 1

Use the matching J-Link script for each serial number. After flashing, test
only the HEAD/PELVIS pair at 40 cm before adding any other board.
