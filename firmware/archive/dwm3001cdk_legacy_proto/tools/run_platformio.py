"""Run PlatformIO with the nRF SDK Python runtime."""

import os
import sys

# The managed shell may add a second Python runtime to sys.path. Mixing its
# extension modules with the nRF Python executable breaks ctypes on Windows.
sys.path[:] = [
    path for path in sys.path
    if not os.path.normcase(path).startswith(os.path.normcase(r"C:\tmp\py312"))
]

import ctypes  # noqa: E402,F401 - load before Click/PySerial

from platformio.__main__ import main


if __name__ == "__main__":
    sys.exit(main())
