import serial
import time
import struct
import subprocess
import os
import sys

# COM ports
COM_GATEWAY = "COM16"
BAUD_GATEWAY = 115200

# Retained RAM addresses from ELF
ADDR_FORCE_STALL = 0x20005484
ADDR_RETAINED = 0x20015E0C

def write_stall_flag(val):
    cmd = f"nrfjprog --snr 760225805 --memwr {ADDR_FORCE_STALL:#x} --val {val}"
    res = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=10)
    print(f"[SWD] write_stall_flag({val}) -> rc={res.returncode}")

def read_mem(addr):
    cmd = f"nrfjprog --snr 760225805 --memrd {addr:#x} --n 4"
    res = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=10)
    print(f"[SWD] read_mem({addr:#x}) -> {res.stdout.strip()}")
    return res.stdout.strip()

print("Opening Gateway COM16...")
ser = serial.Serial(COM_GATEWAY, BAUD_GATEWAY, timeout=0.1)
ser.reset_input_buffer()

print("Verifying initial communications for 5 seconds...")
t0 = time.time()
raw_bytes = bytearray()
while time.time() - t0 < 5.0:
    chunk = ser.read(1024)
    if chunk:
        raw_bytes.extend(chunk)
print(f"Captured {len(raw_bytes)} bytes from COM16 prior to stall test.")

print("Injecting stall into HEAD (SN 760225805) by holding g_fira_force_stall = 1 for 10 seconds...")
write_stall_flag(1)

# Now monitor COM16 for 20 seconds.
# We expect:
# 1. Ranging ceases.
# 2. Stage 1 at 2s (fails to resume callbacks because force_stall=1)
# 3. Stage 2 at 4s (fails to resume callbacks because force_stall=1)
# 4. Stage 3 at 6s (ceases feeding watchdog)
# 5. Hardware watchdog fires at ~8s.
# 6. HEAD reboots, increments boot_count, records reset_reason (0x00000002 WDT),
#    retains snapshot block with magic 0x5354414C, escalation stage 3,
#    reconnects over BLE and transmits stall snapshot frame (0x06).

t_start = time.time()
frames_06 = []
all_frames = []

def parse_slip_frames(buf):
    frames = []
    # SLIP END = 0xC0, ESC = 0xDB, ESC_END = 0xDC, ESC_ESC = 0xDD
    # Or gateway simple framing. Let's inspect raw search for message_type == 0x06 (COMP971_TELEMETRY_STALL_SNAPSHOT)
    # The gateway serial output may have binary frames or framed packets.
    return

# Clear flag after 10 seconds so after reboot it runs normally if it doesn't clear RAM
cleared_flag = False
while time.time() - t_start < 25.0:
    if not cleared_flag and (time.time() - t_start > 12.0):
        # Ensure flag is 0 so newly booted node runs normally
        try:
            write_stall_flag(0)
            cleared_flag = True
        except Exception as e:
            print(f"[SWD Error] {e}")
            
    chunk = ser.read(1024)
    if chunk:
        raw_bytes.extend(chunk)
        # Search for stall snapshot pattern:
        # protocol_version=1 (0x01), message_type=0x06, magic=0x5354414C ("STAL" in little endian: 4C 41 54 53)
        # In byte order: 01 06 01 xx 4C 41 54 53 (for node 1) or 01 06 00 xx 4C 41 54 53 (for node 0)
        for i in range(len(raw_bytes) - 56):
            if raw_bytes[i] == 0x01 and raw_bytes[i+1] == 0x06 and raw_bytes[i+4:i+8] == b'\x4c\x41\x54\x53':
                payload = raw_bytes[i:i+56]
                if payload not in frames_06:
                    frames_06.append(payload)
                    print(f"!!! FOUND STALL SNAPSHOT FRAME (0x06) at offset {i} !!!")
                    (proto, mtype, node, res, magic, boot, rreas, esc,
                     r2_0, r2_1, r2_2, dur, last_blk, min_lead, late_tx,
                     cb0_blk, cb0_st, cb0_dur, cb0_dtu,
                     cb1_blk, cb1_st, cb1_dur, cb1_dtu,
                     crc) = struct.unpack("<BBBBIIIBBBBIIiIIhBBIhBBI", payload)
                    print(f"  Protocol: {proto}, Type: {mtype:#x}, Node: {node}")
                    print(f"  Magic: {magic:#x} ('{payload[4:8].decode('ascii', errors='ignore')}')")
                    print(f"  Boot Count: {boot}")
                    print(f"  Reset Reason: {rreas:#x} (Bit 1 = WDT: {bool(rreas & 2)})")
                    print(f"  Escalation Reached: Stage {esc}")
                    print(f"  Stall Duration: {dur} ms")
                    print(f"  Last Block Index: {last_blk}")
                    print(f"  Lifetime Min Lead: {min_lead} DTU")
                    print(f"  Late TX Count: {late_tx}")
                    print(f"  Recent CB 0: block={cb0_blk}, status={cb0_st}, dur={cb0_dur}, lead={cb0_dtu}")
                    print(f"  Recent CB 1: block={cb1_blk}, status={cb1_st}, dur={cb1_dur}, lead={cb1_dtu}")
                    print(f"  CRC32: {crc:#x}")

ser.close()

# Save captured raw bytes for offline decode and audit
tools_dir = os.path.dirname(os.path.abspath(__file__))
hardware_dir = os.path.dirname(tools_dir)
output_path = os.path.join(hardware_dir, "outputs", "records", "captures", "test_stage3_capture.bin")
with open(output_path, "wb") as f:
    f.write(raw_bytes)

print(f"\nCaptured total {len(raw_bytes)} bytes to {output_path}. Total snapshot frames detected: {len(frames_06)}")


