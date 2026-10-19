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

def count_a5(mem, start_addr, max_len):
    count = 0
    for i in range(max_len):
        if read_u8(mem, start_addr + i) == 0xa5:
            count += 1
        else:
            break
    return count

import argparse
import os
from pathlib import Path

def main():
    parser = argparse.ArgumentParser(description="Check FreeRTOS stack watermarks in RAM hex dumps")
    default_head_ram = Path(__file__).resolve().parents[1] / "outputs" / "records" / "diagnostics" / "ram_dumps_2026-10-05" / "ram_head.hex"
    default_pelvis_ram = Path(__file__).resolve().parents[1] / "outputs" / "records" / "diagnostics" / "ram_dumps_2026-10-05" / "ram_pelvis.hex"

    parser.add_argument("--head-ram", default=str(default_head_ram), help="Path to head RAM hex dump")
    parser.add_argument("--pelvis-ram", default=str(default_pelvis_ram), help="Path to pelvis RAM hex dump")
    args = parser.parse_args()

    if not os.path.exists(args.head_ram) or not os.path.exists(args.pelvis_ram):
        print(f"Warning: Hex dump files not found: {args.head_ram} or {args.pelvis_ram}")
        return

    mem = parse_intel_hex(args.head_ram)
    stat_pxStack = read_u32(mem, 0x20006d58 + 0x30)
    stat_free = count_a5(mem, stat_pxStack, 1024)

    uwb_pxStack = read_u32(mem, 0x20007dc8 + 0x30)
    uwb_free = count_a5(mem, uwb_pxStack, 4096)

    print(f"HEAD comp971stat: pxStack={hex(stat_pxStack)}, unused={stat_free}/1024 bytes ({(stat_free/1024.0)*100:.1f}% free)")
    print(f"HEAD comp971uwb:  pxStack={hex(uwb_pxStack)}, unused={uwb_free}/4096 bytes ({(uwb_free/4096.0)*100:.1f}% free)")

    mem_p = parse_intel_hex(args.pelvis_ram)
    p_stat_pxStack = read_u32(mem_p, 0x20006d58 + 0x30)
    p_stat_free = count_a5(mem_p, p_stat_pxStack, 1024)

    p_uwb_pxStack = read_u32(mem_p, 0x20007dc8 + 0x30)
    p_uwb_free = count_a5(mem_p, p_uwb_pxStack, 4096)

    print(f"PELVIS comp971stat: pxStack={hex(p_stat_pxStack)}, unused={p_stat_free}/1024 bytes ({(p_stat_free/1024.0)*100:.1f}% free)")
    print(f"PELVIS comp971uwb:  pxStack={hex(p_uwb_pxStack)}, unused={p_uwb_free}/4096 bytes ({(p_uwb_free/4096.0)*100:.1f}% free)")

if __name__ == "__main__":
    main()

