"""Unit tests for Step 28 offline dataset validation and causal alignment pipeline."""

from __future__ import annotations

import json
import sys
import pytest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from hardware.tools.pipelines.prepare_step28_dataset import (
    decode_capture_records,
    analyze_dataset_integrity,
    generate_causal_60hz_dataset,
    LINK_NAMES,
    NODE_NAMES,
)


CAPTURE_PATH = Path("hardware/outputs/records/acquisition/gateway_raw_2026-10-12_101320.bin")


@pytest.mark.skipif(not CAPTURE_PATH.exists(), reason="Step 19 capture binary required")
def test_reproduce_step19_benchmark_statistics():
    """Verify that the parser reproduces the exact Step 19 ground truth benchmark numbers."""
    records = decode_capture_records(CAPTURE_PATH)
    stats = analyze_dataset_integrity(records)

    # UWB Verification
    uwb = stats["uwb_statistics"]
    assert uwb["received_blocks"] == 8871
    assert uwb["evaluated_slots_total"] == 44355
    assert uwb["valid_measurements_total"] == 35389
    assert uwb["aggregate_validity_percent"] == 79.79

    per_link = uwb["per_link"]
    assert per_link["pelvis_head"]["valid_count"] == 8829
    assert per_link["pelvis_head"]["validity_percent"] == 99.53

    assert per_link["pelvis_left_wrist"]["valid_count"] == 6411
    assert per_link["pelvis_left_wrist"]["validity_percent"] == 72.27

    assert per_link["pelvis_right_wrist"]["valid_count"] == 5530
    assert per_link["pelvis_right_wrist"]["validity_percent"] == 62.34

    assert per_link["pelvis_left_ankle"]["valid_count"] == 6103
    assert per_link["pelvis_left_ankle"]["validity_percent"] == 68.80

    assert per_link["pelvis_right_ankle"]["valid_count"] == 8516
    assert per_link["pelvis_right_ankle"]["validity_percent"] == 96.00

    # IMU Node Counts
    imu = stats["imu_statistics"]
    assert imu[0]["received_samples"] == 105882
    assert imu[1]["received_samples"] == 106868
    assert imu[2]["received_samples"] == 107586
    assert imu[3]["received_samples"] == 107080
    assert imu[4]["received_samples"] == 107602
    assert imu[5]["received_samples"] == 105906

    # Node 2 duplicate and reset verification
    assert imu[2]["duplicate_samples"] == 6
    assert imu[2]["missing_samples"] == 420
    assert imu[2]["detected_mcu_resets"] == 0

    # Pelvis resets verification
    assert imu[1]["detected_mcu_resets"] == 2


def test_causal_no_future_leakage(tmp_path: Path):
    """Verify that aligned output only uses past or concurrent data and enforces age validity."""
    synthetic_records = {
        "imu": [
            {
                "node_index": 0, "sequence": 1, "sync_epoch": 0,
                "local_timestamp_us": 1000, "gateway_receive_timestamp_us": 1000,
                "ax": 1, "ay": 2, "az": 3, "gx": 4, "gy": 5, "gz": 6, "temperature": 25
            },
            {
                "node_index": 0, "sequence": 2, "sync_epoch": 0,
                "local_timestamp_us": 20000, "gateway_receive_timestamp_us": 20000,
                "ax": 10, "ay": 20, "az": 30, "gx": 40, "gy": 50, "gz": 60, "temperature": 25
            }
        ],
        "star_ranges": [
            {
                "source_node": 1, "pelvis_timestamp_us": 1000, "block_index": 1,
                "gateway_receive_timestamp_us": 1000,
                "pairs": [
                    {"pair_index": 0, "link_name": "pelvis_head", "range_cm": 150, "status": 0, "flags": 1, "valid": True},
                    {"pair_index": 1, "link_name": "pelvis_left_wrist", "range_cm": -1, "status": 1, "flags": 0, "valid": False},
                    {"pair_index": 2, "link_name": "pelvis_right_wrist", "range_cm": 160, "status": 0, "flags": 1, "valid": True},
                    {"pair_index": 3, "link_name": "pelvis_left_ankle", "range_cm": 170, "status": 0, "flags": 1, "valid": True},
                    {"pair_index": 4, "link_name": "pelvis_right_ankle", "range_cm": 180, "status": 0, "flags": 1, "valid": True},
                ]
            },
            {
                "source_node": 1, "pelvis_timestamp_us": 201000, "block_index": 2,
                "gateway_receive_timestamp_us": 201000,
                "pairs": [
                    {"pair_index": 0, "link_name": "pelvis_head", "range_cm": 152, "status": 0, "flags": 1, "valid": True},
                    {"pair_index": 1, "link_name": "pelvis_left_wrist", "range_cm": -1, "status": 1, "flags": 0, "valid": False},
                    {"pair_index": 2, "link_name": "pelvis_right_wrist", "range_cm": 162, "status": 0, "flags": 1, "valid": True},
                    {"pair_index": 3, "link_name": "pelvis_left_ankle", "range_cm": 172, "status": 0, "flags": 1, "valid": True},
                    {"pair_index": 4, "link_name": "pelvis_right_ankle", "range_cm": 182, "status": 0, "flags": 1, "valid": True},
                ]
            }
        ],
        "stall_snapshots": []
    }

    out_file = tmp_path / "test_aligned.jsonl"
    count = generate_causal_60hz_dataset(synthetic_records, out_file, max_frames=2)
    assert count == 2

    with open(out_file, "r") as f:
        lines = [json.loads(line) for line in f]

    # Frame 0 at t=1000us
    f0 = lines[0]
    assert f0["grid_timestamp_us"] == 1000
    assert f0["imu"]["head"]["sequence"] == 1
    assert f0["uwb_5link"]["pelvis_head"]["range_cm"] == 150
    assert f0["uwb_5link"]["pelvis_head"]["valid_mask"] == 1
    assert f0["uwb_5link"]["pelvis_left_wrist"]["valid_mask"] == 0

    # Frame 1 at t=1000 + 16667 = 17667us: sequence 2 (arriving at 20000us) must NOT be present!
    f1 = lines[1]
    assert f1["grid_timestamp_us"] == 17667
    assert f1["imu"]["head"]["sequence"] == 1  # Still sequence 1, NO future leakage
    assert f1["imu"]["head"]["age_us"] == 16667
