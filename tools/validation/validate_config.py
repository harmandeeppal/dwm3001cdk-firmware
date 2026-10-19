"""Validate the six-position DWM3001CDK hardware configuration."""

from __future__ import annotations

import json
from pathlib import Path


# This source lives in hardware/scripts/validation/.
HARDWARE_ROOT = Path(__file__).resolve().parents[2]
REPOSITORY_ROOT = HARDWARE_ROOT.parent


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def require_unique(values: list[object], description: str) -> None:
    if len(values) != len(set(values)):
        raise ValueError(f"Duplicate {description}: {values}")


def main() -> None:
    layout = load_json(HARDWARE_ROOT / "config" / "body_positions.json")
    registry = load_json(HARDWARE_ROOT / "config" / "device_registry.json")
    profile_paths = sorted((HARDWARE_ROOT / "config" / "profiles").glob("*.json"))

    positions = layout["positions"]
    devices = registry["devices"]
    if len(positions) != 6 or len(devices) != 6:
        raise ValueError("Exactly six body positions and six device slots are required.")

    require_unique([item["number"] for item in positions], "position numbers")
    require_unique([item["key"] for item in positions], "position keys")
    require_unique([item["device_id"] for item in positions], "position device IDs")
    require_unique([item["permanent_name"] for item in positions], "position names")
    require_unique([item["device_id"] for item in devices], "registry device IDs")

    position_by_device = {item["device_id"]: item for item in positions}
    device_by_id = {item["device_id"]: item for item in devices}
    if set(position_by_device) != set(device_by_id):
        raise ValueError("The layout and registry must contain the same device IDs.")

    registered_serials: list[str] = []
    for device_id, device in device_by_id.items():
        position = position_by_device[device_id]
        for field, expected in (
            ("device_number", position["number"]),
            ("body_position", position["key"]),
            ("permanent_name", position["permanent_name"]),
        ):
            if device[field] != expected:
                raise ValueError(
                    f"{device_id} has {field}={device[field]!r}; expected {expected!r}."
                )
        serial = device["jlink_serial"]
        if device["status"] == "registered":
            if not serial:
                raise ValueError(f"{device_id} is registered but has no J-Link serial.")
            registered_serials.append(serial)

    require_unique(registered_serials, "registered J-Link serials")

    topology_path = REPOSITORY_ROOT / "workspace" / "configs" / "topology" / "smplh_wearable_topology_v0.json"
    if topology_path.exists():
        topology = load_json(topology_path)
        configured_order = [item["key"] for item in positions]
        if topology["sensor_order"] != configured_order:
            raise ValueError(
                "Hardware body-position order differs from the simulation sensor order."
            )

    profiles = [load_json(path) for path in profile_paths]
    require_unique([profile["profile_id"] for profile in profiles], "profile IDs")
    for profile in profiles:
        if not isinstance(profile.get("enabled"), bool):
            raise ValueError(f"{profile['profile_id']} must declare a boolean enabled field.")

        role_assignments = profile.get("role_assignments", {})
        if len(role_assignments.values()) != len(set(role_assignments.values())):
            raise ValueError(
                f"{profile['profile_id']} assigns one device to multiple roles."
            )
        for role, device_id in role_assignments.items():
            if device_id not in device_by_id:
                raise ValueError(f"Unknown {role} device in {profile['profile_id']}: {device_id}")
            if profile["enabled"] and device_by_id[device_id]["status"] != "registered":
                raise ValueError(
                    f"The {role} device is not registered in {profile['profile_id']}: {device_id}"
                )

        declared_devices = profile.get("device_ids")
        if declared_devices is not None:
            require_unique(declared_devices, f"device IDs in {profile['profile_id']}")
            unknown_devices = set(declared_devices) - set(device_by_id)
            if unknown_devices:
                raise ValueError(
                    f"{profile['profile_id']} has unknown device IDs: {sorted(unknown_devices)}"
                )

        if profile.get("firmware_image") == "dwm3001cdk_dw3110_probe":
            target_policy = profile.get("target_policy", {})
            probe = profile.get("probe", {})
            telemetry_profile_id = profile.get("telemetry_contract_profile_id")
            if target_policy.get("selection") != "explicit_cli_device_id":
                raise ValueError(
                    f"{profile['profile_id']} must require explicit device selection."
                )
            if target_policy.get("discover_by") != "jlink_serial":
                raise ValueError(
                    f"{profile['profile_id']} must target J-Link serials, not COM ports."
                )
            if target_policy.get("com_port_is_identity") is not False:
                raise ValueError(
                    f"{profile['profile_id']} must explicitly reject COM port identity."
                )
            if probe.get("radio_tx_enabled") is not False:
                raise ValueError(
                    f"{profile['profile_id']} is an identity probe and must not transmit UWB."
                )
            if not isinstance(probe.get("expected_dw3110_device_id"), str):
                raise ValueError(
                    f"{profile['profile_id']} needs an expected DW3110 device ID."
                )
            if telemetry_profile_id not in {item["profile_id"] for item in profiles}:
                raise ValueError(
                    f"{profile['profile_id']} references an unknown telemetry profile."
                )

        if profile.get("firmware_image") == "dwm3001cdk_icm20948_stream":
            target_policy = profile.get("target_policy", {})
            imu = profile.get("external_imu", {})
            programming = profile.get("programming", {})
            if target_policy.get("selection") != "explicit_cli_device_id":
                raise ValueError(
                    f"{profile['profile_id']} must require explicit device selection."
                )
            if target_policy.get("discover_by") != "jlink_serial":
                raise ValueError(
                    f"{profile['profile_id']} must target J-Link serials, not COM ports."
                )
            if target_policy.get("com_port_is_identity") is not False:
                raise ValueError(
                    f"{profile['profile_id']} must explicitly reject COM port identity."
                )
            if not isinstance(imu.get("i2c_address"), str) or not isinstance(
                imu.get("expected_who_am_i"), str
            ):
                raise ValueError(
                    f"{profile['profile_id']} needs ICM I2C address and WHO_AM_I values."
                )
            if programming.get("backend") != "nrfutil_device":
                raise ValueError(
                    f"{profile['profile_id']} must use the nrfutil_device programmer."
                )
            if programming.get("interface") != "SWD":
                raise ValueError(f"{profile['profile_id']} must declare SWD programming.")
            if programming.get("device_family") != "nrf52":
                raise ValueError(f"{profile['profile_id']} must declare the nrf52 family.")
            if not isinstance(programming.get("swd_clock_khz"), int) or programming[
                "swd_clock_khz"
            ] <= 0:
                raise ValueError(
                    f"{profile['profile_id']} needs a positive SWD speed in kHz."
                )
            expected_options = {
                "chip_erase_mode": "ERASE_RANGES_TOUCHED_BY_FIRMWARE",
                "verify": "VERIFY_READ",
                "reset": "RESET_PIN",
            }
            if programming.get("flash_options") != expected_options:
                raise ValueError(
                    f"{profile['profile_id']} must declare the approved safe flash policy."
                )

    print("Hardware configuration: VALID")
    for device in devices:
        serial = device["jlink_serial"] or "not assigned"
        print(f"{device['permanent_name']}: J-Link {serial} [{device['status']}]")
    for profile in profiles:
        state = "enabled" if profile["enabled"] else "disabled"
        print(f"Profile {profile['profile_id']}: {state}")


if __name__ == "__main__":
    main()
