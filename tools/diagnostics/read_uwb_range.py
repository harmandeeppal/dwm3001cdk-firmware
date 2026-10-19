"""Read COBS-framed DWM3001CDK range reports from the laptop-connected node.

Supports both legacy single-link reports (type 0x01) and scheduled six-node
mesh reports (type 0x04).
"""

from __future__ import annotations

import argparse
import json
import os
import statistics
import struct
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


# The managed Windows shell can expose extension modules from a second Python
# runtime. Keep the nRF SDK Python and its extension modules together.
sys.path[:] = [
    path
    for path in sys.path
    if not os.path.normcase(path).startswith(os.path.normcase(r"C:\tmp\py312"))
]

import ctypes  # noqa: E402,F401
import serial  # noqa: E402


def decode_cobs(encoded: bytes) -> bytes:
    decoded = bytearray()
    index = 0
    while index < len(encoded):
        code = encoded[index]
        if code == 0:
            raise ValueError("zero byte inside COBS frame")
        index += 1
        end = index + code - 1
        if end > len(encoded):
            raise ValueError("truncated COBS frame")
        decoded.extend(encoded[index:end])
        index = end
        if code != 0xFF and index < len(encoded):
            decoded.append(0)
    return bytes(decoded)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, required=True)
    parser.add_argument("--seconds", type=float, default=10.0)
    parser.add_argument("--source", required=True)
    parser.add_argument("--peer", required=True)
    parser.add_argument("--known-distance-cm", type=float)
    parser.add_argument("--antenna-delay-calibrated", action="store_true")
    parser.add_argument("--tx-antenna-delay", type=int)
    parser.add_argument("--rx-antenna-delay", type=int)
    parser.add_argument("--response-delay-uus", type=int)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--live", action="store_true")
    parser.add_argument("--display-interval", type=float, default=0.25)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    ranges_cm: list[int] = []
    mesh_ranges_cm: dict[int, list[int]] = {}
    mesh_nonpositive_counts: dict[int, int] = {}
    mesh_latest: dict[int, dict[str, int]] = {}
    boot_names: list[str] = []
    bad_frames = 0
    range_report_count = 0
    nonpositive_ranges_cm: list[int] = []
    icm_sample_count = 0
    icm_sample_counts_by_node: dict[int, int] = {}
    icm_sequence_stats: dict[int, dict[str, int]] = {}
    latest_icm: dict[str, int] | None = None
    mesh_debug_counts: dict[str, int] = {}
    mesh_debug_detail_counts: dict[str, int] = {}
    encoded = bytearray()
    started = time.monotonic()
    deadline = None if args.seconds <= 0 else started + args.seconds
    next_display = started

    if args.live:
        print(f"LIVE UWB: {args.source} to {args.peer}")
        print("Press Ctrl+C to stop.\n")
        print(f"Waiting for {args.peer}...", flush=True)

    try:
        with serial.Serial(args.port, args.baud, timeout=0.1) as port:
            port.dtr = True
            port.rts = True
            port.reset_input_buffer()
            while deadline is None or time.monotonic() < deadline:
                byte = port.read(1)
                if not byte:
                    continue
                if byte != b"\x00":
                    encoded.extend(byte)
                    continue
                if not encoded:
                    continue
                try:
                    frame = decode_cobs(bytes(encoded))
                    if len(frame) < 6:
                        raise ValueError("short application frame")
                    frame_type = frame[0]
                    range_cm = struct.unpack_from("<i", frame, 1)[0]
                    payload_length = frame[5]
                    payload = frame[6 : 6 + payload_length]
                    if frame_type == 0x01:
                        range_report_count += 1
                        if range_cm > 0:
                            ranges_cm.append(range_cm)
                            now = time.monotonic()
                            if args.live and now >= next_display:
                                rolling = ranges_cm[-25:]
                                rolling_median = statistics.median(rolling)
                                print(
                                    f"\rDistance: {rolling_median:6.1f} cm  "
                                    f"latest: {range_cm:4d} cm  "
                                    f"samples: {len(ranges_cm):6d}",
                                    end="",
                                    flush=True,
                                )
                                next_display = now + args.display_interval
                        else:
                            nonpositive_ranges_cm.append(range_cm)
                    elif frame_type == 0x04:
                        if len(payload) != 3:
                            raise ValueError("unexpected mesh-range payload length")
                        pair_index, first_node, second_node = payload
                        mesh_latest[pair_index] = {
                            "first_node": first_node,
                            "second_node": second_node,
                            "range_cm": range_cm,
                        }
                        if range_cm > 0:
                            pair_ranges = mesh_ranges_cm.setdefault(pair_index, [])
                            pair_ranges.append(range_cm)
                            now = time.monotonic()
                            if args.live and now >= next_display:
                                rolling = pair_ranges[-25:]
                                rolling_median = statistics.median(rolling)
                                print(
                                    f"\rMESH pair={pair_index:02d} "
                                    f"nodes={first_node}-{second_node}  "
                                    f"distance={rolling_median:6.1f} cm  "
                                    f"latest={range_cm:4d} cm  "
                                    f"samples={len(pair_ranges):5d}",
                                    end="",
                                    flush=True,
                                )
                                next_display = now + args.display_interval
                        else:
                            nonpositive_ranges_cm.append(range_cm)
                            mesh_nonpositive_counts[pair_index] = (
                                mesh_nonpositive_counts.get(pair_index, 0) + 1
                            )
                    elif frame_type == 0xFF:
                        boot_names.append(payload.decode("ascii", errors="replace"))
                    elif frame_type == 0x03:
                        if len(payload) != 23:
                            raise ValueError("unexpected ICM payload length")
                        source_node = payload[0]
                        unpacked = struct.unpack("<IIhhhhhhh", payload[1:])
                        icm_sample_count += 1
                        icm_sample_counts_by_node[source_node] = (
                            icm_sample_counts_by_node.get(source_node, 0) + 1
                        )
                        sequence = unpacked[0]
                        node_stats = icm_sequence_stats.setdefault(
                            source_node,
                            {"first_sequence": sequence, "last_sequence": sequence,
                             "missing_sequence_count": 0},
                        )
                        previous_sequence = node_stats["last_sequence"]
                        if sequence > previous_sequence + 1:
                            node_stats["missing_sequence_count"] += sequence - previous_sequence - 1
                        node_stats["last_sequence"] = sequence
                        latest_icm = {
                            "source_node": source_node,
                            "sequence": unpacked[0],
                            "timestamp_ms": unpacked[1],
                            "ax": unpacked[2], "ay": unpacked[3], "az": unpacked[4],
                            "gx": unpacked[5], "gy": unpacked[6], "gz": unpacked[7],
                            "temperature": unpacked[8],
                        }
                        now = time.monotonic()
                        if args.live and now >= next_display:
                            print(
                                f"\rICM: {icm_sample_count:6d} samples  "
                                f"node={latest_icm['source_node']}  "
                                f"accel=({latest_icm['ax']:+6d},{latest_icm['ay']:+6d},{latest_icm['az']:+6d})  "
                                f"gyro=({latest_icm['gx']:+6d},{latest_icm['gy']:+6d},{latest_icm['gz']:+6d})",
                                end="",
                                flush=True,
                            )
                            next_display = now + args.display_interval
                    elif frame_type == 0xFE:
                        if len(payload) != 6:
                            raise ValueError("unexpected mesh-debug payload length")
                        code, pair_index = payload[:2]
                        detail = struct.unpack_from("<i", payload, 2)[0]
                        key = f"code_{code:02X}_pair_{pair_index}"
                        mesh_debug_counts[key] = mesh_debug_counts.get(key, 0) + 1
                        detail_key = f"{key}_detail_{detail}"
                        mesh_debug_detail_counts[detail_key] = (
                            mesh_debug_detail_counts.get(detail_key, 0) + 1
                        )
                        if args.live:
                            print(
                                f"\rMESH_DEBUG code=0x{code:02X} pair={pair_index} "
                                f"detail={detail}",
                                end="",
                                flush=True,
                            )
                except (ValueError, struct.error):
                    bad_frames += 1
                finally:
                    encoded.clear()
    except KeyboardInterrupt:
        if args.live:
            print("\nStopped.")

    elapsed = time.monotonic() - started

    result: dict[str, object] = {
        "captured_at_utc": datetime.now(timezone.utc).isoformat(),
        "source": args.source,
        "peer": args.peer,
        "port": args.port,
        "baud": args.baud,
        "duration_s": elapsed,
        "sample_count": len(ranges_cm),
        "range_report_count": range_report_count,
        "nonpositive_range_count": len(nonpositive_ranges_cm),
        "nonpositive_ranges_cm": nonpositive_ranges_cm,
        "boot_names": boot_names,
        "bad_frame_count": bad_frames,
        "icm_sample_count": icm_sample_count,
        "icm_sample_counts_by_node": {
            str(node): count for node, count in sorted(icm_sample_counts_by_node.items())
        },
        "icm_sequence_stats_by_node": {
            str(node): values for node, values in sorted(icm_sequence_stats.items())
        },
        "mesh_pair_report_count": sum(len(values) for values in mesh_ranges_cm.values()),
        "mesh_nonpositive_counts": {
            str(pair_index): count
            for pair_index, count in sorted(mesh_nonpositive_counts.items())
        },
        "mesh_debug_counts": mesh_debug_counts,
        "mesh_debug_detail_counts": mesh_debug_detail_counts,
        "antenna_delay_calibrated": args.antenna_delay_calibrated,
    }
    if latest_icm is not None:
        result["latest_icm_raw"] = latest_icm
    if mesh_latest:
        result["mesh_latest_ranges_cm"] = {
            str(pair_index): value for pair_index, value in sorted(mesh_latest.items())
        }

    if any(
        value is not None
        for value in (
            args.tx_antenna_delay,
            args.rx_antenna_delay,
            args.response_delay_uus,
        )
    ):
        result["uwb_settings"] = {
            "tx_antenna_delay": args.tx_antenna_delay,
            "rx_antenna_delay": args.rx_antenna_delay,
            "response_delay_uus": args.response_delay_uus,
        }

    if ranges_cm:
        median_cm = statistics.median(ranges_cm)
        sorted_ranges = sorted(ranges_cm)
        p05_index = int((len(sorted_ranges) - 1) * 0.05)
        p95_index = int((len(sorted_ranges) - 1) * 0.95)
        standard_deviation_cm = statistics.pstdev(ranges_cm)
        result["distance_cm"] = {
            "median": median_cm,
            "mean": statistics.fmean(ranges_cm),
            "standard_deviation": standard_deviation_cm,
            "p05": sorted_ranges[p05_index],
            "p95": sorted_ranges[p95_index],
            "minimum": min(ranges_cm),
            "maximum": max(ranges_cm),
        }
        if args.known_distance_cm is not None:
            result["known_distance_cm"] = args.known_distance_cm
            result["median_error_cm"] = median_cm - args.known_distance_cm
        result["samples_cm"] = ranges_cm
        summary = result["distance_cm"]
        print(f"RANGE_OK samples={len(ranges_cm)}")
        print(
            "DISTANCE_CM "
            f"median={summary['median']} mean={summary['mean']:.2f} "
            f"sd={summary['standard_deviation']:.2f} "
            f"p05={summary['p05']} p95={summary['p95']} "
            f"min={summary['minimum']} max={summary['maximum']}"
        )
        if args.known_distance_cm is not None:
            print(
                "CALIBRATION_CM "
                f"known={args.known_distance_cm:g} "
                f"median_error={median_cm - args.known_distance_cm:+.1f}"
            )
    elif mesh_ranges_cm:
        print(f"MESH_OK pairs={len(mesh_ranges_cm)} samples={sum(len(values) for values in mesh_ranges_cm.values())}")
        for pair_index in sorted(mesh_ranges_cm):
            samples = mesh_ranges_cm[pair_index]
            latest = mesh_latest[pair_index]
            print(
                "MESH_DISTANCE_CM "
                f"pair={pair_index} nodes={latest['first_node']}-{latest['second_node']} "
                f"median={statistics.median(samples):.1f} "
                f"samples={len(samples)}"
            )
        for pair_index in sorted(mesh_nonpositive_counts):
            print(
                "MESH_INVALID "
                f"pair={pair_index} reports={mesh_nonpositive_counts[pair_index]}"
            )
    else:
        print(
            "NO_RANGE_SAMPLES "
            f"reports={range_report_count} "
            f"nonpositive={len(nonpositive_ranges_cm)} "
            f"bad_frames={bad_frames}"
        )

    if icm_sample_count:
        print(f"ICM_OK samples={icm_sample_count}")
        print(
            "ICM_LAST "
            f"node={latest_icm['source_node']} sequence={latest_icm['sequence']} "
            f"timestamp_ms={latest_icm['timestamp_ms']}"
        )

    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(f"OUTPUT={args.output}")

    return 0 if ranges_cm or mesh_ranges_cm else 2


if __name__ == "__main__":
    raise SystemExit(main())
