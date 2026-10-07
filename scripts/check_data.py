#!/usr/bin/env python3
"""Compare the contents of every annotated global with the original.

For each `// GLOBAL: TH16 0x...` annotation in src/, the global's bytes in
build/th16.exe (address from build/th16.map, size from the PDB's type
information) are compared with orig/th16.exe at the annotated address.
Globals that only get their value at run time are zero in both files, so
they compare equal; a table whose definition lacks an initializer while the
original has contents shows up as a difference.

A 4-byte value that differs still counts as equal when both are pointers
to corresponding things:
- functions: build/functions.txt pairs each decompiled function with its
  original address; library functions come from build/lib.csv;
- data: annotated globals and vtables (`// GLOBAL:`, `// VTABLE:`) and the
  library data and string literals in build/lib.csv, at the same offset;
- other vtables: by the class name in the original's RTTI;
- other string literals: our value points at a `??_C@` literal and both
  point at the same NUL-terminated bytes;
- other data our map names (a library's own tables, such as dinput8's
  object format arrays): both point at contents that compare equal by these
  same rules (noted in the output).
Any other differing pair of in-image addresses is reported as an unmapped
pointer.

It also reports any global whose size in our PDB, placed at its original
address, runs past the next annotated address: such a pair is really one
object (a field annotated as a separate variable), which the byte comparison
cannot see.

Usage:
  scripts/check_data.py            # report the globals that differ
  scripts/check_data.py -v         # also list every global that matches
  scripts/check_data.py g_foo ...  # only these globals, with all details
"""

import bisect
import csv
import os
import re
import sys
from pathlib import Path

import pefile

sys.path.insert(0, str(Path(__file__).resolve().parent))
import toolchain as tc  # noqa: E402

ROOT = tc.ROOT
ANNOTATION = re.compile(r"//\s*(GLOBAL|VTABLE):\s*TH16\s+(0x[0-9a-fA-F]+)")
MAP_LINE = re.compile(r"\s*([0-9a-f]{4}):([0-9a-f]{8})\s+(\S+)\s+([0-9a-f]{8})\s+(?:f\s+)?(?:i\s+)?(\S+)\s*$")


class Image:
    def __init__(self, path):
        self.pe = pefile.PE(str(path), fast_load=True)
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.end = self.base + self.pe.OPTIONAL_HEADER.SizeOfImage
        self.data = self.pe.get_memory_mapped_image()

    def read(self, va, size):
        # The mapped image stops at the last section's raw data; the rest of
        # an uninitialized section (.bss) reads as zeros.
        return bytes(self.data[va - self.base:va - self.base + size]).ljust(size, b"\0")

    def dword(self, va):
        return int.from_bytes(self.read(va, 4), "little")

    def cstring(self, va):
        return self.read(va, 0x400).split(b"\0")[0]

    def contains(self, va):
        return self.base <= va < self.end

    def section_va(self, section, offset):
        return self.base + self.pe.sections[section - 1].VirtualAddress + offset


def read_map():
    """Our symbols: decorated name -> [(address, object file)]."""
    syms = {}
    for line in (ROOT / "build/th16.map").read_text(errors="replace").splitlines():
        m = MAP_LINE.match(line)
        if m and m.group(1) != "0000":
            syms.setdefault(m.group(3), []).append((int(m.group(4), 16), m.group(5)))
    return syms


def read_annotations():
    """(kind, original address, name, source file) for every GLOBAL and
    VTABLE annotation. The name is the variable (GLOBAL) or class (VTABLE)
    declared on the next line that is not a comment."""
    found = []
    for src in sorted((ROOT / "src").rglob("*")):
        if src.suffix not in (".cpp", ".h"):
            continue
        lines = src.read_text(errors="replace").splitlines()
        for i, line in enumerate(lines):
            m = ANNOTATION.search(line)
            if not m:
                continue
            j = i + 1
            while j < len(lines) and (lines[j].strip().startswith("//") or not lines[j].strip()):
                j += 1
            decl = lines[j] if j < len(lines) else ""
            if m.group(1) == "VTABLE":
                name = re.search(r"\b(?:class|struct)\s+(\w+)", decl)
            else:
                # A function pointer `T (*name)(...)`, else the identifier
                # before an array bound, initializer or `;`.
                name = (re.search(r"\(\s*(?:\w+\s+)?\*\s*(?:const\s+)?(\w+)\s*\)", decl)
                        or re.search(r"(\w+)\s*(?:\[[^\]]*\]\s*)*(?:=|;|\(|\{)", decl))
            if name:
                found.append((m.group(1), int(m.group(2), 16), name.group(1), src))
    return found


