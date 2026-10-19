# Pelvis–Head UWB recovery validation

## 100 cm controlled test

The file `head_pelvis_100cm_20260929_pre_filename_fix.json` contains the
45-second capture made after restoring matching standard firmware to Pelvis and
Head. Its internal `pair_index` is `0`, which is the canonical Head–Pelvis
pair. It contains 66 valid samples, 5 invalid attempts, a 160.5 cm median, and
a 12.76 cm standard deviation at a stated 100 cm.

The capture tool at that time had a filename and pair-label defect inherited
from the older Pelvis–Left-Wrist experiment. The file was immediately moved
here under a correct filename; its raw sample data was not altered. The tool
was then corrected to derive both names from the supplied pair index and to
include a timestamp, preventing future overwrite.

## Evidence interpretation

This test proves that the restored Head and Pelvis UWB radios exchange valid
ranging frames. It does not pass the accuracy acceptance rule because 160.5 cm
is not close enough to the stated 100 cm and spread remains high.

## Superseded Left-Wrist 100 cm record

The previous raw Pelvis–Left-Wrist 100 cm JSON was overwritten by the naming
defect before it was detected. Its previously reported terminal summary was
122 valid, 2 invalid, 150 cm median, and 26.5 cm standard deviation. Those
values are retained only as historical context in the system guide and are not
acceptable calibration evidence. A clean repeat is required if that pair is
used again.
