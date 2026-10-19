"""Decode the loss-aware COMP971 gateway USB binary stream (protocol v1)."""

from __future__ import annotations

import argparse
import json
import struct
from collections import defaultdict
from pathlib import Path
from typing import Iterator

MAGIC = b"\xc9\x71"
HEADER_SIZE = 6
MAX_PAYLOAD = 72
IMU_TYPE = 0x01
CLOCK_STATUS_TYPE = 0x02
UWB_TYPE = 0x03
NODE_STATUS_TYPE = 0x04
UWB_DIAGNOSTIC_TYPE = 0x05
STALL_SNAPSHOT_TYPE = 0x06
STAR_RANGES_TYPE = 0x07
GATEWAY_STATUS_TYPE = 0x80


def frames(data: bytes) -> tuple[Iterator[tuple[int, bytes]], int]:
    """Yield valid frames and return the byte count skipped while re-syncing."""
    index = 0
    skipped = 0
    parsed: list[tuple[int, bytes]] = []
    while index + HEADER_SIZE <= len(data):
        if data[index:index + 2] != MAGIC:
            index += 1
            skipped += 1
            continue
        version, record_type, payload_length = struct.unpack_from("<BBH", data, index + 2)
        if version != 1 or payload_length > MAX_PAYLOAD:
            index += 1
            skipped += 1
            continue
        end = index + HEADER_SIZE + payload_length
        if end > len(data):
            break
        parsed.append((record_type, data[index + HEADER_SIZE:end]))
        index = end
    return iter(parsed), skipped + (len(data) - index)


def as_imu(payload: bytes) -> dict:
    if len(payload) != 38 or payload[0] != 1 or payload[1] != IMU_TYPE:
        raise ValueError("invalid IMU record")
    sync_epoch = struct.unpack_from("<I", payload, 4)[0]
    sequence, local_us, ax, ay, az, gx, gy, gz, temperature = struct.unpack_from(
        "<IIhhhhhhh", payload, 8
    )
    received_us = struct.unpack_from("<Q", payload, 30)[0]
    return {
        "node_index": payload[2], "sync_epoch": sync_epoch, "sequence": sequence,
        "local_timestamp_us": local_us, "ax": ax, "ay": ay, "az": az,
        "gx": gx, "gy": gy, "gz": gz, "temperature": temperature,
        "gateway_receive_timestamp_us": received_us,
    }


def as_uwb(payload: bytes) -> dict:
    if len(payload) != 24 or payload[0] != 1 or payload[1] != UWB_TYPE:
        raise ValueError("invalid UWB record")
    local_us, range_cm, quality_flags = struct.unpack_from("<IiH", payload, 4)
    return {
        "source_node": payload[2], "pair_index": payload[3],
        "local_timestamp_us": local_us,
        "pelvis_timestamp_us": local_us, # retained for backwards compatibility
        "range_cm": range_cm,
        "quality_flags": quality_flags,
        "gateway_receive_timestamp_us": struct.unpack_from("<Q", payload, 16)[0],
    }


def as_star_ranges(payload: bytes) -> dict:
    if len(payload) != 40 or payload[0] != 1 or payload[1] != STAR_RANGES_TYPE:
        raise ValueError("invalid star-ranges record")
    node_idx = payload[2]
    pelvis_us, block_idx = struct.unpack_from("<II", payload, 4)
    pairs = []
    for p in range(5):
        offset = 12 + p * 4
        range_cm, status, flags = struct.unpack_from("<hBB", payload, offset)
        pairs.append({
            "pair_index": p,
            "range_cm": range_cm,
            "status": status,
            "valid": bool(flags & 1) and range_cm >= 0,
        })
    gateway_us = struct.unpack_from("<Q", payload, 32)[0]
    return {
        "source_node": node_idx,
        "pelvis_timestamp_us": pelvis_us,
        "block_index": block_idx,
        "pairs": pairs,
        "gateway_receive_timestamp_us": gateway_us,
    }


