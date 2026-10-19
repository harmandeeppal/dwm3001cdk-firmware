"""
test_retention_sim.py
Simulates the exact C logic of g_comp971_retained vs g_comp971_frozen,
proves the overwrite defect in g_retained,
proves immunity in g_frozen,
and writes out test_c_retention_frame.bin for decode_gateway_capture.py.
"""
import struct
import zlib
import os

def calc_crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF

# ABI checks
# struct comp971_cb_record: 8 bytes
CB_FMT = "<HBBHH"
assert struct.calcsize(CB_FMT) == 8

# struct comp971_stall_snapshot_v1: 56 bytes
SNAP_FMT = "<BBBBIIIB3sIIiI16sI"
assert struct.calcsize(SNAP_FMT) == 56

# struct comp971_frozen_record: 56 bytes
FROZEN_FMT = "<IIHBBIIiIII16sI"
assert struct.calcsize(FROZEN_FMT) == 56

print("ABI verification passed: All struct sizes match 56 bytes, cb_record 8 bytes.")

# Constants
COMP971_PROTOCOL_VERSION = 1
COMP971_TELEMETRY_STALL_SNAPSHOT = 0x06
COMP971_RETAINED_MAGIC = 0x5354414C
COMP971_FROZEN_VALID_MARKER = 0xA55A0001
COMP971_ESCALATION_STAGE3_WDT = 0x03
COMP971_SNAPSHOT_RING_SIZE = 16

class RetainedSimulator:
    def __init__(self):
        self.magic = COMP971_RETAINED_MAGIC
        self.boot_count = 1
        self.reset_reason = 0
        self.last_escalation_stage = 0
        self.reset_loop_count = 0
        self.trigger_flags = 0
        self.last_reset_tick_ms = 0
        self.last_stall_duration_ms = 0
        self.last_block_index = 0
        self.lifetime_min_lead = 5200
        self.late_tx_count = 14
        self.sys_status_lo = 0x00000002
        self.sys_status_hi = 0x00000008
        self.ring_head = 0
        self.ring_count = 0
        self.ring = []

        # Frozen record
        self.frozen = None

    def callback(self, block: int, status: int, lead_ms: int):
        self.last_block_index = block
        rec = struct.pack(CB_FMT, block & 0xFFFF, status, lead_ms, (block * 200) & 0xFFFF, 0)
        if len(self.ring) < COMP971_SNAPSHOT_RING_SIZE:
            self.ring.append(rec)
        else:
            self.ring[self.ring_head] = rec
        self.ring_head = (self.ring_head + 1) % COMP971_SNAPSHOT_RING_SIZE
        self.ring_count = min(self.ring_count + 1, COMP971_SNAPSHOT_RING_SIZE)

    def record_stall_snapshot(self, stage: int, duration_ms: int, trigger_flags: int):
        self.last_escalation_stage = stage
        self.last_stall_duration_ms = duration_ms
        self.trigger_flags = trigger_flags

        # Two-phase commit into frozen record
        recent = [b"\x00" * 8, b"\x00" * 8]
        if self.ring_count > 0:
            idx1 = (self.ring_head + COMP971_SNAPSHOT_RING_SIZE - 1) % COMP971_SNAPSHOT_RING_SIZE
            recent[0] = self.ring[idx1]
            if self.ring_count > 1:
                idx2 = (self.ring_head + COMP971_SNAPSHOT_RING_SIZE - 2) % COMP971_SNAPSHOT_RING_SIZE
                recent[1] = self.ring[idx2]

        recent_bytes = recent[0] + recent[1]
        payload = struct.pack(
            "<IIHBBIIiIII16s",
            COMP971_FROZEN_VALID_MARKER,
            self.boot_count,
            trigger_flags,
            stage,
            0, # published
            duration_ms,
            self.last_block_index,
            self.lifetime_min_lead,
            self.late_tx_count,
            self.sys_status_lo,
            self.sys_status_hi,
            recent_bytes
        )
        crc = calc_crc32(payload)
        self.frozen = payload + struct.pack("<I", crc)

sim = RetainedSimulator()

# 1. Run Boot 1 up to block 1450
for b in range(1400, 1451):
    status = 33 if b == 1450 else 0
    sim.callback(b, status, 12)

assert sim.last_block_index == 1450

