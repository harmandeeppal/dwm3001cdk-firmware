"""Generate the external ICM-20948 stream header from hardware JSON."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path


HARDWARE_ROOT = Path(__file__).resolve().parents[1]
CONFIG_ROOT = HARDWARE_ROOT / "config"
BYTE_LITERAL_RE = re.compile(r"0x[0-9A-Fa-f]{1,2}\Z")


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def find_profile(profile_id: str) -> dict:
    for path in sorted((CONFIG_ROOT / "profiles").glob("*.json")):
        profile = load_json(path)
        if profile.get("profile_id") == profile_id:
            return profile
    raise ValueError(f"Unknown profile ID: {profile_id}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", required=True)
    parser.add_argument("--device", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--nrfutil-operation-output", type=Path)
    parser.add_argument("--firmware-image", type=Path)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    profile = find_profile(args.profile)
    if not profile.get("enabled", False):
        raise ValueError(f"Profile is disabled: {args.profile}")
    if profile.get("firmware_image") != "dwm3001cdk_icm20948_stream":
        raise ValueError(f"Profile is not for the ICM-20948 stream: {args.profile}")

    registry = load_json(CONFIG_ROOT / "device_registry.json")
    devices = {item["device_id"]: item for item in registry["devices"]}
    device = devices.get(args.device)
    if device is None:
        raise ValueError(f"Unknown device ID: {args.device}")
    required_status = profile["target_policy"]["eligible_registry_status"]
    if device["status"] != required_status:
        raise ValueError(f"Selected device must have status {required_status!r}")

    telemetry_profile = find_profile(profile["telemetry_contract_profile_id"])
    imu = profile["external_imu"]
    for field in ("i2c_address", "expected_who_am_i"):
        if not BYTE_LITERAL_RE.fullmatch(imu[field]):
            raise ValueError(f"{field} must be a hexadecimal byte literal")

    programming = profile["programming"]
    if programming["backend"] != "nrfutil_device":
        raise ValueError("Only the nrfutil_device programming backend is supported")
    if programming["interface"] != "SWD":
        raise ValueError("Only SWD programming is supported for this diagnostic")
    if programming["device_family"] != "nrf52":
        raise ValueError("programming.device_family must be nrf52 for DWM3001CDK")
    if not isinstance(programming["swd_clock_khz"], int) or programming["swd_clock_khz"] <= 0:
        raise ValueError("programming.swd_clock_khz must be a positive integer")
    flash_options = programming["flash_options"]
    expected_options = {
        "chip_erase_mode": "ERASE_RANGES_TOUCHED_BY_FIRMWARE",
        "verify": "VERIFY_READ",
        "reset": "RESET_PIN",
    }
    if flash_options != expected_options:
        raise ValueError("programming.flash_options must use the approved safe policy")
    if (args.nrfutil_operation_output is None) != (args.firmware_image is None):
        raise ValueError(
            "--nrfutil-operation-output and --firmware-image must be supplied together"
        )

    provenance = {
        "profile": profile,
        "device": {
            "device_id": device["device_id"],
            "permanent_name": device["permanent_name"],
            "jlink_serial": device["jlink_serial"],
        },
        "telemetry": telemetry_profile["telemetry"],
    }
    config_hash = hashlib.sha256(
        json.dumps(provenance, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()
    header = "\n".join(
        (
            "/* Generated from hardware JSON; do not edit. */",
            "#pragma once",
            f'#define ICM_NODE_NAME "{device["permanent_name"]}"',
            f'#define ICM_DEVICE_ID "{device["device_id"]}"',
            f'#define ICM_JLINK_SERIAL "{device["jlink_serial"]}"',
            f'#define ICM_CONFIG_SHA256 "{config_hash}"',
            f"#define ICM_SERIAL_BAUD {telemetry_profile['telemetry']['serial_baud']}U",
            f"#define ICM_SAMPLE_RATE_HZ {telemetry_profile['telemetry']['imu_rate_hz']}U",
            f"#define ICM_I2C_ADDRESS {imu['i2c_address']}U",
            f"#define ICM_WHO_AM_I_VALUE {imu['expected_who_am_i']}U",
            "",
        )
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(header, encoding="utf-8")
    print(f"Generated {args.output}")
    print(f"Target: {device['permanent_name']} / J-Link {device['jlink_serial']}")
    print(f"Configuration SHA-256: {config_hash}")

    if args.nrfutil_operation_output is not None:
        image = args.firmware_image.resolve()
        if not image.is_file():
            raise ValueError(f"Firmware image does not exist: {image}")
        operation = {
            "schema_version": 1,
            "tool": "nrfutil device program",
            "profile_id": args.profile,
            "configuration_sha256": config_hash,
            "target": {
                "device_id": device["device_id"],
                "permanent_name": device["permanent_name"],
                "jlink_serial": device["jlink_serial"],
            },
            "firmware_image": str(image),
            "programming": programming,
        }
        args.nrfutil_operation_output.parent.mkdir(parents=True, exist_ok=True)
        args.nrfutil_operation_output.write_text(
            json.dumps(operation, indent=2) + "\n", encoding="utf-8"
        )
        print(f"Generated {args.nrfutil_operation_output}")


if __name__ == "__main__":
    main()
