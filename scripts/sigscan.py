#!/usr/bin/env python3
"""Find the functions of a build of ours inside the original th16.exe.

Every function in the built executable's map file becomes a byte pattern with
its address-dependent bytes masked out (base relocations, plus rel32 operands
of calls and jumps that leave the function). Each pattern is searched for in
the original .text section.

Used to check that the toolchain's static libraries (CRT, UCRT) are the ones
ZUN linked, and to name those library functions in the original.

Usage: sigscan.py build.exe build.map [--orig orig/th16.exe] [--csv out.csv]
"""

import argparse
import csv
import re
import sys
from pathlib import Path

import capstone
import pefile

ROOT = Path(__file__).resolve().parent.parent


def parse_map(path):
    """Return [(va, name, object)] for .text functions in a link.exe map."""
    funcs = []
    for line in Path(path).read_text(errors="replace").splitlines():
        m = re.match(r"\s*0001:[0-9a-f]+\s+(\S+)\s+([0-9a-f]{8})\s+f\s+(?:i\s+)?(\S+)", line)
        if m:
            funcs.append((int(m.group(2), 16), m.group(1), m.group(3)))
    return sorted(set(funcs))


def text_section(pe):
    for s in pe.sections:
        if s.Name.rstrip(b"\0") == b".text":
            return s
    raise SystemExit("no .text section")


def reloc_targets(pe):
    out = set()
    if hasattr(pe, "DIRECTORY_ENTRY_BASERELOC"):
        for block in pe.DIRECTORY_ENTRY_BASERELOC:
            for e in block.entries:
                if e.type == 3:  # IMAGE_REL_BASED_HIGHLOW
                    out.add(e.rva + pe.OPTIONAL_HEADER.ImageBase)
    return out


def build_pattern(code, va, relocs, md, image_lo, image_hi):
    """Regex for code at va with address-dependent bytes wildcarded.

    Absolute addresses are found from base relocations when the build has
    them, and otherwise from any 32-bit immediate or displacement that points
    into the image. rel32 operands of calls and jumps that leave the function
    are wildcarded too.
    """
    mask = bytearray(len(code))

    def wild(off, size=4):
        mask[off:off + size] = b"\1" * size

    for i in range(len(code)):
        if va + i in relocs:
            wild(i)
    for insn in md.disasm(code, va):
        off = insn.address - va
        if insn.group(capstone.CS_GRP_JUMP) or insn.group(capstone.CS_GRP_CALL):
            op = insn.operands[-1] if insn.operands else None
            if op is not None and op.type == capstone.x86.X86_OP_IMM and insn.size >= 5:
                if not (va <= op.imm < va + len(code)):
                    wild(off + insn.size - 4)
                continue
        for op in insn.operands:
            if op.type == capstone.x86.X86_OP_IMM and insn.imm_size == 4 and image_lo <= (op.imm & 0xFFFFFFFF) < image_hi:
                wild(off + insn.imm_offset)
            elif op.type == capstone.x86.X86_OP_MEM and insn.disp_size == 4 and image_lo <= (op.mem.disp & 0xFFFFFFFF) < image_hi:
                wild(off + insn.disp_offset)
    parts = []
    for b, m in zip(code, mask):
        parts.append(b"." if m else re.escape(bytes([b])))
    return re.compile(b"".join(parts), re.DOTALL), len(code) - sum(mask)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("exe")
    ap.add_argument("map")
    ap.add_argument("--orig", default=str(ROOT / "orig/th16.exe"))
    ap.add_argument("--csv", help="write unique matches as a reccmp data source (library functions)")
    ap.add_argument("--only-lib", action="store_true", help="only scan functions that come from .lib archives")
    ap.add_argument("--min-bytes", type=int, default=12, help="skip patterns with fewer fixed bytes")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    ours = pefile.PE(args.exe)
    orig = pefile.PE(args.orig)
    base = ours.OPTIONAL_HEADER.ImageBase
    ts = text_section(ours)
    text = ts.get_data()
    text_va = base + ts.VirtualAddress
    text_end = text_va + ts.Misc_VirtualSize
    ots = text_section(orig)
    otext = ots.get_data()[:ots.Misc_VirtualSize]
    otext_va = orig.OPTIONAL_HEADER.ImageBase + ots.VirtualAddress
    relocs = reloc_targets(ours)
    image_lo = base
    image_hi = base + ours.OPTIONAL_HEADER.SizeOfImage
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True

    funcs = parse_map(args.map)
    found, missing, ambiguous, skipped = [], [], [], 0
    for k, (va, name, obj) in enumerate(funcs):
        end = funcs[k + 1][0] if k + 1 < len(funcs) else text_end
        if args.only_lib and ":" not in obj:
            continue
        code = text[va - text_va:end - text_va].rstrip(b"\xcc")
        pat, fixed = build_pattern(code, va, relocs, md, image_lo, image_hi)
        if fixed < args.min_bytes:
            skipped += 1
            continue
        hits = [m.start() + otext_va for m in pat.finditer(otext)]
        if len(hits) == 1:
            found.append((name, obj, hits[0]))
        elif hits:
            ambiguous.append((name, obj, hits))
        else:
            missing.append((name, obj, len(code)))

    total = len(found) + len(ambiguous) + len(missing)
    print(f"{len(found)} unique, {len(ambiguous)} ambiguous, {len(missing)} not found "
          f"of {total} functions ({skipped} too small to test)")
    by_obj = {}
    for name, obj, size in missing:
        by_obj.setdefault(obj, []).append((name, size))
    if missing:
        print("not found, by object:")
        for obj, items in sorted(by_obj.items()):
            names = ", ".join(n for n, _ in items[:6]) + (" ..." if len(items) > 6 else "")
            print(f"  {obj}: {names}")
    if args.verbose:
        for name, obj, addr in found:
            print(f"  {addr:#x} {name} ({obj})")
    if args.csv:
        with open(args.csv, "w", newline="") as f:
            w = csv.writer(f, delimiter="|", lineterminator="\n")
            f.write("# Generated by scripts/sigscan.py: library code located in the original.\n")
            w.writerow(["address", "symbol", "type"])
            rows = {(addr, name) for name, _, addr in found}
            # The entry stub is too small to scan for, but both entry points
            # are the CRT's WinMainCRTStartup.
            rows.add((orig.OPTIONAL_HEADER.ImageBase + orig.OPTIONAL_HEADER.AddressOfEntryPoint, "_WinMainCRTStartup"))
            for addr, name in sorted(rows):
                w.writerow([f"{addr:#x}", name, "library"])


if __name__ == "__main__":
    main()