def as_node_status(payload: bytes) -> dict:
    if len(payload) != 60 or payload[0] != 1 or payload[1] != NODE_STATUS_TYPE:
        raise ValueError("invalid node-status record")
    fields = struct.unpack_from("<12I", payload, 4)
    names = (
        "sync_epoch", "imu_samples_acquired", "imu_queue_put_ok", "imu_queue_dropped",
        "imu_queue_high_water", "ble_imu_notify_ok", "ble_imu_notify_failed",
        "ble_uwb_notify_failed",
        "uwb_range_attempts", "uwb_range_valid", "uwb_range_invalid", "uwb_result_timeout",
    )
    record = {"node_index": payload[2], "flags": payload[3]}
    record.update(dict(zip(names, fields, strict=True)))
    record["gateway_receive_timestamp_us"] = struct.unpack_from("<Q", payload, 52)[0]
    return record


def as_uwb_diagnostic(payload: bytes) -> dict:
    if len(payload) != 24 or payload[0] != 1 or payload[1] != UWB_DIAGNOSTIC_TYPE:
        raise ValueError("invalid UWB diagnostic record")
    stage_code = payload[4]
    stage_names = {
        0x20: "poll_tx_failed", 0x21: "response_wait_failed",
        0x22: "response_bad_length", 0x23: "response_wrong_source_or_type",
        0x25: "final_tx_failed", 0x30: "responder_poll_not_received",
        0x32: "responder_response_tx_failed", 0x33: "responder_final_not_received",
        0x40: "coordinator_result_timeout", 0x41: "coordinator_command_failed",
        0x42: "responder_reported_invalid_range", 0x43: "responder_range_calculation_failed",
        # Lifecycle Progress Milestones
        0xC1: "prog_mac_ready", 0xC2: "prog_sched_ready", 0xC3: "prog_session_ready",
        0xC4: "prog_mac_started", 0xC5: "prog_session_started",
        # Lifecycle Error Stage Codes
        0xD1: "err_mac_init", 0xD2: "err_helper_open", 0xD3: "err_scheduler",
        0xD4: "err_measurement", 0xD5: "err_session_init", 0xD6: "err_session_params",
        0xD7: "err_add_controlee", 0xD8: "err_mac_start", 0xD9: "err_session_start",
        0xEE: "session_status_reason",
        # Periodic Health Telemetry Metrics (B0..B7, C8..CF - NO collision with D1..D9)
        0xB0: "dw3000_tx_count",
        0xB1: "interval_min_lead",
        0xB2: "dw3000_late_tx_count",
        0xB3: "fira_stall_recovery_count",
        0xB4: "fira_steady_fail_count",
        0xB5: "fira_steady_total",
        0xB6: "fira_last_block_index",
        0xB7: "fira_lifetime_min_lead",
        0xB8: "dw3000_sys_status_lo",
        0xB9: "dw3000_sys_status_hi",
        0xBA: "fira_config_hash",
        0xC8: "uwb_task_free_stack_bytes",
        0xC9: "imu_task_free_stack_bytes",
        0xCA: "status_task_free_stack_bytes",
        0xCB: "disconnect_and_flush_counts",
        0xCC: "connect_block_index",
        0xCD: "fail_ring_last_block",
        0xCE: "fail_ring_last_status",
        0xCF: "connect_count_and_queue_depth",
        # Frozen Stall Diagnostic Indicators (E0..EB) - Associated with pre-reset stall
        0xE0: "stall_dw3000_irq_pin_level",
        0xE1: "stall_app_uwb_task_state",
        0xE2: "stall_dw3000_tx_count",
        0xE3: "stall_fira_cb_seq",
        0xE4: "stall_dw3000_irq_count",
        0xE5: "stall_cb_last_irq_count",
        0xE6: "stall_cb_last_tx_count",
        0xE7: "stall_last_tx_info",
        0xE8: "stall_last_tx_sys_state",
        0xE9: "stall_last_tx_status",
        0xEA: "stall_last_tx_diff",
        0xEB: "stall_last_tx_seq",
    }
    block_index_snapshot = struct.unpack_from("<H", payload, 5)[0]
    detail = struct.unpack_from("<i", payload, 12)[0]
    rec = {
        "node_index": payload[2], "pair_index": payload[3],
        "stage_code": stage_code,
        "stage": stage_names.get(stage_code, "unknown"),
        "block_index_snapshot": block_index_snapshot,
        "local_timestamp_us": struct.unpack_from("<I", payload, 8)[0],
        "detail": detail,
        "gateway_receive_timestamp_us": struct.unpack_from("<Q", payload, 16)[0],
    }
    if stage_code == 0xE1:
        # FreeRTOS eTaskState mapping: eRunning=0, eReady=1, eBlocked=2, eSuspended=3, eDeleted=4
        task_states = {0: "eRunning", 1: "eReady", 2: "eBlocked", 3: "eSuspended", 4: "eDeleted"}
        rec["task_state_name"] = task_states.get(detail, "eInvalid")
    elif stage_code == 0xE7:
        # Unpack [31:24]fastcmd [23:16]mode [15:8]result [7:0]hpdwarn
        uval = struct.unpack_from("<I", payload, 12)[0]
        rec["tx_fastcmd"] = (uval >> 24) & 0xFF
        rec["tx_mode"] = (uval >> 16) & 0xFF
        rec["tx_result"] = (uval >> 8) & 0xFF
        rec["tx_hpdwarn_seen"] = uval & 0xFF
    elif stage_code == 0xE8:
        val = struct.unpack_from('<I', payload, 12)[0]
        rec["tx_sys_state"] = "UNAVAILABLE" if val == 0xFFFFFFFF else f"0x{val:08X}"
    elif stage_code == 0xE9:
        rec["tx_sys_status_b3"] = f"0x{struct.unpack_from('<I', payload, 12)[0]:02X}"
    return rec


