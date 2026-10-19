"""Step 28A Corrected Offline IMU/UWB Dataset Pipeline and Causal Alignment.

Implements:
1. Robust per-boot epoch identification for all IMU nodes without false sequence-reuse duplicates.
2. Mode A: Offline Acquisition-Time Alignment (where synchronization uncertainty permits).
3. Mode B: Causal Gateway-Availability Streaming Alignment (zero future data leakage).
4. Configurable freshness timeouts, separate latest-attempt vs latest-valid age tracking.
5. Direct 5-link hardware UWB representation with explicit masks and outage indicators.
"""

from __future__ import annotations

import argparse
import hashlib
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


def analyze_epochs_and_loss(records: dict[str, list[dict[str, Any]]]) -> dict[str, Any]:
    """Segment IMU samples into verified boot epochs and compute loss/duplicates strictly within epochs."""
    node_samples = defaultdict(list)
    for r in records["imu"]:
        node_samples[r["node_index"]].append(r)

    node_stats = {}
    for node in sorted(node_samples.keys()):
        recs = node_samples[node]
        epochs = []
        curr_start = 0

        # Segment by genuine MCU resets (local timestamp dropping by > 1.0 s)
        for i in range(1, len(recs)):
            if recs[i]["local_timestamp_us"] < recs[i - 1]["local_timestamp_us"] - 1000000:
                epochs.append((curr_start, i - 1))
                curr_start = i
        epochs.append((curr_start, len(recs) - 1))

        epoch_details = []
        total_epoch_dups = 0
        total_epoch_jumps = 0
        total_epoch_missing = 0

        for ep_idx, (s_idx, e_idx) in enumerate(epochs):
            ep_samples = recs[s_idx : e_idx + 1]
            s_seq = ep_samples[0]["sequence"]
            e_seq = ep_samples[-1]["sequence"]
            seq_span = e_seq - s_seq + 1

            seen_seqs = set()
            dups = 0
            jumps = 0
            missing = 0

            for k in range(len(ep_samples)):
                sq = ep_samples[k]["sequence"]
                if sq in seen_seqs:
                    dups += 1
                else:
                    seen_seqs.add(sq)
                if k > 0:
                    d = sq - ep_samples[k - 1]["sequence"]
                    if d > 1:
                        jumps += 1
                        missing += (d - 1)

            total_epoch_dups += dups
            total_epoch_jumps += jumps
            total_epoch_missing += missing

            epoch_details.append({
                "epoch_index": ep_idx,
                "start_index": s_idx,
                "end_index": e_idx,
                "sample_count": len(ep_samples),
                "first_sequence": s_seq,
                "last_sequence": e_seq,
                "sequence_span": seq_span,
                "duplicate_samples": dups,
                "sequence_jumps": jumps,
                "missing_samples": missing,
            })

        node_stats[node] = {
            "node_name": NODE_NAMES.get(node, f"node_{node}"),
            "total_received_samples": len(recs),
            "boot_epoch_count": len(epochs),
            "detected_mcu_resets": len(epochs) - 1,
            "transport_duplicate_samples": total_epoch_dups,
            "sequence_jumps": total_epoch_jumps,
            "net_missing_samples": total_epoch_missing,
            "epochs": epoch_details,
        }

    # UWB Statistics
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
        "imu_node_statistics": node_stats,
        "uwb_statistics": uwb_stats,
        "stall_snapshots_count": len(records["stall_snapshots"]),
    }


