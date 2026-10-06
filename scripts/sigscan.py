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
import struct
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


def parse_data_symbols(path):
    """{address: symbol} for public symbols outside .text in a link.exe map."""
    out = {}
    for line in Path(path).read_text(errors="replace").splitlines():
        m = re.match(r"\s*000([3-9]):[0-9a-f]+\s+(\S+)\s+([0-9a-f]{8})\s", line)
        if m:
            out.setdefault(int(m.group(3), 16), m.group(2))
    return out


def parse_all_symbols(path):
    """{address: symbol} for every public or static symbol in a map file."""
    out = {}
    for line in Path(path).read_text(errors="replace").splitlines():
        m = re.match(r"\s*[0-9a-f]{4}:[0-9a-f]+\s+(\S+)\s+([0-9a-f]{8})\s", line)
        if m:
            out.setdefault(int(m.group(2), 16), m.group(1))
    return out


def disasm_function(pe, va, md):
    base = pe.OPTIONAL_HEADER.ImageBase
    data = pe.get_memory_mapped_image()[va - base:va - base + 0x4000]
    max_target = va
    for insn in md.disasm(data, va):
        yield insn
        if insn.group(capstone.CS_GRP_JUMP) and insn.operands and insn.operands[0].type == capstone.x86.X86_OP_IMM:
            max_target = max(max_target, insn.operands[0].imm)
        if insn.mnemonic == "ret" and insn.address >= max_target:
            return


def constant_size(symbol):
    """Size in bytes of a compiler-generated constant, from its name."""
    m = re.fullmatch(r"__(real|xmm)@([0-9a-f]+)", symbol)
    return len(m.group(2)) // 2 if m else None