def our_symbol(syms, kind, name, src):
    """The decorated symbol and our address for an annotated name."""
    if kind == "VTABLE":
        candidates = [s for s in syms if s.startswith(f"??_7{name}@@6B")]
    else:
        candidates = [s for s in syms if s == f"_{name}" or s.startswith(f"?{name}@@3")]
    entries = [(s, a, obj) for s in candidates for a, obj in syms[s]]
    if len(entries) > 1:
        # A static defined in several objects: take the one from this file.
        own = [e for e in entries if e[2].lower() == f"{src.stem.lower()}.obj"]
        entries = own or entries
    return (entries[0][0], entries[0][1]) if entries else (None, None)


def pdb_sizes(ours):
    """Size of each global in our build, keyed by (address, name), from the
    PDB's global symbols and types (reccmp's cvdump in the project's Wine
    prefix)."""
    os.environ.update(tc.env())
    from reccmp.cvdump.runner import Cvdump
    parsed = Cvdump(str(ROOT / "build/th16.pdb")).globals().types().run()
    sizes = {}
    for g in parsed.globals:
        if not 0 < g.section <= len(ours.pe.sections):
            continue
        try:
            size = parsed.types.get(g.type).size
        except Exception:
            continue
        if size:
            key = (ours.section_va(g.section, g.offset), g.name)
            sizes[key] = max(size, sizes.get(key, 0))
    return sizes


class PointerMap:
    """Pairs original addresses with ours: functions, annotated data,
    library symbols, and vtables by their RTTI class name."""

    def __init__(self, orig, syms, annotations):
        self.orig = orig
        self.syms = syms
        self.func = {}
        for line in (ROOT / "build/functions.txt").read_text().splitlines():
            o, r, _ = line.split()
            self.func[int(o, 16)] = int(r, 16)
        self.data = []  # (orig start, orig end, our start)
        rows = (l for l in (ROOT / "build/lib.csv").read_text().splitlines() if l and not l.startswith("#"))
        for row in csv.DictReader(rows, delimiter="|"):
            if row["symbol"] in syms:
                ours = syms[row["symbol"]][0][0]
                orig = int(row["address"], 16)
                if row["type"] == "library":
                    self.func.setdefault(orig, ours)
                else:
                    self.data.append((orig, orig + 1, ours))
        for kind, orig, name, src, sym, ours, size in annotations:
            if ours is not None:
                self.data.append((orig, orig + max(size or 4, 4), ours))

    def expected(self, value):
        """Our address corresponding to an original address, or None."""
        if value in self.func:
            return self.func[value]
        for lo, hi, ours in self.data:
            if lo <= value < hi:
                return ours + (value - lo)
        return self.vtable(value)

    def vtable(self, value):
        """A vtable is preceded by its RTTI complete object locator, whose
        type descriptor names the class (`.?AVName@@`); ours is `??_7Name@@6B@`."""
        col = self.orig.dword(value - 4)
        if not self.orig.contains(col) or self.orig.dword(col) != 0:
            return None
        desc = self.orig.dword(col + 12)
        if not self.orig.contains(desc):
            return None
        name = self.orig.cstring(desc + 8)
        if not name.startswith(b".?AV") or not name.endswith(b"@@"):
            return None
        entries = self.syms.get(f"??_7{name[4:].decode()}6B@")
        return entries[0][0] if entries else None


class Comparer:
    """Compares a global's bytes, pointers by what they point at."""

    def __init__(self, orig, ours, pointers, by_addr):
        self.orig, self.ours, self.pointers = orig, ours, pointers
        self.string_literals = {a for a, s in by_addr if s.startswith("??_C@")}
        # Our symbols by address, sized up to the next symbol.
        self.symbol_at = {}
        addrs = sorted({a for a, _ in by_addr})
        for a, s in by_addr:
            if a not in self.symbol_at:
                k = bisect.bisect_right(addrs, a)
                self.symbol_at[a] = (s, (addrs[k] if k < len(addrs) else a + 4) - a)
        # Pointers to unnamed library data that matched by contents.
        self.by_contents = []

    def pointer(self, va, vb, depth):
        """None if the pointers correspond, else the kind of difference."""
        want = self.pointers.expected(va)
        if want is not None:
            return None if want == vb else "pointer"
        if vb in self.string_literals:
            return None if self.orig.cstring(va) == self.ours.cstring(vb) else "string"
        # Data no annotation names (a library's own tables): the pointers
        # correspond when what they point at does, compared the same way.
        if depth > 0 and vb in self.symbol_at:
            name, size = self.symbol_at[vb]
            if not self.diff(va, vb, size, depth - 1):
                self.by_contents.append((va, vb, name))
                return None
        return "unmapped"

    def diff(self, orig_addr, our_addr, size, depth=1):
        """Differences of one global: list of (offset, kind, orig value, our value)."""
        a, b = self.orig.read(orig_addr, size), self.ours.read(our_addr, size)
        if a == b:
            return []
        diffs = []
        for off in range(0, size, 4):
            n = min(4, size - off)
            if a[off:off + n] == b[off:off + n]:
                continue
            if n < 4:
                diffs.append((off, "bytes", a[off:off + n].hex(), b[off:off + n].hex()))
                continue
            va, vb = int.from_bytes(a[off:off + 4], "little"), int.from_bytes(b[off:off + 4], "little")
            if self.orig.contains(va) and self.ours.contains(vb):
                kind = self.pointer(va, vb, depth)
                if kind is not None:
                    diffs.append((off, kind, va, vb))
                continue
            diffs.append((off, "value", va, vb))
        return diffs