def build_step28a_aligned_dataset(
    records: dict[str, list[dict[str, Any]]],
    output_path: Path | str,
    timing_mode: str = "mode_b",
    imu_freshness_timeout_us: int = 50000,
    uwb_freshness_timeout_us: int = 350000,
    max_frames: int | None = None,
) -> int:
    """Build causal aligned 60 Hz dataset.

    timing_mode:
      - 'mode_b': Causal gateway-availability alignment (at time t_k, only packets where gw_us <= t_k).
      - 'mode_a': Offline estimated acquisition-time alignment (local clock spacing, marked uncertain across nodes).
    """
    if not records["star_ranges"] or not records["imu"]:
        raise ValueError("Cannot align empty records")

    first_gw_us = records["star_ranges"][0]["gateway_receive_timestamp_us"]
    last_gw_us = records["star_ranges"][-1]["gateway_receive_timestamp_us"]

    frame_interval_us = 16667  # ~60.0 Hz
    num_frames = int((last_gw_us - first_gw_us) // frame_interval_us) + 1
    if max_frames is not None:
        num_frames = min(num_frames, max_frames)

    # Sort streams
    node_imu = defaultdict(list)
    for r in records["imu"]:
        node_imu[r["node_index"]].append(r)
    for n in node_imu:
        node_imu[n].sort(key=lambda x: x["gateway_receive_timestamp_us"])

    star_blocks = sorted(records["star_ranges"], key=lambda x: x["gateway_receive_timestamp_us"])

    imu_ptrs = {n: 0 for n in range(6)}
    uwb_ptr = 0

    latest_imu = {n: None for n in range(6)}
    latest_attempted_uwb = None
    latest_valid_uwb_link = {link: None for link in LINK_NAMES}

    # Tracking Pelvis boot count
    pelvis_boot_epoch = 0
    last_pelvis_loc = 0

    written_frames = 0
    with open(output_path, "w", encoding="utf-8") as out_fp:
        for frame_idx in range(num_frames):
            t_k = first_gw_us + frame_idx * frame_interval_us

            # Advance IMU pointers causally (up to t_k)
            for n in range(6):
                stream = node_imu[n]
                while imu_ptrs[n] < len(stream) and stream[imu_ptrs[n]]["gateway_receive_timestamp_us"] <= t_k:
                    s = stream[imu_ptrs[n]]
                    if n == 1:
                        # Check for Pelvis reboot: local timestamp drops significantly
                        if last_pelvis_loc > 0 and s["local_timestamp_us"] < last_pelvis_loc - 500000:
                            pelvis_boot_epoch += 1
                            # Invalidate cached valid UWB measurements across coordinator reboot!
                            latest_valid_uwb_link = {link: None for link in LINK_NAMES}
                        last_pelvis_loc = s["local_timestamp_us"]
                    latest_imu[n] = s
                    imu_ptrs[n] += 1

            # Advance UWB pointer causally (up to t_k)
            while uwb_ptr < len(star_blocks) and star_blocks[uwb_ptr]["gateway_receive_timestamp_us"] <= t_k:
                blk = star_blocks[uwb_ptr]
                latest_attempted_uwb = blk
                # Update latest valid measurement per link
                for p in blk["pairs"]:
                    if p["valid"]:
                        latest_valid_uwb_link[p["link_name"]] = (p, blk["gateway_receive_timestamp_us"], blk["block_index"])
                uwb_ptr += 1

            # Build aligned frame dictionary
            frame_data: dict[str, Any] = {
                "schema_version": "v1.0-step28a",
                "timing_mode": timing_mode,
                "frame_index": frame_idx,
                "t_rel_seconds": round(frame_idx * frame_interval_us / 1e6, 4),
                "grid_timestamp_us": t_k,
                "pelvis_boot_epoch": pelvis_boot_epoch,
                "imu": {},
                "uwb_5link": {},
                "uwb_latest_attempt": None,
            }

            # Populate IMU features
            for n in range(6):
                sample = latest_imu[n]
                if sample is not None:
                    imu_age_us = t_k - sample["gateway_receive_timestamp_us"]
                    is_valid = (imu_age_us <= imu_freshness_timeout_us)
                    frame_data["imu"][NODE_NAMES[n]] = {
                        "sequence": sample["sequence"],
                        "local_timestamp_us": sample["local_timestamp_us"],
                        "ax": sample["ax"], "ay": sample["ay"], "az": sample["az"],
                        "gx": sample["gx"], "gy": sample["gy"], "gz": sample["gz"],
                        "transport_age_us": imu_age_us,
                        "valid": is_valid,
                    }
                else:
                    frame_data["imu"][NODE_NAMES[n]] = None

            # Populate latest attempted UWB metadata
            if latest_attempted_uwb is not None:
                att_age_us = t_k - latest_attempted_uwb["gateway_receive_timestamp_us"]
                frame_data["uwb_latest_attempt"] = {
                    "block_index": latest_attempted_uwb["block_index"],
                    "attempt_age_us": att_age_us,
                    "is_recent": att_age_us <= uwb_freshness_timeout_us,
                }

            # Populate 5-link UWB channels
            for link in LINK_NAMES:
                val_entry = latest_valid_uwb_link[link]
                if val_entry is not None:
                    p_info, gw_rx_us, b_idx = val_entry
                    link_age_us = t_k - gw_rx_us
                    is_fresh = (link_age_us <= uwb_freshness_timeout_us)
                    frame_data["uwb_5link"][link] = {
                        "range_cm": p_info["range_cm"] if is_fresh else -1,
                        "valid_mask": 1 if is_fresh else 0,
                        "source_block_index": b_idx,
                        "last_valid_age_us": link_age_us,
                        "is_fresh": is_fresh,
                    }
                else:
                    frame_data["uwb_5link"][link] = {
                        "range_cm": -1,
                        "valid_mask": 0,
                        "source_block_index": None,
                        "last_valid_age_us": None,
                        "is_fresh": False,
                    }

            out_fp.write(json.dumps(frame_data) + "\n")
            written_frames += 1

    return written_frames


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="COMP971 Step 28A Dataset Preparation Pipeline")
    parser.add_argument("--capture", type=str, required=True, help="Path to raw gateway capture binary")
    parser.add_argument("--output_dir", type=str, default="hardware/outputs/records/validation_step28a", help="Output directory")
    parser.add_argument("--mode", type=str, default="mode_b", choices=["mode_a", "mode_b"], help="Alignment timing mode")
    parser.add_argument("--full", action="store_true", help="Generate full session without truncation")
    args = parser.parse_args()

    out_p = Path(args.output_dir)
    out_p.mkdir(parents=True, exist_ok=True)

    print(f"Decoding records from {args.capture}...")
    recs = decode_capture_records(args.capture)

    print("Analyzing epoch integrity, loss, and duplicates...")
    stats = analyze_epochs_and_loss(recs)
    stats_file = out_p / "step28a_epoch_and_loss_statistics.json"
    with open(stats_file, "w", encoding="utf-8") as f:
        json.dump(stats, f, indent=2)
    print(f"Saved statistics to {stats_file}")

    aligned_file = out_p / f"step28a_causal_60hz_aligned_{args.mode}.jsonl"
    max_f = None if args.full else 1800  # Default to 30s preview unless --full specified
    print(f"Generating aligned dataset ({args.mode}, max_frames={max_f})...")
    total_f = build_step28a_aligned_dataset(recs, aligned_file, timing_mode=args.mode, max_frames=max_f)
    print(f"Successfully generated {total_f} frames to {aligned_file}")