def as_stall_snapshot(payload: bytes) -> dict:
    if len(payload) not in (52, 56, 60, 64) or payload[0] != 1 or payload[1] != STALL_SNAPSHOT_TYPE:
        raise ValueError("invalid stall-snapshot record")
    magic, boot_cnt, resetreas, esc_stage = struct.unpack_from("<III B", payload, 4)
    stall_ms, last_blk, min_lead, late_tx = struct.unpack_from("<IIiI", payload, 20)
    cb0_blk, cb0_status, cb0_lead_ms, cb0_offset = struct.unpack_from("<HBBH", payload, 36)
    cb1_blk, cb1_status, cb1_lead_ms, cb1_offset = struct.unpack_from("<HBBH", payload, 44)
    trigger_flags = payload[3]
    crc = struct.unpack_from("<I", payload, 52)[0]
    return {
        "node_index": payload[2],
        "trigger_flags": trigger_flags,
        "magic": f"0x{magic:08X}",
        "boot_count": boot_cnt,
        "reset_reason": f"0x{resetreas:08X}",
        "escalation_reached": esc_stage,
        "stall_duration_ms": stall_ms,
        "last_block_index": last_blk,
        "min_lead_dtu": min_lead,
        "late_tx_count": late_tx,
        "recent_callbacks": [
            {"block_index": cb0_blk, "status": cb0_status, "lead_ms": cb0_lead_ms, "tick_offset_ms": cb0_offset},
            {"block_index": cb1_blk, "status": cb1_status, "lead_ms": cb1_lead_ms, "tick_offset_ms": cb1_offset},
        ],
        "crc32": f"0x{crc:08X}",
    }


