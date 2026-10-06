#!/usr/bin/env python3
"""Fast side-by-side diff of annotated functions, for iterating on a match.

Much quicker than reccmp (no PDB parsing), but cruder: absolute addresses and
call targets are shown as ADDR, branch targets as offsets from the function
start. Use scripts/compare.py for the authoritative result.

Usage: quickdiff.py [0x<orig addr> ...]   (default: every annotated function)
"""

import difflib
import re
import sys
from pathlib import Path

import capstone
import pefile

ROOT = Path(__file__).resolve().parent.parent
MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
MD.detail = True


def disasm(pe, va):
    base = pe.OPTIONAL_HEADER.ImageBase
    hi = base + pe.OPTIONAL_HEADER.SizeOfImage
    data = pe.get_memory_mapped_image()[va - base:va - base + 0x4000]
    out = []
    max_target = va
    for insn in MD.disasm(data, va):
        text = f"{insn.mnemonic} {insn.op_str}".strip()
        if insn.group(capstone.CS_GRP_JUMP) and insn.operands and insn.operands[0].type == capstone.x86.X86_OP_IMM:
            target = insn.operands[0].imm
            max_target = max(max_target, target)
            text = f"{insn.mnemonic} +{target - va:#x}"
        elif insn.group(capstone.CS_GRP_CALL) and insn.operands and insn.operands[0].type == capstone.x86.X86_OP_IMM:
            text = f"{insn.mnemonic} ADDR"
        else:
            text = re.sub(r"0x[0-9a-f]{6,8}", lambda m: "ADDR" if base <= int(m.group(0), 16) < hi else m.group(0), text)
        out.append(text)
        if insn.mnemonic == "ret" and insn.address >= max_target:
            break
    return out


def main():
    orig = pefile.PE(str(ROOT / "orig/th16.exe"))
    ours = pefile.PE(str(ROOT / "build/th16.exe"))
    funcs = [l.split() for l in (ROOT / "build/functions.txt").read_text().splitlines()]
    wanted = {int(a, 16) for a in sys.argv[1:]}
    for o, r, sym in funcs:
        o, r = int(o, 16), int(r, 16)
        if wanted and o not in wanted:
            continue
        a, b = disasm(orig, o), disasm(ours, r)
        ratio = difflib.SequenceMatcher(None, a, b, autojunk=False).ratio()
        status = "MATCH" if a == b else f"{ratio * 100:.1f}% similar ({len(a)} vs {len(b)} instructions)"
        print(f"{o:#x} {sym}: {status}")
        if a != b and wanted:
            sm = difflib.SequenceMatcher(None, a, b, autojunk=False)
            for tag, i1, i2, j1, j2 in sm.get_opcodes():
                if tag == "equal":
                    for x in a[i1:i2]:
                        print(f"     {x}")
                    continue
                for k in range(max(i2 - i1, j2 - j1)):
                    x = a[i1 + k] if i1 + k < i2 else ""
                    y = b[j1 + k] if j1 + k < j2 else ""
                    print(f"  != {x:42s} {y}")

if __name__ == "__main__":
    main()
