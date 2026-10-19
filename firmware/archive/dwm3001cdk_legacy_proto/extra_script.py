Import("env")
import json
import os
import sys
import glob
import shutil
import subprocess
import tempfile

# ── Qorvo DW3xxx driver sources ────────────────────────────────────────────

SDK_DRIVER = os.path.join(
    env.subst("$PROJECT_DIR"),
    "lib", "dwt_uwb_driver"
)

HARDWARE_ROOT = os.path.normpath(
    os.path.join(env.subst("$PROJECT_DIR"), "..", "..")
)
CONFIG_ROOT = os.path.join(HARDWARE_ROOT, "config")


def load_json(path):
    with open(path, encoding="utf-8") as config_file:
        return json.load(config_file)


def find_profile(profile_id):
    profiles_dir = os.path.join(CONFIG_ROOT, "profiles")
    for filename in os.listdir(profiles_dir):
        if not filename.endswith(".json"):
            continue
        profile = load_json(os.path.join(profiles_dir, filename))
        if profile.get("profile_id") == profile_id:
            return profile
    raise ValueError("Unknown DWM_PROFILE_ID: " + profile_id)


def configure_build_from_json():
    """Inject the selected device's build settings from configuration truth."""
    profile_id = os.environ.get("DWM_PROFILE_ID")
    device_id = os.environ.get("DWM_DEVICE_ID")
    if not profile_id or not device_id:
        print(
            "ERROR: set DWM_PROFILE_ID and DWM_DEVICE_ID; no firmware target "
            "or hardware identity is hardcoded in platformio.ini."
        )
        env.Exit(1)

    profile = find_profile(profile_id)
    if not profile.get("enabled", False):
        raise ValueError(
            "Selected profile is disabled and cannot be built or flashed: "
            + profile_id
        )

    registry = load_json(os.path.join(CONFIG_ROOT, "device_registry.json"))
    devices = {item["device_id"]: item for item in registry["devices"]}
    device = devices.get(device_id)
    if not device or device.get("status") != "registered":
        raise ValueError("Selected device is not a registered hardware target: " + device_id)

    role_assignments = profile.get("role_assignments", {})
    roles = [role for role, assigned_device in role_assignments.items() if assigned_device == device_id]
    if len(roles) != 1:
        raise ValueError(
            "Selected device must have exactly one role in the selected profile: "
            + device_id
        )
    role = roles[0]
    uwb = profile["uwb"]
    antenna_delay = uwb["antenna_delay"][device_id]

    defines = [
        ("UWB_NODE_NAME", '\"' + device["permanent_name"] + '\"'),
        ("UWB_TX_ANT_DLY", str(antenna_delay["tx"])),
        ("UWB_RX_ANT_DLY", str(antenna_delay["rx"])),
        ("UWB_RESP_DELAY_UUS", str(uwb["response_delay_uus"])),
    ]
    if role == "initiator":
        defines.append("UWB_INITIATOR")
    elif role != "responder":
        raise ValueError("Unsupported current firmware role: " + role)

    env.Append(CPPDEFINES=defines)
    env["DWM_JLINK_SERIAL"] = device["jlink_serial"]
    env["DWM_PERMANENT_NAME"] = device["permanent_name"]
    print(
        "[DIY-UWB] JSON configuration: profile={} device={} role={} jlink={}".format(
            profile_id, device["permanent_name"], role, device["jlink_serial"]
        )
    )


configure_build_from_json()

env.Append(CPPPATH=[
    SDK_DRIVER,
    os.path.join(SDK_DRIVER, "dw3000"),
    os.path.join(SDK_DRIVER, "lib", "qmath", "include"),
])

env.BuildSources(
    os.path.join("$BUILD_DIR", "dwt_uwb_driver"),
    SDK_DRIVER,
    src_filter=[
        "-<*>",
        "+<deca_interface.c>",
        "+<deca_compat.c>",
        "+<deca_rsl.c>",
        "+<dw3000/dw3000_device.c>",
        "+<lib/qmath/src/qmath.c>",
    ],
)