def as_gateway_status(payload: bytes) -> dict:
    if len(payload) not in (24, 32) or payload[0] != 1 or payload[1] != GATEWAY_STATUS_TYPE:
        raise ValueError("invalid gateway-status record")
    values = struct.unpack_from("<5I", payload, 4)
    res = {
        "node_index": payload[2],
        "last_disconnect_reason": payload[3],
        "gateway_packets_received": values[0],
        "gateway_bad_packets": values[1],
        "gateway_sequence_gaps": values[2],
        "usb_records_dropped": values[3],
        "usb_queue_high_water": values[4],
    }
    if len(payload) >= 32:
        ext = struct.unpack_from("<2H2BH", payload, 24)
        res.update({
            "gateway_connects": ext[0],
            "gateway_disconnects": ext[1],
            "connected_count": ext[2],
            "scan_active": ext[3],
            "connecting_active": ext[4],
        })
    return res


def write_ndjson(path: Path, records: list[dict]) -> None:
    with path.open("w", encoding="utf-8", newline="\n") as target:
        for record in records:
            target.write(json.dumps(record, separators=(",", ":")) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path, help="raw .bin file from capture_gateway_raw.ps1")
    parser.add_argument("--output-directory", type=Path)
    args = parser.parse_args()
    output = args.output_directory or args.capture.with_name(args.capture.stem + "_decoded")
    output.mkdir(parents=True, exist_ok=True)

    stream, skipped_bytes = frames(args.capture.read_bytes())
    imu: list[dict] = []
    uwb: list[dict] = []
    star_ranges: list[dict] = []
    node_status: list[dict] = []
    uwb_diagnostics: list[dict] = []
    stall_snapshots: list[dict] = []
    gateway_status: list[dict] = []
    invalid_frames = 0
    for record_type, payload in stream:
        try:
            if record_type == IMU_TYPE:
                imu.append(as_imu(payload))
            elif record_type == UWB_TYPE:
                uwb.append(as_uwb(payload))
            elif record_type == STAR_RANGES_TYPE:
                star_ranges.append(as_star_ranges(payload))
            elif record_type == NODE_STATUS_TYPE:
                node_status.append(as_node_status(payload))
            elif record_type == UWB_DIAGNOSTIC_TYPE:
                uwb_diagnostics.append(as_uwb_diagnostic(payload))
            elif record_type == STALL_SNAPSHOT_TYPE:
                stall_snapshots.append(as_stall_snapshot(payload))
            elif record_type == GATEWAY_STATUS_TYPE:
                gateway_status.append(as_gateway_status(payload))
            else:
                invalid_frames += 1
        except ValueError:
            invalid_frames += 1

    gaps = defaultdict(int)
    previous: dict[int, int] = {}
    for sample in imu:
        node = sample["node_index"]
        if node in previous and sample["sequence"] > previous[node] + 1:
            gaps[node] += sample["sequence"] - previous[node] - 1
        previous[node] = sample["sequence"]

    write_ndjson(output / "imu.ndjson", imu)
    write_ndjson(output / "uwb.ndjson", uwb)
    write_ndjson(output / "star_ranges.ndjson", star_ranges)
    write_ndjson(output / "node_status.ndjson", node_status)
    write_ndjson(output / "uwb_diag.ndjson", uwb_diagnostics)
    write_ndjson(output / "stall_snapshots.ndjson", stall_snapshots)
    write_ndjson(output / "gateway_status.ndjson", gateway_status)
    summary = {
        "format": "COMP971 gateway USB framed binary protocol v1",
        "source": str(args.capture), "raw_bytes": args.capture.stat().st_size,
        "imu_records": len(imu), "uwb_records": len(uwb),
        "node_status_records": len(node_status), "gateway_status_records": len(gateway_status),
        "uwb_diagnostic_records": len(uwb_diagnostics),
        "uwb_diagnostic_events_by_stage": {
            name: sum(event["stage"] == name for event in uwb_diagnostics)
            for name in sorted({event["stage"] for event in uwb_diagnostics})
        },
        "invalid_frames": invalid_frames, "bytes_not_in_valid_frames": skipped_bytes,
        "imu_records_by_node": {str(node): sum(s["node_index"] == node for s in imu) for node in range(6)},
        "sequence_gaps_by_node": {str(node): gaps[node] for node in range(6)},
    }
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
