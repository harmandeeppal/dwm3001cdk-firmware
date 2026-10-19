import subprocess

def parse_intel_hex(filepath):
    mem = {}
    base = 0
    with open(filepath, 'r') as f:
        for line in f:
            line = line.strip()
            if line.startswith(':'):
                byte_count = int(line[1:3], 16)
                addr = int(line[3:7], 16)
                record_type = int(line[7:9], 16)
                data = line[9:9 + byte_count * 2]
                if record_type == 0:
                    full_addr = base + addr
                    for i in range(byte_count):
                        mem[full_addr + i] = int(data[i*2:(i+1)*2], 16)
                elif record_type == 4:
                    base = int(data, 16) << 16
    return mem

def read_u8(mem, addr):
    return mem.get(addr, 0)

def read_u32(mem, addr):
    b = [read_u8(mem, addr + i) for i in range(4)]
    return b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24)

def read_str(mem, addr, maxlen=16):
    s = ""
    for i in range(maxlen):
        c = read_u8(mem, addr + i)
        if c == 0: break
        s += chr(c)
    return s

def scan_tcbs(mem):
    # Search for TCB patterns in RAM (0x20000000 to 0x20015000)
    # In FreeRTOS ARM Cortex-M:
    # offset 0: pxTopOfStack
    # offset 0x30 or 0x34: pcTaskName (char[16])
    # offset pxStack (start of stack buffer)
    tasks = []
    for addr in range(0x20000000, 0x20014000, 4):
        # Check string at addr + 52 (0x34) or addr + 48 (0x30)
        for name_off in [52, 48, 56]:
            name = read_str(mem, addr + name_off, 12)
            if name in ['comp971stat', 'comp971uwb', 'IDLE', 'Tmr Svc', 'uwb_mcps']:
                top_of_stack = read_u32(mem, addr)
                # find pxStack offset
                stack_base = read_u32(mem, addr + name_off + 16) # pxStack often follows pcTaskName
                tasks.append({'name': name, 'tcb': hex(addr), 'top': hex(top_of_stack)})
                break
    return tasks

import argparse
import os
from pathlib import Path

def main():
    parser = argparse.ArgumentParser(description="Scan FreeRTOS TCB patterns in RAM hex dumps")
    default_ram = Path(__file__).resolve().parents[1] / "outputs" / "records" / "diagnostics" / "ram_dumps_2026-10-05" / "ram_head.hex"
    parser.add_argument("--ram", default=str(default_ram), help="Path to RAM hex dump")
    args = parser.parse_args()

    if not os.path.exists(args.ram):
        print(f"Warning: Hex file not found: {args.ram}")
        return

    mem = parse_intel_hex(args.ram)
    tasks = scan_tcbs(mem)
    print(f"Found {len(tasks)} tasks in RAM ({args.ram}):")
    for t in tasks:
        print(t)

if __name__ == "__main__":
    main()

