import collections
import struct
import time

import serial


def decode_cobs(encoded):
    out = bytearray()
    i = 0
    while i < len(encoded):
        code = encoded[i]
        if code == 0:
            raise ValueError("zero in COBS payload")
        i += 1
        end = i + code - 1
        if end > len(encoded):
            raise ValueError("truncated COBS payload")
        out.extend(encoded[i:end])
        i = end
        if code != 0xFF and i < len(encoded):
            out.append(0)
    return bytes(out)


ports = {"PELVIS": "COM6", "HEAD": "COM13"}
handles = {name: serial.Serial(port, 460800, timeout=0.01)
           for name, port in ports.items()}
buffers = {name: bytearray() for name in ports}
events = collections.Counter()
boots = collections.Counter()
bad = collections.Counter()
deadline = time.monotonic() + 5.0
try:
    for handle in handles.values():
        handle.reset_input_buffer()
    while time.monotonic() < deadline:
        for name, handle in handles.items():
            data = handle.read(handle.in_waiting or 1)
            if not data:
                continue
            buffers[name].extend(data)
            while 0 in buffers[name]:
                end = buffers[name].index(0)
                encoded = bytes(buffers[name][:end])
                del buffers[name][:end + 1]
                if not encoded:
                    continue
                try:
                    frame = decode_cobs(encoded)
                    if len(frame) < 6:
                        raise ValueError("short frame")
                    frame_type = frame[0]
                    payload_len = frame[5]
                    payload = frame[6:6 + payload_len]
                    if frame_type == 0xFE and len(payload) == 6:
                        code, index = payload[:2]
                        detail = struct.unpack_from("<i", payload, 2)[0]
                        events[(name, code, index, detail)] += 1
                    elif frame_type == 0xFF:
                        boots[(name, payload.decode("ascii", "replace"))] += 1
                except (ValueError, struct.error):
                    bad[name] += 1
finally:
    for handle in handles.values():
        handle.close()

print("BOOT NAMES")
for key, count in sorted(boots.items()):
    print(count, key)
print("DEBUG EVENTS")
for key, count in sorted(events.items()):
    print(count, key)
print("BAD", dict(bad))
