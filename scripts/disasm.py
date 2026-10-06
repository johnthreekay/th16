#!/usr/bin/env python3
"""Disassemble a function of the original th16.exe with names filled in.

Names come from, in order of preference: our annotations (build/functions.txt
and // GLOBAL: comments in src/), library code located by sigscan
(build/lib.csv), and ExpHP's th-re-data if TH_RE_DATA points at a checkout.

Usage: disasm.py 0x402220 [0x402330 ...]
"""

import csv
import json
import os
import re
import sys
from pathlib import Path

import capstone
import pefile

ROOT = Path(__file__).resolve().parent.parent
ORIG = ROOT / "orig/th16.exe"


def load_names():
    names = {}
    th_re = os.environ.get("TH_RE_DATA")
    if th_re:
        data = Path(th_re) / "data/th16.v1.00a"
        for fname in ("funcs.json", "statics.json"):
            path = data / fname
            if path.exists():
                for row in json.loads(path.read_text()):
                    names[int(row["addr"], 16)] = row["name"]
    lib = ROOT / "build/lib.csv"
    if lib.exists():
        rows = (l for l in lib.read_text().splitlines() if l and not l.startswith("#"))
        for row in csv.DictReader(rows, delimiter="|"):
            names[int(row["address"], 16)] = row["symbol"]
    for src in (ROOT / "src").rglob("*.cpp"):
        lines = src.read_text(errors="replace").splitlines()
        for i, line in enumerate(lines):
            m = re.search(r"//\s*(FUNCTION|GLOBAL|STUB):\s*TH16\s+(0x[0-9a-fA-F]+)", line)
            if not m:
                continue
            for nxt in lines[i + 1:i + 4]:
                decl = nxt.split("(", 1)[0].split("=", 1)[0].strip().rstrip(";")
                if decl and not decl.startswith("//"):
                    names[int(m.group(2), 16)] = decl.split()[-1].lstrip("*&")
                    break
    return names


def main():
    pe = pefile.PE(str(ORIG))
    base = pe.OPTIONAL_HEADER.ImageBase
    hi = base + pe.OPTIONAL_HEADER.SizeOfImage
    img = pe.get_memory_mapped_image()
    names = load_names()
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True

    def label(value):
        if value in names:
            return names[value]
        for delta in range(1, 0x40):
            if value - delta in names:
                return f"{names[value - delta]}+{delta:#x}"
        return None

    for arg in sys.argv[1:]:
        va = int(arg, 16)
        print(f"; {va:#x} {names.get(va, '')}")
        max_target = va
        for insn in md.disasm(img[va - base:va - base + 0x8000], va):
            text = f"{insn.mnemonic} {insn.op_str}"
            notes = []
            for m in re.finditer(r"0x[0-9a-f]{6,8}", insn.op_str):
                v = int(m.group(0), 16)
                if base <= v < hi:
                    lab = label(v)
                    if lab:
                        notes.append(lab)
            if insn.group(capstone.CS_GRP_JUMP) and insn.operands and insn.operands[0].type == capstone.x86.X86_OP_IMM:
                max_target = max(max_target, insn.operands[0].imm)
            print(f"  {insn.address:x}: {text:48s}{' ; ' + ', '.join(notes) if notes else ''}")
            if insn.mnemonic in ("ret", "jmp") and insn.address >= max_target:
                nxt = img[insn.address + insn.size - base]
                if insn.mnemonic == "ret" or nxt == 0xCC:
                    break
        print()


if __name__ == "__main__":
    main()
