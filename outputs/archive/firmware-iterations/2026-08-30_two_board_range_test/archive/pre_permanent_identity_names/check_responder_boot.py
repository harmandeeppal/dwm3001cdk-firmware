import subprocess
import time

import serial
from cobs import cobs


PORT = "COM6"
JLINK = r"C:\Program Files\SEGGER\JLink_V972\JLink.exe"
SCRIPT = r"C:\Users\harma\OneDrive - AUT University\AUT - COMP971 Dissertation (60 points)\workspace\outputs\hardware\dwm3001cdk_twr\2026-08-30_build\reset_760224787.jlink"


with serial.Serial(PORT, 115200, timeout=0.1) as port:
    port.reset_input_buffer()
    subprocess.run(
        [JLINK, "-NoGui", "1", "-SelectEmuBySN", "760224787", "-CommanderScript", SCRIPT],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.STDOUT,
    )
    deadline = time.monotonic() + 4.0
    encoded = bytearray()
    while time.monotonic() < deadline:
        byte = port.read(1)
        if not byte:
            continue
        if byte == b"\x00":
            if encoded:
                try:
                    decoded = cobs.decode(bytes(encoded))
                    print("FRAME", decoded.hex(" "))
                except Exception as exc:
                    print("BAD_FRAME", exc)
                encoded.clear()
        else:
            encoded.extend(byte)