def paired_constants(orig, ours, map_path, pairs_path, md):
    """Original addresses of the constants (__real@..., __xmm@...) that our
    annotated functions use.

    Each (original, ours) pair from build/functions.txt is disassembled in
    lockstep. Where an operand names a constant in our build, the original's
    operand at the same place is accepted as the same constant only if the
    bytes stored there are identical, so a wrong constant still shows up as
    a difference."""
    syms = parse_all_symbols(map_path)
    obase, rbase = orig.OPTIONAL_HEADER.ImageBase, ours.OPTIONAL_HEADER.ImageBase
    oimg, rimg = orig.get_memory_mapped_image(), ours.get_memory_mapped_image()
    found = {}
    for line in Path(pairs_path).read_text().splitlines():
        o, r, _ = line.split()
        a = list(disasm_function(orig, int(o, 16), md))
        b = list(disasm_function(ours, int(r, 16), md))
        for x, y in zip(a, b):
            if x.mnemonic != y.mnemonic or x.size != y.size:
                break
            for xo, yo in zip(x.operands, y.operands):
                if yo.type != capstone.x86.X86_OP_MEM or xo.type != capstone.x86.X86_OP_MEM:
                    continue
                ov, rv = xo.mem.disp & 0xFFFFFFFF, yo.mem.disp & 0xFFFFFFFF
                name = syms.get(rv)
                size = constant_size(name) if name else None
                if size is None:
                    continue
                if oimg[ov - obase:ov - obase + size] == rimg[rv - rbase:rv - rbase + size]:
                    found.setdefault(name, ov)
    return found


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
    branches = []
    absolutes = []

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
                    branches.append((off + insn.size - 4, op.imm))
                continue
        for op in insn.operands:
            if op.type == capstone.x86.X86_OP_IMM and insn.imm_size == 4 and image_lo <= (op.imm & 0xFFFFFFFF) < image_hi:
                wild(off + insn.imm_offset)
                absolutes.append((off + insn.imm_offset, op.imm & 0xFFFFFFFF))
            elif op.type == capstone.x86.X86_OP_MEM and insn.disp_size == 4 and image_lo <= (op.mem.disp & 0xFFFFFFFF) < image_hi:
                wild(off + insn.disp_offset)
                absolutes.append((off + insn.disp_offset, op.mem.disp & 0xFFFFFFFF))
    parts = []
    for b, m in zip(code, mask):
        parts.append(b"." if m else re.escape(bytes([b])))
    return re.compile(b"".join(parts), re.DOTALL), len(code) - sum(mask), branches, absolutes


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("exe")
    ap.add_argument("map")
    ap.add_argument("--orig", default=str(ROOT / "orig/th16.exe"))
    ap.add_argument("--csv", help="write unique matches as a reccmp data source (library functions)")
    ap.add_argument("--only-lib", action="store_true", help="only scan functions that come from .lib archives")
    ap.add_argument("--pairs", help="build/functions.txt: also pair up the constants these functions use")
    ap.add_argument("--min-bytes", type=int, default=12, help="skip patterns with fewer fixed bytes")
    ap.add_argument("--min-small-bytes", type=int, default=6,
                    help="with --only-lib: smallest pattern accepted inside the library code range")
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
    candidates = []
    for k, (va, name, obj) in enumerate(funcs):
        end = funcs[k + 1][0] if k + 1 < len(funcs) else text_end
        if args.only_lib and ":" not in obj:
            continue
        code = text[va - text_va:end - text_va].rstrip(b"\xcc")
        pat, fixed, branches, absolutes = build_pattern(code, va, relocs, md, image_lo, image_hi)
        candidates.append((name, obj, len(code), pat, fixed, branches, absolutes))
    name_at = {va: name for va, name, _ in funcs}

    def branches_agree(hit, branches, known):
        """Do the hit's rel32 branches go where the known callees are?"""
        for off, target in branches:
            want = known.get(name_at.get(target))
            if want is None:
                continue
            pos = hit - otext_va + off
            got = hit + off + 4 + struct.unpack_from("<i", otext, pos)[0]
            if got != want:
                return False
        return True

    def scan(lo, min_fixed, max_fixed=None, known=None):
        found, missing, ambiguous = [], [], []
        for name, obj, size, pat, fixed, branches, _ in candidates:
            if fixed < min_fixed or (max_fixed is not None and fixed >= max_fixed):
                continue
            hits = [m.start() + otext_va for m in pat.finditer(otext, lo - otext_va)]
            if len(hits) > 1 and known:
                hits = [h for h in hits if branches_agree(h, branches, known)]
            if len(hits) == 1:
                found.append((name, obj, hits[0]))
            elif hits:
                ambiguous.append((name, obj, hits))
            else:
                missing.append((name, obj, size))
        return found, missing, ambiguous

    found, missing, ambiguous = scan(otext_va, args.min_bytes)
    skipped = sum(1 for c in candidates if c[4] < args.min_bytes)
    if args.only_lib and found:
        # Small library functions match too easily by chance, so only accept
        # them inside the range where the larger ones were found, and only
        # when the match is unique there.
        lib_start = min(addr for _, _, addr in found)
        small_found, _, _ = scan(lib_start, args.min_small_bytes, args.min_bytes)
        found += small_found
        skipped -= len(small_found)
        # Grow the set until nothing changes: settle ambiguous matches by
        # their call targets, and name callees through the calls of
        # functions that matched.
        branches_of = {c[0]: c[5] for c in candidates}
        obj_of = {c[0]: c[1] for c in candidates}
        while True:
            known = {name: addr for name, _, addr in found}
            more, _, _ = scan(lib_start, args.min_small_bytes, known=known)
            more = [f for f in more if f[0] not in known]
            for name, _, hit in found:
                for off, target in branches_of.get(name, ()):
                    callee = name_at.get(target)
                    if callee is None or callee in known or callee not in obj_of:
                        continue
                    pos = hit - otext_va + off
                    got = hit + off + 4 + struct.unpack_from("<i", otext, pos)[0]
                    more.append((callee, obj_of[callee], got))
                    known[callee] = got
            if not more:
                break
            found += more

    # Globals referenced by matched library code: our operand names a data
    # symbol, the original's operand at the same offset is its address there.
    data_syms = parse_data_symbols(args.map)
    absolutes_of = {c[0]: c[6] for c in candidates}
    globals_found = {}
    for name, _, hit in found:
        for off, value in absolutes_of.get(name, ()):
            sym = data_syms.get(value)
            if sym is None:
                continue
            orig_value = struct.unpack_from("<I", otext, hit - otext_va + off)[0]
            globals_found.setdefault(sym, orig_value)

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
            rows = {(addr, name, "library") for name, _, addr in found}
            rows |= {(addr, name, "global") for name, addr in globals_found.items()}
            if args.pairs:
                consts = paired_constants(orig, ours, args.map, args.pairs, md)
                rows |= {(addr, name, "float" if name.startswith("__real@") else "global")
                         for name, addr in consts.items()}
            # The entry stub is too small to scan for, but both entry points
            # are the CRT's WinMainCRTStartup.
            rows.add((orig.OPTIONAL_HEADER.ImageBase + orig.OPTIONAL_HEADER.AddressOfEntryPoint, "_WinMainCRTStartup", "library"))
            for addr, name, kind in sorted(rows):
                w.writerow([f"{addr:#x}", name, kind])


if __name__ == "__main__":
    main()
