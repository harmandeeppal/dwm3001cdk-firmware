"""Generate the DW3110 probe's C header from the hardware JSON authority."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path


HARDWARE_ROOT = Path(__file__).resolve().parents[1]
CONFIG_ROOT = HARDWARE_ROOT / "config"
DEVICE_ID_RE = re.compile(r"0x[0-9A-Fa-f]{1,8}\Z")


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
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    profile = find_profile(args.profile)
    if not profile.get("enabled", False):
        raise ValueError(f"Profile is disabled: {args.profile}")
    if profile.get("firmware_image") != "dwm3001cdk_dw3110_probe":
        raise ValueError(f"Profile is not for the DW3110 probe: {args.profile}")

    registry = load_json(CONFIG_ROOT / "device_registry.json")
    device_by_id = {device["device_id"]: device for device in registry["devices"]}
    device = device_by_id.get(args.device)
    if device is None:
        raise ValueError(f"Unknown device ID: {args.device}")
    required_status = profile["target_policy"]["eligible_registry_status"]
    if device["status"] != required_status:
        raise ValueError(
            f"{args.device} status is {device['status']!r}; expected {required_status!r}"
        )

    telemetry_profile = find_profile(profile["telemetry_contract_profile_id"])
    baud = telemetry_profile["telemetry"]["serial_baud"]
    probe = profile["probe"]
    expected_device_id = probe["expected_dw3110_device_id"]
    if not DEVICE_ID_RE.fullmatch(expected_device_id):
        raise ValueError("expected_dw3110_device_id must be a hexadecimal 32-bit literal")
    if probe["radio_tx_enabled"]:
        raise ValueError("The DW3110 identity probe must not enable UWB transmission")

    provenance = {
        "profile": profile,
        "device": {
            "device_id": device["device_id"],
            "permanent_name": device["permanent_name"],
            "jlink_serial": device["jlink_serial"],
        },
        "telemetry_profile": {
            "profile_id": telemetry_profile["profile_id"],
            "serial_baud": baud,
        },
    }
    config_hash = hashlib.sha256(
        json.dumps(provenance, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()
    header = "\n".join(
        (
            "/* Generated from hardware JSON; do not edit. */",
            "#pragma once",
            f'#define DWM_PROBE_NODE_NAME "{device["permanent_name"]}"',
            f'#define DWM_PROBE_DEVICE_ID "{device["device_id"]}"',
            f'#define DWM_PROBE_JLINK_SERIAL "{device["jlink_serial"]}"',
            f'#define DWM_PROBE_CONFIG_SHA256 "{config_hash}"',
            f"#define DWM_PROBE_SERIAL_BAUD {baud}U",
            f"#define DWM_PROBE_EXPECTED_DEVICE_ID {expected_device_id}UL",
            f"#define DWM_PROBE_BOOT_DELAY_MS {probe['boot_delay_ms']}U",
            f"#define DWM_PROBE_RETRY_INTERVAL_MS {probe['retry_interval_ms']}U",
            f"#define DWM_PROBE_SUCCESS_STATUS_INTERVAL_MS {probe['success_status_interval_ms']}U",
            "",
        )
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(header, encoding="utf-8")
    print(f"Generated {args.output}")
    print(f"Target: {device['permanent_name']} / J-Link {device['jlink_serial']}")
    print(f"Configuration SHA-256: {config_hash}")


if __name__ == "__main__":
    main()
