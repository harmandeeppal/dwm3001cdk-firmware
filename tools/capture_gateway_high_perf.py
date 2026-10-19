"""High-performance binary stream capturer for COMP971 Motion Gateway (COM16).
Uses dedicated read thread and double-buffering to ensure USB bulk endpoint never experiences host backpressure.
"""
from __future__ import annotations

import argparse
import datetime
import json
import queue
import sys
import threading
import time
from pathlib import Path
import serial

def capture(port: str, baudrate: int, duration_seconds: float, settling_seconds: float, output_path: Path) -> dict:
    q = queue.Queue(maxsize=10000)
    stop_event = threading.Event()
    total_bytes = 0

    ser = serial.Serial(port=port, baudrate=baudrate, timeout=0.1)
    ser.setDTR(True)
    ser.setRTS(True)

    # Discard settling buffer
    if settling_seconds > 0:
        settle_end = time.time() + settling_seconds
        while time.time() < settle_end:
            _ = ser.read(4096)

    def writer_thread():
        nonlocal total_bytes
        last_flush = time.time()
        with output_path.open("wb", buffering=1024 * 1024) as f:
            while not stop_event.is_set() or not q.empty():
                try:
                    chunk = q.get(timeout=0.05)
                    f.write(chunk)
                    total_bytes += len(chunk)
                    q.task_done()
                    now = time.time()
                    if now - last_flush >= 2.0:
                        f.flush()
                        last_flush = now
                except queue.Empty:
                    pass
            f.flush()

    t = threading.Thread(target=writer_thread, daemon=True)
    t.start()

    start_time = time.time()
    start_utc = datetime.datetime.now(datetime.timezone.utc).isoformat()
    end_time = start_time + duration_seconds

    try:
        while time.time() < end_time:
            data = ser.read(ser.in_waiting or 4096)
            if data:
                try:
                    q.put_nowait(data)
                except queue.Full:
                    # Queue full, emergency drain
                    time.sleep(0.001)
    finally:
        stop_event.set()
        t.join(timeout=5.0)
        ser.close()

    metadata = {
        "capture_started_utc": start_utc,
        "duration_seconds": duration_seconds,
        "pre_capture_discard_seconds": settling_seconds,
        "port": port,
        "format": "COMP971 gateway USB framed binary protocol v1",
        "raw_file": output_path.name,
        "raw_bytes": total_bytes,
    }
    meta_path = output_path.with_suffix(".json")
    meta_path.write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    return metadata

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM16")
    parser.add_argument("--baudrate", type=int, default=115200)
    parser.add_argument("--duration", type=float, default=60.0)
    parser.add_argument("--settling", type=float, default=3.0)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    meta = capture(args.port, args.baudrate, args.duration, args.settling, args.output)
    print(f"Captured {meta['raw_bytes']} bytes to {args.output}")
