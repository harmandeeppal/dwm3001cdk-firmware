"""Step 28 Offline IMU/UWB Dataset Validation and Causal Alignment Pipeline.

Processes raw gateway binary captures (COMP971 protocol v1), extracts all IMU
and FiRa star-ranging streams, computes rigorous synchronisation and data quality
metrics, and generates a causal 60 Hz aligned multimodal representation.
"""

from __future__ import annotations

import argparse
import json
import struct
from collections import defaultdict
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

MAGIC = b"\xc9\x71"
HEADER_SIZE = 6
IMU_TYPE = 0x01
STAR_RANGES_TYPE = 0x07
STALL_SNAPSHOT_TYPE = 0x06

NODE_NAMES = {
    0: "head",
    1: "pelvis",
    2: "left_wrist",
    3: "right_wrist",
    4: "left_ankle",
    5: "right_ankle",
}

LINK_NAMES = [
    "pelvis_head",         # Pair 0 (Node 1 <-> Node 0)
    "pelvis_left_wrist",   # Pair 1 (Node 1 <-> Node 2)
    "pelvis_right_wrist",  # Pair 2 (Node 1 <-> Node 3)
    "pelvis_left_ankle",   # Pair 3 (Node 1 <-> Node 4)
    "pelvis_right_ankle",  # Pair 4 (Node 1 <-> Node 5)
]


def decode_capture_records(bin_path: Path | str) -> dict[str, list[dict[str, Any]]]:
    """Parse raw binary capture file into typed record streams."""
    with open(bin_path, "rb") as fp:
        data = fp.read()

    index = 0
    records: dict[str, list[dict[str, Any]]] = {
        "imu": [],
        "star_ranges": [],
        "stall_snapshots": [],
    }

    while index + HEADER_SIZE <= len(data):
        if data[index : index + 2] != MAGIC:
            index += 1
            continue
        version, record_type, payload_len = struct.unpack_from("<BBH", data, index + 2)
        if version != 1 or payload_len > 72:
            index += 1
            continue
        end = index + HEADER_SIZE + payload_len
        if end > len(data):
            break
        payload = data[index + HEADER_SIZE : end]
        index = end

        if record_type == IMU_TYPE:
            node_idx = payload[2]
            sync_epoch = struct.unpack_from("<I", payload, 4)[0]
            seq, local_us, ax, ay, az, gx, gy, gz, temp = struct.unpack_from(
                "<IIhhhhhhh", payload, 8
            )
            gw_us = struct.unpack_from("<Q", payload, 30)[0]
            records["imu"].append({
                "node_index": node_idx,
                "sync_epoch": sync_epoch,
                "sequence": seq,
                "local_timestamp_us": local_us,
                "ax": ax, "ay": ay, "az": az,
                "gx": gx, "gy": gy, "gz": gz,
                "temperature": temp,
                "gateway_receive_timestamp_us": gw_us,
            })
        elif record_type == STAR_RANGES_TYPE:
            node_idx = payload[2]
            pelvis_us, block_idx = struct.unpack_from("<II", payload, 4)
            pairs = []
            for p in range(5):
                offset = 12 + p * 4
                rcm, status, flags = struct.unpack_from("<hBB", payload, offset)
                valid = bool(flags & 1) and rcm >= 0
                pairs.append({
                    "pair_index": p,
                    "link_name": LINK_NAMES[p],
                    "range_cm": rcm,
                    "status": status,
                    "flags": flags,
                    "valid": valid,
                })
            gw_us = struct.unpack_from("<Q", payload, 32)[0]
            records["star_ranges"].append({
                "source_node": node_idx,
                "pelvis_timestamp_us": pelvis_us,
                "block_index": block_idx,
                "pairs": pairs,
                "gateway_receive_timestamp_us": gw_us,
            })
        elif record_type == STALL_SNAPSHOT_TYPE:
            magic, boot_cnt, resetreas, esc_stage = struct.unpack_from("<III B", payload, 4)
            stall_ms, last_blk, min_lead, late_tx = struct.unpack_from("<IIiI", payload, 20)
            records["stall_snapshots"].append({
                "boot_count": boot_cnt,
                "reset_reason": f"0x{resetreas:08X}",
                "escalation_reached": esc_stage,
                "stall_duration_ms": stall_ms,
                "last_block_index": last_blk,
                "min_lead_dtu": min_lead,
                "late_tx_count": late_tx,
                "gateway_receive_timestamp_us": struct.unpack_from("<Q", payload, 32)[0] if len(payload) >= 40 else 0,
            })

    return records