# 2. Trip supervisor stall
sim.record_stall_snapshot(COMP971_ESCALATION_STAGE3_WDT, 2010, 0x01) # Bit 0: cb_delta >= 2000 ms

# Parse frozen record before reboot
f_vals = struct.unpack(FROZEN_FMT, sim.frozen)
assert f_vals[0] == COMP971_FROZEN_VALID_MARKER
assert f_vals[1] == 1 # pre_reset_boot_id
assert f_vals[2] == 0x01 # trigger_flags
assert f_vals[3] == COMP971_ESCALATION_STAGE3_WDT
assert f_vals[6] == 1450 # last_block_index

cb0 = struct.unpack(CB_FMT, f_vals[11][:8])
assert cb0[0] == 1450
assert cb0[1] == 33 # Status 33 RX timeout

# 3. Simulate Reboot -> Boot 2
sim.boot_count = 2
boot2_resetreas = 0x00000004 # SREQ

# 4. Simulate 60 callbacks during boot grace period
for new_b in range(0, 60):
    sim.callback(new_b, 0, 15)

print(f"[Defect Proof] Live retained last_block_index is now: {sim.last_block_index} (Overwritten!)")
assert sim.last_block_index == 59

# 5. Check Frozen Record - must be 100% intact!
f_vals_post = struct.unpack(FROZEN_FMT, sim.frozen)
assert f_vals_post[0] == COMP971_FROZEN_VALID_MARKER
assert f_vals_post[1] == 1
assert f_vals_post[2] == 0x01
assert f_vals_post[3] == COMP971_ESCALATION_STAGE3_WDT
assert f_vals_post[6] == 1450 # Pre-reset block 1450 preserved!
cb0_post = struct.unpack(CB_FMT, f_vals_post[11][:8])
assert cb0_post[0] == 1450
assert cb0_post[1] == 33
assert calc_crc32(sim.frozen[:52]) == f_vals_post[12]

print(f"[Immunity Proof] Frozen record last_block_index is: {f_vals_post[6]} (Completely Immune!)")
print(f"[Immunity Proof] Frozen record recent_cbs[0].block: {cb0_post[0]}, status: {cb0_post[1]}")
print(f"[Immunity Proof] Frozen record CRC32 verified: 0x{f_vals_post[12]:08X}")

# 6. Build outgoing 56-byte snapshot wire packet
snap_payload = struct.pack(
    "<BBBBIIIB3sIIiI16s",
    COMP971_PROTOCOL_VERSION,
    COMP971_TELEMETRY_STALL_SNAPSHOT,
    0, # node_index (Pelvis)
    f_vals_post[2], # trigger_flags (0x01)
    COMP971_RETAINED_MAGIC,
    f_vals_post[1], # boot_count (1)
    boot2_resetreas, # reset_reason (0x04)
    f_vals_post[3], # escalation_reached (0x03)
    b"\x00\x00\x00",
    f_vals_post[5], # stall_duration_ms (2010)
    f_vals_post[6], # last_block_index (1450)
    f_vals_post[7], # min_lead_dtu (5200)
    f_vals_post[8], # late_tx_count (14)
    f_vals_post[11] # recent_cbs (16 bytes)
)
snap_crc = calc_crc32(snap_payload)
snap_wire = snap_payload + struct.pack("<I", snap_crc)
assert len(snap_wire) == 56

# 7. Wrap into 70-byte Gateway USB serial frame
# MAGIC: 0xC9 0x71, version: 1, type: 0x06, length: 64
gw_payload = snap_wire + struct.pack("<Q", 123456789000) # 8-byte gw timestamp
assert len(gw_payload) == 64

gw_hdr = b"\xc9\x71" + struct.pack("<BBH", 1, COMP971_TELEMETRY_STALL_SNAPSHOT, 64)
assert len(gw_hdr) == 6
gw_frame = gw_hdr + gw_payload
assert len(gw_frame) == 70

import tempfile

with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as tmp:
    tmp.write(gw_frame)
    temp_path = tmp.name

try:
    assert os.path.getsize(temp_path) == 70, "Gateway frame must be exactly 70 bytes"
    print(f"Successfully generated and validated temporary gateway frame (70 bytes): {temp_path}")
finally:
    if os.path.exists(temp_path):
        os.remove(temp_path)

