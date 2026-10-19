import os
import sys
import time
import subprocess
import serial
import json

PYTHON = sys.executable
TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
HARDWARE_DIR = os.path.dirname(TOOLS_DIR)
DECODER = os.path.join(TOOLS_DIR, "decode_gateway_capture.py")
GATEWAY_PORT = "COM16"
BAUD_RATE = 115200
OUTPUT_BIN = os.path.join(HARDWARE_DIR, "outputs", "records", "captures", "capture_60_disconnect_stress.bin")
OUTPUT_DIR = os.path.join(HARDWARE_DIR, "outputs", "records", "captures", "decoded_60_disconnect_stress")

HEAD_SN = "760225805"
PELVIS_SN = "760224787"
TARGET_ADDR = "0x200045f0"
COMPLETED_ADDR = "0x200045ec"

JLINK_EXE = r"C:\Program Files\SEGGER\JLink_V972\JLink.exe"

def run_cmd(args, timeout=10):
    try:
        res = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
        return res.returncode, res.stdout, res.stderr
    except subprocess.TimeoutExpired:
        return -1, "", "TIMEOUT"

def write_mem(sn, addr, val):
    script = f"connect\nhalt\nw4 {addr} {val}\ngo\nqc\n"
    script_path = f"hardware/tools/temp_write_{sn}.jlink"
    with open(script_path, "w") as f:
        f.write(script)
    args = [JLINK_EXE, "-device", "nRF52840_xxAA", "-if", "SWD", "-speed", "4000", "-SelectEmuBySN", sn, "-CommanderScript", script_path]
    code, out, err = run_cmd(args, timeout=5)
    try:
        os.remove(script_path)
    except Exception:
        pass
    print(f"[{sn}] write_mem {addr} = {val} -> code {code}")
    if code != 0:
        print(f"Error: {err.strip()}")
    return code == 0

def main():
    print("=== 60-Cycle Disconnect Stress Test ===")
    
    # 1. Reset counters in memory first
    write_mem(HEAD_SN, COMPLETED_ADDR, 0)
    write_mem(PELVIS_SN, COMPLETED_ADDR, 0)

    # 2. Open gateway serial port and start capturing
    print(f"Opening gateway on {GATEWAY_PORT}...")
    try:
        ser = serial.Serial(GATEWAY_PORT, BAUD_RATE, timeout=0.1)
    except Exception as e:
        print(f"Failed to open {GATEWAY_PORT}: {e}")
        return

    ser.reset_input_buffer()
    f_out = open(OUTPUT_BIN, "wb")
    
    # 3. Inject stress test target (60 disconnects)
    print("Injecting stress target = 60 into HEAD and PELVIS...")
    write_mem(HEAD_SN, TARGET_ADDR, 60)
    write_mem(PELVIS_SN, TARGET_ADDR, 60)

    # 4. Capture for 240 seconds while streaming
    start_time = time.time()
    last_poll = start_time
    duration = 240.0
    
    print(f"Capturing gateway stream for {duration} seconds...")
    try:
        while time.time() - start_time < duration:
            data = ser.read(4096)
            if data:
                f_out.write(data)
                f_out.flush()
            
            now = time.time()
            if now - last_poll >= 15.0:
                last_poll = now
                elapsed = now - start_time
                print(f"[{elapsed:5.1f}s] Captured {f_out.tell()} bytes...")
            time.sleep(0.01)
    except KeyboardInterrupt:
        print("Interrupted by user.")
    finally:
        ser.close()
        f_out.close()
        print(f"Capture completed. Saved {os.path.getsize(OUTPUT_BIN)} bytes to {OUTPUT_BIN}")

    # 5. Decode binary capture
    print("Decoding capture...")
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    decode_cmd = [PYTHON, DECODER, OUTPUT_BIN, "--output-directory", OUTPUT_DIR]
    code, out, err = run_cmd(decode_cmd, timeout=60)
    print(out)
    if code != 0:
        print(f"Decoder error: {err}")

    # 6. Extract gateway connection deltas
    gw_file = os.path.join(OUTPUT_DIR, "gateway_status.ndjson")
    if os.path.exists(gw_file):
        gw_lines = [json.loads(line) for line in open(gw_file) if line.strip()]
        if gw_lines:
            first_gw = gw_lines[0]
            last_gw = gw_lines[-1]
            delta_conn = last_gw["gateway_connects"] - first_gw["gateway_connects"]
            delta_disc = last_gw["gateway_disconnects"] - first_gw["gateway_disconnects"]
            print("\n=== GATEWAY DELTA SUMMARY ===")
            print(f"Start: connects={first_gw['gateway_connects']}, disconnects={first_gw['gateway_disconnects']}")
            print(f"End:   connects={last_gw['gateway_connects']}, disconnects={last_gw['gateway_disconnects']}")
            print(f"Delta: connects={delta_conn}, disconnects={delta_disc}")
            print(f"Final connected_count: {last_gw['connected_count']}")

    # 7. Extract IMU sequence gaps and per-reconnect drop analysis
    imu_file = os.path.join(OUTPUT_DIR, "imu.ndjson")
    if os.path.exists(imu_file):
        imu_lines = [json.loads(line) for line in open(imu_file) if line.strip()]
        by_node = {}
        for l in imu_lines:
            by_node.setdefault(l["node_index"], []).append(l)
        
        print("\n=== IMU SEQUENCE GAP & RECONNECT ANALYSIS ===")
        for n, samples in by_node.items():
            gaps = []
            for i in range(1, len(samples)):
                prev_seq = samples[i-1]["sequence"]
                curr_seq = samples[i]["sequence"]
                diff = curr_seq - prev_seq
                if diff > 1:
                    dt_ms = (samples[i]["gateway_receive_timestamp_us"] - samples[i-1]["gateway_receive_timestamp_us"]) / 1000.0
                    gaps.append((i, prev_seq, curr_seq, diff - 1, dt_ms))
            print(f"Node {n}: Total Received Samples={len(samples)}, Detected Gaps={len(gaps)}")
            total_dropped = sum(g[3] for g in gaps)
            print(f"Node {n}: Total Dropped Samples During Disconnects = {total_dropped}")
            if gaps:
                print(f"Node {n} Sample Gaps (first 10):")
                for g in gaps[:10]:
                    print(f"   Gap #{g[0]}: seq {g[1]} -> {g[2]} (dropped {g[3]} samples, link downtime {g[4]:.1f} ms)")

    # 8. Extract UWB diagnostics
    diag_file = os.path.join(OUTPUT_DIR, "uwb_diag.ndjson")
    if os.path.exists(diag_file):
        diag_lines = [json.loads(line) for line in open(diag_file) if line.strip()]
        disc_reports = [d for d in diag_lines if d.get("stage") == "disconnect_and_flush_counts"]
        conn_reports = [d for d in diag_lines if d.get("stage") == "connect_count_and_queue_depth"]
        print("\n=== NODE TELEMETRY SUMMARY ===")
        for d in disc_reports[-4:]:
            node = d["node_index"]
            disc_cnt = (d["detail"] >> 16) & 0xFFFF
            flush_cnt = d["detail"] & 0xFFFF
            print(f"Node {node} Disconnects Reported: {disc_cnt}, Items Flushed: {flush_cnt}")
        for c in conn_reports[-4:]:
            node = c["node_index"]
            conn_cnt = (c["detail"] >> 16) & 0xFFFF
            print(f"Node {node} Connects Reported: {conn_cnt}")

if __name__ == "__main__":
    main()