def analyze_dataset_integrity(records: dict[str, list[dict[str, Any]]]) -> dict[str, Any]:
    """Calculate audit statistics across IMU, UWB, and reset events."""
    # Group IMU by node
    node_imu = defaultdict(list)
    for r in records["imu"]:
        node_imu[r["node_index"]].append(r)

    imu_stats = {}
    for node in sorted(node_imu.keys()):
        samples = node_imu[node]
        seqs = [s["sequence"] for s in samples]
        first_seq, last_seq = seqs[0], seqs[-1]
        span = last_seq - first_seq + 1

        # Check sequence differences and duplicates
        duplicates = 0
        jumps = 0
        missing_count = 0
        resets = 0
        seen_seqs = set()
        for i in range(len(samples)):
            s = samples[i]
            if s["sequence"] in seen_seqs:
                duplicates += 1
            else:
                seen_seqs.add(s["sequence"])
            if i > 0:
                diff = s["sequence"] - samples[i - 1]["sequence"]
                if diff < 0:
                    # Require both negative sequence and significant local timestamp jump backwards (> 1 second)
                    time_jump_back_us = samples[i - 1]["local_timestamp_us"] - s["local_timestamp_us"]
                    if time_jump_back_us > 1000000:
                        resets += 1
                elif diff > 1:
                    jumps += 1
                    missing_count += (diff - 1)

        imu_stats[node] = {
            "node_name": NODE_NAMES.get(node, f"node_{node}"),
            "received_samples": len(samples),
            "first_sequence": first_seq,
            "last_sequence": last_seq,
            "span_sequences": span if resets == 0 else "multiboot",
            "duplicate_samples": duplicates,
            "sequence_jumps": jumps,
            "missing_samples": missing_count,
            "detected_mcu_resets": resets,
        }

    # UWB link validity
    star_blocks = records["star_ranges"]
    link_valid = [0] * 5
    for b in star_blocks:
        for p in b["pairs"]:
            if p["valid"]:
                link_valid[p["pair_index"]] += 1

    uwb_stats = {
        "received_blocks": len(star_blocks),
        "evaluated_slots_total": len(star_blocks) * 5,
        "valid_measurements_total": sum(link_valid),
        "aggregate_validity_percent": round(sum(link_valid) / (len(star_blocks) * 5) * 100, 2),
        "per_link": {},
    }
    for p in range(5):
        uwb_stats["per_link"][LINK_NAMES[p]] = {
            "valid_count": link_valid[p],
            "total_opportunities": len(star_blocks),
            "validity_percent": round(link_valid[p] / len(star_blocks) * 100, 2),
        }

    return {
        "imu_statistics": imu_stats,
        "uwb_statistics": uwb_stats,
        "stall_snapshots_count": len(records["stall_snapshots"]),
    }


