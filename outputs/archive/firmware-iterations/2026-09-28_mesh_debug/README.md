# 2026-09-28 temporary mesh diagnostic build

This temporary two-node build includes the mesh preamble, command
resynchronisation, and FCS-length fixes, plus type `0xFE` diagnostic frames.
The diagnostic frame payload is six bytes: `event_code`, `pair_or_index`, and a
little-endian signed detail value. It is for debugging only and is not the
production six-node image.

Only HEAD and PELVIS are included. Do not flash the other four boards.
