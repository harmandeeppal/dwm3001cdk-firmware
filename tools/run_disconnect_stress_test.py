"""60-Cycle BLE Disconnect & Reconnect Stress Test Runner for COMP971.
Integrates high-performance serial stream capture and deterministic SWD disconnect injection.
"""
from __future__ import annotations

import argparse
import datetime
import json
import queue
import subprocess
import sys
import threading
import time
from pathlib import Path
import serial

HEAD_SNR = "760225805"
PELVIS_SNR = "760224787"
FORCE_DISC_ADDR = "0x20004600"

def trigger_disconnect(snr: str, addr: str = FORCE_DISC_ADDR, timeout_s: float = 3.0) -> bool:
    # Note: --ramwr halts core; we must immediately run it with --run so the node keeps executing
    cmd1 = ["nrfjprog", "--snr", snr, "--family", "NRF52", "--ramwr", addr, "--val", "1"]
    cmd2 = ["nrfjprog", "--snr", snr, "--family", "NRF52", "--run"]
    try:
        res1 = subprocess.run(cmd1, capture_output=True, text=True, timeout=timeout_s)
        res2 = subprocess.run(cmd2, capture_output=True, text=True, timeout=timeout_s)
        return (res1.returncode == 0 and res2.returncode == 0)
    except subprocess.TimeoutExpired:
        print(f"[WARN] nrfjprog timed out for SNR {snr}", flush=True)
        return False
    except Exception as e:
        print(f"[WARN] Failed to trigger disconnect on SNR {snr}: {e}", flush=True)
        return False