def generate_causal_60hz_dataset(
    records: dict[str, list[dict[str, Any]]],
    output_jsonl: Path | str,
    max_frames: int | None = None,
) -> int:
    """Produce causal 60 Hz aligned dataset with explicit freshness and validity masks.

    Alignment Grid:
    Reference clock is the Gateway microsecond timeline (gw_us) at 60.0 Hz (16,666.67 us).
    For each frame k:
    - IMU features: Most recently received sample from each node strictly before or at t_k.
    - UWB features: Most recently received Star Ranges block strictly before or at t_k.
    - Freshness: Age of UWB range in microseconds (t_k - UWB_receive_time).
    - Validity Mask: 1 if within freshness timeout (e.g. 350 ms) and valid flag True; else 0.
    """
    if not records["star_ranges"] or not records["imu"]:
        raise ValueError("Cannot align empty records")

    first_gw_us = records["star_ranges"][0]["gateway_receive_timestamp_us"]
    last_gw_us = records["star_ranges"][-1]["gateway_receive_timestamp_us"]

    frame_interval_us = 16667  # ~60.0 Hz
    num_frames = int((last_gw_us - first_gw_us) // frame_interval_us) + 1
    if max_frames is not None:
        num_frames = min(num_frames, max_frames)

    # Sort IMU streams by gw_us
    node_imu = defaultdict(list)
    for r in records["imu"]:
        node_imu[r["node_index"]].append(r)
    for n in node_imu:
        node_imu[n].sort(key=lambda x: x["gateway_receive_timestamp_us"])

    star_blocks = sorted(records["star_ranges"], key=lambda x: x["gateway_receive_timestamp_us"])

    # Iterative causal forward pointers
    imu_ptrs = {n: 0 for n in range(6)}
    uwb_ptr = 0

    latest_imu = {n: None for n in range(6)}
    latest_uwb = None

    UWB_FRESHNESS_TIMEOUT_US = 350000  # 350 ms (1.75x 200 ms block period)

    written_frames = 0
    with open(output_jsonl, "w", encoding="utf-8") as out_fp:
        for frame_idx in range(num_frames):
            t_k = first_gw_us + frame_idx * frame_interval_us

            # Advance IMU pointers causally (up to t_k)
            for n in range(6):
                stream = node_imu[n]
                while imu_ptrs[n] < len(stream) and stream[imu_ptrs[n]]["gateway_receive_timestamp_us"] <= t_k:
                    latest_imu[n] = stream[imu_ptrs[n]]
                    imu_ptrs[n] += 1

            # Advance UWB pointer causally (up to t_k)
            while uwb_ptr < len(star_blocks) and star_blocks[uwb_ptr]["gateway_receive_timestamp_us"] <= t_k:
                latest_uwb = star_blocks[uwb_ptr]
                uwb_ptr += 1

            # Build causal frame dictionary
            frame_data: dict[str, Any] = {
                "frame_index": frame_idx,
                "t_rel_seconds": round(frame_idx * frame_interval_us / 1e6, 4),
                "grid_timestamp_us": t_k,
                "imu": {},
                "uwb_5link": {},
            }

            for n in range(6):
                sample = latest_imu[n]
                if sample is not None:
                    imu_age_us = t_k - sample["gateway_receive_timestamp_us"]
                    frame_data["imu"][NODE_NAMES[n]] = {
                        "sequence": sample["sequence"],
                        "ax": sample["ax"], "ay": sample["ay"], "az": sample["az"],
                        "gx": sample["gx"], "gy": sample["gy"], "gz": sample["gz"],
                        "age_us": imu_age_us,
                        "valid": imu_age_us <= 50000,  # Valid if received within 50 ms
                    }
                else:
                    frame_data["imu"][NODE_NAMES[n]] = None

            if latest_uwb is not None:
                uwb_age_us = t_k - latest_uwb["gateway_receive_timestamp_us"]
                is_fresh = uwb_age_us <= UWB_FRESHNESS_TIMEOUT_US
                frame_data["uwb_block_index"] = latest_uwb["block_index"]
                frame_data["uwb_age_us"] = uwb_age_us

                for p in latest_uwb["pairs"]:
                    link = p["link_name"]
                    is_valid = is_fresh and p["valid"]
                    frame_data["uwb_5link"][link] = {
                        "range_cm": p["range_cm"] if is_valid else -1,
                        "valid_mask": 1 if is_valid else 0,
                        "raw_range_cm": p["range_cm"],
                        "raw_valid": p["valid"],
                        "is_fresh": is_fresh,
                    }
            else:
                frame_data["uwb_block_index"] = None
                frame_data["uwb_age_us"] = None
                for link in LINK_NAMES:
                    frame_data["uwb_5link"][link] = {
                        "range_cm": -1,
                        "valid_mask": 0,
                        "raw_range_cm": -1,
                        "raw_valid": False,
                        "is_fresh": False,
                    }

            out_fp.write(json.dumps(frame_data) + "\n")
            written_frames += 1

    return written_frames


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="COMP971 Step 28 Dataset Preparation Pipeline")
    parser.add_argument("--capture", type=str, required=True, help="Path to raw gateway capture binary")
    parser.add_argument("--output_dir", type=str, default="hardware/outputs/records/validation_step28", help="Output directory")
    args = parser.parse_args()

    out_p = Path(args.output_dir)
    out_p.mkdir(parents=True, exist_ok=True)

    print(f"Decoding records from {args.capture}...")
    recs = decode_capture_records(args.capture)

    print("Analyzing dataset integrity...")
    stats = analyze_dataset_integrity(recs)
    stats_path = out_p / "step28_benchmark_statistics.json"
    with open(stats_path, "w", encoding="utf-8") as f:
        json.dump(stats, f, indent=2)
    print(f"Statistics written to {stats_path}")

    aligned_path = out_p / "step28_causal_60hz_aligned_preview.jsonl"
    print(f"Generating causal 60 Hz aligned dataset preview (first 1,800 frames = 30 seconds)...")
    n_frames = generate_causal_60hz_dataset(recs, aligned_path, max_frames=1800)
    print(f"Successfully generated {n_frames} aligned frames to {aligned_path}")
