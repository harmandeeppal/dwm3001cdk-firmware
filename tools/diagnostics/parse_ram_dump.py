import argparse
import os
import subprocess
import sys
from pathlib import Path

def parse_intel_hex(filepath):
    mem = {}
    base = 0
    if not os.path.exists(filepath):
        print(f"Warning: Hex file not found: {filepath}")
        return mem
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

def read_i32(mem, addr):
    val = read_u32(mem, addr)
    if val >= 0x80000000:
        val -= 0x100000000
    return val

def get_elf_symbols(elf_path):
    syms = {}
    if not os.path.exists(elf_path):
        return syms
    try:
        out = subprocess.check_output(['arm-none-eabi-nm', elf_path]).decode('utf-8')
        for line in out.splitlines():
            parts = line.strip().split()
            if len(parts) >= 3:
                addr = int(parts[0], 16)
                name = parts[2]
                syms[name] = addr
    except Exception as e:
        print(f"Warning: Could not read symbols from {elf_path}: {e}")
    return syms

def main():
    parser = argparse.ArgumentParser(description="Parse DWM3001CDK RAM dumps against symbols")
    default_head_ram = Path(__file__).resolve().parents[1] / "outputs" / "records" / "diagnostics" / "ram_dumps_2026-10-05" / "ram_head.hex"
    default_pelvis_ram = Path(__file__).resolve().parents[1] / "outputs" / "records" / "diagnostics" / "ram_dumps_2026-10-05" / "ram_pelvis.hex"

    parser.add_argument("--head-ram", default=str(default_head_ram), help="Path to head RAM hex dump")
    parser.add_argument("--pelvis-ram", default=str(default_pelvis_ram), help="Path to pelvis RAM hex dump")
    parser.add_argument("--head-elf", default=r'C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_HEAD_v5\DWM3001CDK-COMP971-FreeRTOS.elf', help="Path to head ELF")
    parser.add_argument("--pelvis-elf", default=r'C:\COMP971_QORVO_SDK_1_1_1\BuildOutput\COMP971\FreeRTOS\DWM3001CDK\Debug_PELVIS_v5\DWM3001CDK-COMP971-FreeRTOS.elf', help="Path to pelvis ELF")
    args = parser.parse_args()

    head_syms = get_elf_symbols(args.head_elf)
    pelvis_syms = get_elf_symbols(args.pelvis_elf)

    head_mem = parse_intel_hex(args.head_ram)
    pelvis_mem = parse_intel_hex(args.pelvis_ram)

    print("=== HEAD Failures ===")
    if 'g_fira_fail_ring_count' in head_syms:
        h_count = read_u8(head_mem, head_syms['g_fira_fail_ring_count'])
        print(f"HEAD Fail Count: {h_count}")
        for i in range(min(h_count, 32)):
            blk = read_u32(head_mem, head_syms['g_fira_fail_block_indices'] + i * 4)
            st = read_u8(head_mem, head_syms['g_fira_fail_statuses'] + i)
            print(f"  Head Fail {i}: Block {blk}, Status {st}")
    else:
        print("  Symbol g_fira_fail_ring_count not found in ELF.")

    print("\n=== PELVIS Failures ===")
    if 'g_fira_fail_ring_count' in pelvis_syms:
        p_count = read_u8(pelvis_mem, pelvis_syms['g_fira_fail_ring_count'])
        print(f"PELVIS Fail Count: {p_count}")
        for i in range(min(p_count, 32)):
            blk = read_u32(pelvis_mem, pelvis_syms['g_fira_fail_block_indices'] + i * 4)
            st = read_u8(pelvis_mem, pelvis_syms['g_fira_fail_statuses'] + i)
            print(f"  Pelvis Fail {i}: Block {blk}, Status {st}")
    else:
        print("  Symbol g_fira_fail_ring_count not found in ELF.")

    print("\n=== Tasks Watermarks ===")
    if 'g_stat_task_min_stack' in head_syms and 'g_uwb_task_min_stack' in head_syms:
        h_stat_wm = read_u32(head_mem, head_syms['g_stat_task_min_stack'])
        h_uwb_wm = read_u32(head_mem, head_syms['g_uwb_task_min_stack'])
        print(f"HEAD comp971stat unused stack: {h_stat_wm * 4} bytes")
        print(f"HEAD comp971uwb unused stack: {h_uwb_wm * 4} bytes")
    else:
        print("  Task min stack symbols not present in ELF.")

if __name__ == "__main__":
    main()


