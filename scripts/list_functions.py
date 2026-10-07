#!/usr/bin/env python3
"""List the functions of the original th16.exe in an address range.

Function starts are direct call targets plus code that follows int3
padding (so functions only reached through pointers are included, but a
few jump targets may show up too). Names come from ExpHP's th-re-data when
TH_RE_DATA points at a checkout. Functions already decompiled in src/ are
marked "done"; ones with only a placeholder are marked "stub"; CRT code
that sigscan located (build/lib.csv) is marked "lib".

Usage: list_functions.py 0x409490 0x411860
"""

import json
import os
import re
import sys
from pathlib import Path

import capstone
import pefile

ROOT = Path(__file__).resolve().parent.parent


def main():
    lo, hi = int(sys.argv[1], 16), int(sys.argv[2], 16)
    pe = pefile.PE(str(ROOT / "orig/th16.exe"))
    base = pe.OPTIONAL_HEADER.ImageBase
    text = next(s for s in pe.sections if s.Name.rstrip(b"\0") == b".text")
    code = text.get_data()[:text.Misc_VirtualSize]
    start = base + text.VirtualAddress
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    # Jump tables live in .text; keep going past bytes that do not decode.
    md.skipdata = True

    starts = set()
    after_pad = True
    for insn in md.disasm(code, start):
        if insn.mnemonic == "int3":
            after_pad = True
            continue
        if after_pad:
            starts.add(insn.address)
        after_pad = False
        if insn.mnemonic == "call" and insn.op_str.startswith("0x"):
            starts.add(int(insn.op_str, 16))

    names = {}
    th_re = os.environ.get("TH_RE_DATA")
    if th_re:
        for row in json.loads((Path(th_re) / "data/th16.v1.00a/funcs.json").read_text()):
            names[int(row["addr"], 16)] = row["name"]
    status = {}
    for src in (ROOT / "src").rglob("*.cpp"):
        for m in re.finditer(r"//\s*(FUNCTION|SYNTHETIC|LIBRARY|STUB):\s*TH16\s+(0x[0-9a-fA-F]+)", src.read_text(errors="replace")):
            addr = int(m.group(2), 16)
            if m.group(1) == "STUB":
                status.setdefault(addr, "stub")
            else:
                status[addr] = "done"

    # CRT code that sigscan located (build/lib.csv) needs no source.
    lib = ROOT / "build/lib.csv"
    if lib.exists():
        for line in lib.read_text().splitlines():
            parts = line.split("|")
            if len(parts) == 3 and parts[2] == "library" and parts[0].startswith("0x"):
                status.setdefault(int(parts[0], 16), "lib ")
    ordered = sorted(a for a in starts if start <= a < start + len(code))
    for k, a in enumerate(ordered):
        if not lo <= a < hi:
            continue
        size = (ordered[k + 1] if k + 1 < len(ordered) else start + len(code)) - a
        mark = status.get(a, "    ")
        print(f"{a:#x} {size:6d} {mark} {names.get(a, '')}")


if __name__ == "__main__":
    main()
