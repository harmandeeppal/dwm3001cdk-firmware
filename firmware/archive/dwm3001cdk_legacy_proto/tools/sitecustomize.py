"""Keep the nRF Python runtime isolated when PlatformIO starts subprocesses."""

import os
import sys

sys.path[:] = [
    path for path in sys.path
    if not os.path.normcase(path).startswith(os.path.normcase(r"C:\tmp\py312"))
]

import ctypes  # noqa: E402,F401
