#!/usr/bin/env python3
"""Fast side-by-side diff of annotated functions, for iterating on a match.

Much quicker than reccmp (no PDB parsing). Data addresses are compared as
symbol+offset wherever the original's address of that symbol is known (from
// GLOBAL: annotations and build/lib.csv), so a wrong field offset in a
global object shows up; other data addresses and call targets are shown as
ADDR, branch targets as offsets from the function start. Use
scripts/compare.py for the authoritative result.

Usage: quickdiff.py [0x<orig addr> ...]   (default: every annotated function)
"""

import bisect
import csv
import difflib
import re
import sys
from pathlib import Path

import capstone
import pefile

ROOT = Path(__file__).resolve().parent.parent
MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
MD.detail = True


class Symbols:
    """Data symbols of our build and, where known, of the original."""

    def __init__(self):
        entries = []
        for line in (ROOT / "build/th16.map").read_text(errors="replace").splitlines():
            m = re.match(r"\s*([0-9a-f]{4}):[0-9a-f]+\s+(\S+)\s+([0-9a-f]{8})\s", line)
            if m and m.group(1) != "0001":
                entries.append((int(m.group(3), 16), m.group(2)))
        entries.sort()
        self.ours_addr = [a for a, _ in entries]
        self.ours_name = [n for _, n in entries]
        size = {}
        for k, (a, n) in enumerate(entries):
            nxt = entries[k + 1][0] if k + 1 < len(entries) else a + 4
            size.setdefault(n, max(nxt - a, 1))
        decorated = set(self.ours_name)
        orig = {}
        lib = ROOT / "build/lib.csv"
        if lib.exists():
            rows = (l for l in lib.read_text().splitlines() if l and not l.startswith("#"))
            for row in csv.DictReader(rows, delimiter="|"):
                if row["type"] != "library" and row["symbol"] in decorated:
                    orig[row["symbol"]] = int(row["address"], 16)
        for src in (ROOT / "src").rglob("*.cpp"):
            lines = src.read_text(errors="replace").splitlines()
            for i, line in enumerate(lines):
                m = re.search(r"//\s*GLOBAL:\s*TH16\s+(0x[0-9a-fA-F]+)", line)
                if not m or i + 1 >= len(lines):
                    continue
                var = re.search(r"(\w+)\s*(?:\[[^\]]*\]\s*)*(?:=|;|\()", lines[i + 1])
                if not var:
                    continue
                name = var.group(1)
                for d in (f"_{name}",):
                    if d in decorated:
                        orig[d] = int(m.group(1), 16)
                for d in decorated:
                    if d.startswith(f"?{name}@@3"):
                        orig[d] = int(m.group(1), 16)
        self.orig_of = orig
        spans = sorted((a, a + size.get(n, 4), n) for n, a in orig.items())
        self.orig_spans = spans
        self.orig_starts = [a for a, _, _ in spans]

    def ours(self, v):
        k = bisect.bisect_right(self.ours_addr, v) - 1
        if k < 0:
            return None
        name = self.ours_name[k]
        if name not in self.orig_of:
            return None
        return f"{name}+{v - self.ours_addr[k]:#x}"

    def orig(self, v):
        k = bisect.bisect_right(self.orig_starts, v) - 1
        if k < 0:
            return None
        start, end, name = self.orig_spans[k]
        return f"{name}+{v - start:#x}" if v < end else None


def disasm(pe, va, resolve):
    base = pe.OPTIONAL_HEADER.ImageBase
    hi = base + pe.OPTIONAL_HEADER.SizeOfImage
    text = next(s for s in pe.sections if s.Name.rstrip(b"\0") == b".text")
    text_lo, text_hi = base + text.VirtualAddress, base + text.VirtualAddress + text.Misc_VirtualSize
    data = pe.get_memory_mapped_image()[va - base:va - base + 0x4000]

    def name_of(m):
        v = int(m.group(0), 16)
        if not base <= v < hi:
            return m.group(0)
        if text_lo <= v < text_hi:
            return "ADDR"
        return resolve(v) or "ADDR"

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
            text = re.sub(r"0x[0-9a-f]{6,8}", name_of, text)
        out.append(text)
        if insn.mnemonic == "ret" and insn.address >= max_target:
            break
    return out


def main():
    orig = pefile.PE(str(ROOT / "orig/th16.exe"))
    ours = pefile.PE(str(ROOT / "build/th16.exe"))
    funcs = [l.split() for l in (ROOT / "build/functions.txt").read_text().splitlines()]
    wanted = {int(a, 16) for a in sys.argv[1:]}
    syms = Symbols()
    for o, r, sym in funcs:
        o, r = int(o, 16), int(r, 16)
        if wanted and o not in wanted:
            continue
        a, b = disasm(orig, o, syms.orig), disasm(ours, r, syms.ours)
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
