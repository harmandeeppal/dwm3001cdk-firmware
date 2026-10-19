import argparse
import json
import os
from pathlib import Path

def main():
    parser = argparse.ArgumentParser(description="Summarize node health & diagnostics from JSON capture")
    default_capture = Path(__file__).resolve().parents[1] / "outputs" / "records" / "captures" / "2026-10-07_three_node_star_bringup" / "six_node_star_test_2026-10-09_132300.json"
    parser.add_argument("--capture", default=str(default_capture), help="Path to capture JSON file")
    args = parser.parse_args()

    if not os.path.exists(args.capture):
        print(f"Warning: Capture file not found: {args.capture}")
        return

    with open(args.capture, 'r', encoding='utf-8') as f:
        data = json.load(f)

    diags = data.get('diagnostics', [])
    node_names = {0:'HEAD', 1:'PELVIS', 2:'LEFT-WRIST', 3:'RIGHT-WRIST', 4:'LEFT-ANKLE', 5:'RIGHT-ANKLE'}

    print("=== ALL 6 NODES HEALTH & DIAGNOSTICS (FROM CAPTURE) ===")
    for n in range(6):
        nd = [d for d in diags if d.get('source_node') == n]
        late = [d['detail'] for d in nd if d.get('stage_name') == 'dw3000_late_tx_count']
        min_lead = [d['detail'] for d in nd if d.get('stage_name') == 'fira_lifetime_min_lead']
        resync = [d['detail'] for d in nd if d.get('stage_name') == 'fira_stall_recovery_count' and d.get('sub_code') == 0]
        boot = [d for d in nd if d.get('stage_name') == 'fira_stall_recovery_count' and d.get('sub_code', 0) > 0]

        late_val = str(late[-1]) if late else 'N/A'
        lead_val = f"{min_lead[-1]} DTU ({min_lead[-1]/15600.0:.3f} ms)" if min_lead else 'N/A'
        resync_val = str(resync[-1]) if resync else 'N/A'
        boot_cnt = str(boot[-1]['sub_code']) if boot else 'N/A'
        reset_code = f"0x{(boot[-1]['detail'] >> 24) & 0xFF:02X}" if boot else 'N/A'

        print(f"Node {n} ({node_names[n]:<11}): Boot: {boot_cnt:>2} | Reset: {reset_code:<4} | Late TX: {late_val:>2} | Min Lead: {lead_val:<25} | Resyncs: {resync_val:>3}")

if __name__ == "__main__":
    main()