# ── J-Link: auto-patch + serial-number-targeted upload ─────────────────────
#
# PlatformIO bundles tool-jlink but the DLL may be broken.
# We auto-detect the system-installed SEGGER J-Link and patch if newer.
#
# For multi-board setups, the selected JSON device determines the J-Link serial.
#
# Users must install SEGGER J-Link Software Pack (free download):
#   https://www.segger.com/downloads/jlink/

def find_system_jlink_dir():
    """Find the newest system-installed SEGGER J-Link directory."""
    if sys.platform != "win32":
        return None
    segger_base = os.path.join(
        os.environ.get("ProgramFiles", r"C:\Program Files"), "SEGGER"
    )
    if not os.path.isdir(segger_base):
        return None
    dirs = sorted(glob.glob(os.path.join(segger_base, "JLink_V*")), reverse=True)
    return dirs[0] if dirs else None


def patch_jlink_if_needed():
    """Copy newer system J-Link DLLs into PlatformIO's tool-jlink package."""
    if sys.platform != "win32":
        return

    try:
        pio_jlink_dir = env.PioPlatform().get_package_dir("tool-jlink") or ""
    except Exception:
        return
    if not os.path.isdir(pio_jlink_dir):
        return

    sys_jlink = find_system_jlink_dir()
    if not sys_jlink:
        return

    sys_dll = os.path.join(sys_jlink, "JLinkARM.dll")
    pio_dll = os.path.join(pio_jlink_dir, "JLinkARM.dll")

    if not os.path.isfile(sys_dll):
        return
    if os.path.isfile(pio_dll) and os.path.getmtime(sys_dll) <= os.path.getmtime(pio_dll):
        return

    print("[DIY-UWB] Patching PlatformIO J-Link with: " + sys_jlink)
    for f in os.listdir(sys_jlink):
        if f.endswith(".dll") or f == "JLink.exe":
            try:
                shutil.copy2(os.path.join(sys_jlink, f), os.path.join(pio_jlink_dir, f))
            except (PermissionError, OSError):
                pass


def upload_with_serial(source, target, env):
    """
    Upload firmware using J-Link Commander with serial number targeting.

    The configuration-derived serial is injected by configure_build_from_json().
    """
    # Find J-Link Commander
    sys_jlink = find_system_jlink_dir()
    if sys_jlink:
        jlink_exe = os.path.join(sys_jlink, "JLink.exe")
    else:
        try:
            pio_jlink_dir = env.PioPlatform().get_package_dir("tool-jlink") or ""
            jlink_exe = os.path.join(pio_jlink_dir, "JLink.exe")
        except Exception:
            jlink_exe = "JLink.exe"

    if not os.path.isfile(jlink_exe):
        print("\n" + "=" * 60)
        print("ERROR: SEGGER J-Link Software not found!")
        print("")
        print("Install from: https://www.segger.com/downloads/jlink/")
        print("Download: 'J-Link Software and Documentation Pack'")
        print("=" * 60 + "\n")
        env.Exit(1)

    hex_path = os.path.abspath(str(source[0]))
    serial = env.get("DWM_JLINK_SERIAL", "")

    # Build J-Link command script
    script = tempfile.NamedTemporaryFile(mode="w", suffix=".jlink", delete=False)
    if serial:
        script.write(f"USB {serial}\n")
    script.write("device NRF52833_XXAA\n")
    script.write("si SWD\n")
    script.write("speed 4000\n")
    script.write("connect\n")
    script.write(f"loadfile {hex_path}\n")
    script.write("r\n")
    script.write("g\n")
    script.write("q\n")
    script.close()

    board_name = env.subst("$PIOENV")
    if serial:
        print(f"[DIY-UWB] Uploading {board_name} to J-Link S/N {serial}")
    else:
        print(f"[DIY-UWB] Uploading {board_name} (first available J-Link)")

    result = subprocess.run(
        [jlink_exe, "-CommandFile", script.name, "-AutoConnect", "1"],
        timeout=30,
    )
    os.unlink(script.name)

    if result.returncode != 0:
        env.Exit(1)


# Auto-patch on every build
try:
    patch_jlink_if_needed()
except Exception:
    pass

# Override the upload action with our serial-number-aware version
env.Replace(UPLOADCMD=upload_with_serial)