def run_stress_test(
    port: str = "COM16",
    baudrate: int = 115200,
    total_cycles: int = 60,
    cycle_cadence_s: float = 5.0,
    settling_s: float = 2.0,
    pre_test_s: float = 15.0,
    post_test_s: float = 45.0,
    output_dir: Path = Path("C:/comp971_raw_captures"),
    output_path: Path | None = None,
) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    if output_path is not None:
        bin_path = Path(output_path)
        json_path = bin_path.with_suffix(".json")
    else:
        timestamp_str = datetime.datetime.now().strftime("%Y-%m-%d_%H%M%S")
        bin_path = output_dir / f"gateway_disconnect_stress_{timestamp_str}.bin"
        json_path = output_dir / f"gateway_disconnect_stress_{timestamp_str}.json"

    total_test_duration = pre_test_s + (total_cycles * cycle_cadence_s) + post_test_s

    print("=" * 60, flush=True)
    print(f" COMP971 BLE Disconnect & Reconnect Stress Test", flush=True)
    print(f" Total Cycles: {total_cycles} | Cadence: {cycle_cadence_s}s", flush=True)
    print(f" Pre-test: {pre_test_s}s | Test: {total_cycles * cycle_cadence_s}s | Post-test: {post_test_s}s", flush=True)
    print(f" Total Duration: {total_test_duration}s (~{round(total_test_duration/60, 1)} min)", flush=True)
    print(f" Target Nodes: HEAD ({HEAD_SNR}), PELVIS ({PELVIS_SNR})", flush=True)
    print(f" Output File: {bin_path}", flush=True)
    print("=" * 60, flush=True)

    ser = serial.Serial(port=port, baudrate=baudrate, timeout=0.1)
    ser.setDTR(True)
    ser.setRTS(True)

    if settling_s > 0:
        print(f"Discarding {settling_s}s settling buffer...", flush=True)
        settle_end = time.time() + settling_s
        while time.time() < settle_end:
            _ = ser.read(4096)

    q = queue.Queue(maxsize=10000)
    stop_event = threading.Event()
    total_bytes_written = 0

    def writer_thread():
        nonlocal total_bytes_written
        with bin_path.open("wb", buffering=1024 * 1024) as f:
            while not stop_event.is_set() or not q.empty():
                try:
                    chunk = q.get(timeout=0.05)
                    f.write(chunk)
                    total_bytes_written += len(chunk)
                    q.task_done()
                except queue.Empty:
                    pass

    writer = threading.Thread(target=writer_thread, daemon=True)
    writer.start()

    start_time = time.time()
    start_utc = datetime.datetime.now(datetime.timezone.utc).isoformat()
    capture_end_time = start_time + total_test_duration

    # Reader thread
    def reader_thread():
        while time.time() < capture_end_time:
            try:
                data = ser.read(ser.in_waiting or 4096)
                if data:
                    try:
                        q.put_nowait(data)
                    except queue.Full:
                        time.sleep(0.001)
            except Exception as e:
                print(f"[ERR] Serial read error: {e}", flush=True)
                break

    reader = threading.Thread(target=reader_thread, daemon=True)
    reader.start()

    print(f"Starting {pre_test_s}s initial steady-state window...", flush=True)
    time.sleep(pre_test_s)

    print(f"Executing {total_cycles} disconnect cycles at {cycle_cadence_s}s cadence...", flush=True)
    cycle = 0
    cycle_start = time.time()

    for cycle in range(1, total_cycles + 1):
        elapsed = round(time.time() - start_time, 1)
        next_cycle_target = cycle_start + (cycle * cycle_cadence_s)

        if cycle <= 25:
            if cycle % 2 == 1:
                print(f"[{elapsed}s] Cycle {cycle}/{total_cycles}: Disconnect HEAD ({HEAD_SNR})...", flush=True)
                trigger_disconnect(HEAD_SNR)
            else:
                print(f"[{elapsed}s] Cycle {cycle}/{total_cycles}: Disconnect PELVIS ({PELVIS_SNR})...", flush=True)
                trigger_disconnect(PELVIS_SNR)
        elif cycle <= 50:
            print(f"[{elapsed}s] Cycle {cycle}/{total_cycles}: Dual Disconnect (HEAD + PELVIS)...", flush=True)
            trigger_disconnect(HEAD_SNR)
            trigger_disconnect(PELVIS_SNR)
        else:
            if cycle == 60:
                print(f"[{elapsed}s] Cycle {cycle}/{total_cycles}: Final Dual Disconnect (HEAD + PELVIS)...", flush=True)
                trigger_disconnect(HEAD_SNR)
                trigger_disconnect(PELVIS_SNR)
            else:
                target_snr = HEAD_SNR if (cycle % 2 == 1) else PELVIS_SNR
                label = "HEAD" if (cycle % 2 == 1) else "PELVIS"
                print(f"[{elapsed}s] Cycle {cycle}/{total_cycles}: Rapid Disconnect {label} ({target_snr})...", flush=True)
                trigger_disconnect(target_snr)

        # Sleep until next cadence tick
        sleep_dur = next_cycle_target - time.time()
        if sleep_dur > 0:
            time.sleep(sleep_dur)

    print(f"All {total_cycles} cycles completed! Entering {post_test_s}s post-test recovery window...", flush=True)
    remaining_recovery = capture_end_time - time.time()
    if remaining_recovery > 0:
        time.sleep(remaining_recovery)

    # Teardown
    reader.join(timeout=2.0)
    stop_event.set()
    writer.join(timeout=5.0)
    ser.close()

    metadata = {
        "capture_started_utc": start_utc,
        "total_test_duration_s": total_test_duration,
        "total_cycles": total_cycles,
        "cycle_cadence_s": cycle_cadence_s,
        "pre_test_s": pre_test_s,
        "post_test_s": post_test_s,
        "raw_bytes": total_bytes_written,
        "bin_file": bin_path.name,
    }
    json_path.write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    print(f"\nStress test complete! Captured {total_bytes_written} bytes to {bin_path}", flush=True)
    return bin_path

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="COMP971 BLE Disconnect Stress Test")
    parser.add_argument("--port", default="COM16")
    parser.add_argument("--baudrate", type=int, default=115200)
    parser.add_argument("--cycles", type=int, default=60)
    parser.add_argument("--cadence", type=float, default=5.0)
    parser.add_argument("--pre-test", type=float, default=15.0)
    parser.add_argument("--post-test", type=float, default=45.0)
    parser.add_argument("--output", default=None, help="Explicit path to output .bin file")
    args = parser.parse_args()

    run_stress_test(
        port=args.port,
        baudrate=args.baudrate,
        total_cycles=args.cycles,
        cycle_cadence_s=args.cadence,
        pre_test_s=args.pre_test,
        post_test_s=args.post_test,
        output_path=Path(args.output) if args.output else None,
    )