def describe(d, orig, ours):
    off, kind, va, vb = d
    if kind == "bytes":
        return f"+{off:#x}: orig {va} ours {vb}"
    if kind == "pointer":
        return f"+{off:#x}: orig points at {va:#x}, ours at {vb:#x} (does not correspond)"
    if kind == "string":
        return f"+{off:#x}: orig points at {orig.cstring(va)!r}, ours at {ours.cstring(vb)!r}"
    if kind == "unmapped":
        return f"+{off:#x}: unmapped pointer: orig {va:#x}, ours {vb:#x}"
    return f"+{off:#x}: orig {va:#010x} ours {vb:#010x}"


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("-")]
    verbose = "-v" in sys.argv[1:] or bool(args)
    orig = Image(ROOT / "orig/th16.exe")
    ours = Image(ROOT / "build/th16.exe")
    syms = read_map()
    sizes = pdb_sizes(ours)
    by_addr = sorted((a, s) for s, entries in syms.items() for a, _ in entries)

    annotations = []
    for kind, orig_addr, name, src in read_annotations():
        sym, addr = our_symbol(syms, kind, name, src)
        size = sizes.get((addr, name)) if addr is not None else None
        if size is None and addr is not None:
            # No type size (vtables, symbols from libraries): up to the next symbol.
            nxt = min((a for a, _ in by_addr if a > addr), default=addr + 4)
            size = nxt - addr
        annotations.append((kind, orig_addr, name, src, sym, addr, size))

    comparer = Comparer(orig, ours, PointerMap(orig, syms, annotations), by_addr)
    counts = {"match": 0, "differ": 0, "unmapped": 0, "missing": 0}
    for kind, orig_addr, name, src, sym, addr, size in sorted(annotations, key=lambda x: x[1]):
        if kind != "GLOBAL" or (args and name not in args):
            continue
        where = f"{orig_addr:#x} {name} ({src.relative_to(ROOT)})"
        if addr is None:
            counts["missing"] += 1
            print(f"{where}: not found in build/th16.map")
            continue
        diffs = comparer.diff(orig_addr, addr, size)
        if not diffs:
            counts["match"] += 1
            if verbose:
                print(f"{where}: MATCH ({size:#x} bytes)")
            continue
        status = "differ" if any(d[1] != "unmapped" for d in diffs) else "unmapped"
        counts[status] += 1
        label = "DIFFERS" if status == "differ" else "UNMAPPED POINTERS"
        print(f"{where}: {label} at {len(diffs)} of {size:#x} bytes' dwords")
        for d in diffs[:None if verbose else 8]:
            print(f"    {describe(d, orig, ours)}")
        if not verbose and len(diffs) > 8:
            print(f"    ... {len(diffs) - 8} more")
    for va, vb, name in comparer.by_contents:
        print(f"note: {vb:#x} ({name}) and the original's {va:#x} are unnamed data with equal contents")

    # A global whose type, placed at its original address, runs into the
    # next annotated address: one of the two is really part of the other
    # (a field annotated as its own variable), and a value written through
    # one name is never seen through the other. Only sizes from the PDB's
    # types count; a size up to our next symbol says nothing.
    placed = sorted((a for a in annotations if a[5] is not None), key=lambda x: x[1])
    overlaps = unsized = 0
    for (kind, orig_addr, name, src, sym, addr, size), nxt in zip(placed, placed[1:] + [None]):
        if kind != "GLOBAL" or (args and name not in args):
            continue
        if (addr, name) not in sizes:
            unsized += 1
            continue
        if nxt and orig_addr + size > nxt[1]:
            overlaps += 1
            print(f"{orig_addr:#x} {name} ({src.relative_to(ROOT)}): OVERLAPS {nxt[2]} at {nxt[1]:#x} "
                  f"({size:#x} bytes, {nxt[1] - orig_addr:#x} to the next annotation)")

    total = sum(counts.values())
    print(f"{total} globals: {counts['match']} match, {counts['differ']} differ, "
          f"{counts['unmapped']} only through unmapped pointers, {counts['missing']} not found")
    print(f"{total - unsized - counts['missing']} sized from the PDB: {overlaps} overlap the next annotation")
    sys.exit(1 if total != counts["match"] or overlaps else 0)


if __name__ == "__main__":
    main()
