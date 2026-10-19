"""Version-1 decoder for the wearable BLE telemetry contract.

This module has no BLE dependency. The future nRF52840 USB bridge and laptop
collector can both use it to decode the bytes defined in
firmware/dwm3001cdk_motion_node/src/ble_telemetry_protocol.h.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import Final

PROTOCOL_VERSION: Final = 1
IMU_BATCH: Final = 0x01
CLOCK_STATUS: Final = 0x02
UWB_RANGE: Final = 0x03
IMU_SAMPLE: Final = struct.Struct("<IIhhhhhhh")
IMU_BATCH_HEADER: Final = struct.Struct("<BBBBI")
CLOCK_STATUS_PACKET: Final = struct.Struct("<BBBBIIiiI")
UWB_RANGE_PACKET: Final = struct.Struct("<BBBBIiHH")


@dataclass(frozen=True)
class ImuSample:
    sequence: int
    local_timestamp_us: int
    ax: int
    ay: int
    az: int
    gx: int
    gy: int
    gz: int
    temperature: int


@dataclass(frozen=True)
class ImuBatch:
    node_index: int
    sync_epoch: int
    samples: tuple[ImuSample, ...]


def decode_imu_batch(data: bytes) -> ImuBatch:
    """Decode a single variable-length IMU batch notification."""
    if len(data) < IMU_BATCH_HEADER.size:
        raise ValueError("BLE IMU batch is shorter than its header")
    version, message_type, node_index, count, sync_epoch = IMU_BATCH_HEADER.unpack_from(data)
    if version != PROTOCOL_VERSION or message_type != IMU_BATCH:
        raise ValueError("not a version-1 BLE IMU batch")
    if not 1 <= count <= 3:
        raise ValueError("BLE IMU batch must contain one to three samples")
    expected = IMU_BATCH_HEADER.size + count * IMU_SAMPLE.size
    if len(data) != expected:
        raise ValueError(f"BLE IMU batch length {len(data)} does not match {count} samples")
    samples = tuple(
        ImuSample(*IMU_SAMPLE.unpack_from(data, IMU_BATCH_HEADER.size + i * IMU_SAMPLE.size))
        for i in range(count)
    )
    return ImuBatch(node_index=node_index, sync_epoch=sync_epoch, samples=samples)
