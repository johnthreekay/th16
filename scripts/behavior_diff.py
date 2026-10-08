#!/usr/bin/env python3
"""What each function does, compared with the original, ignoring how.

For every annotated function this collects facts that register allocation,
instruction scheduling and block layout cannot change, in both the original
and our build, and reports the functions where they differ:

- calls: the target (the original's function, an import, or a vtable
  offset) with the constants pushed or put in ecx/edx for it, including
  constants pushed before a jump to a call several paths share
- call order: two calls made one after the other in one build and the other
  way round in the other (the order of argument reads, for example)
- constants: integer immediates (not stack adjustments), float constants by
  value (loaded from memory or stored as an integer immediate), string
  literals by content, tables of pointers by their first entry
- globals: loads, stores and address-of, at the original's address (ours
  are moved there first, so a bound just past an array names the same
  thing in both)
- fields: loads and stores through a register other than esp/ebp, by size
  and offset
- comparisons with a constant: the constant and the kind of condition
  (lt, le, ult, ule, eq), whichever way the branch goes
- returns: the constants the function returns
- switches: jump table cases with no code (`__assume(0)`), which are
  undefined if reached

Items are compared by presence, not count: reloading a value instead of
keeping it in a register, or one call shared by two paths, changes counts
without changing behaviour (--items shows the counts). For the same
reason a load of something the function also stores is left out, and field
accesses that line up under one constant shift (ours through `this` plus an
offset, the original through a pointer to the inner struct) are paired. A
function that differs only in registers and scheduling
reports nothing (every exact and every scheduling-only match comes out
empty), so what is reported is either a behaviour difference or an
equivalent shape (inlining, x * 20 as lea and shl or as imul, a loop
pointer that starts elsewhere). Read each one.

Usage:
  behavior_diff.py                     every function with a difference
  behavior_diff.py 0x<addr> ...        those functions
  behavior_diff.py --items 0x<addr>    all items of both builds, side by side
  behavior_diff.py --save              remember the current differences
  behavior_diff.py --check             functions whose differences changed
                                       since --save (after a matching edit,
                                       a new item is a behaviour change)
"""

import bisect
import csv
import json
import re
import struct
import sys
from collections import Counter
from pathlib import Path

import capstone
import capstone.x86 as X
import pefile

ROOT = Path(__file__).resolve().parent.parent
BASELINE = ROOT / "build/behavior_baseline.json"
MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
MD.detail = True

# The /GS cookie check: whether a function has one is a matching question.
IGNORED_CALLS = {"@__security_check_cookie@4"}
IGNORED_GLOBALS = re.compile(r"_*security_cookie")
# Item kinds compared by count. None: reloading a value instead of keeping
# it in a register is register allocation, and identical code on two paths
# shared (or duplicated) by the compiler changes the number of calls and
# stores. Every behaviour difference found so far was a difference in
# presence (a store or load missing, a constant or argument changed);
# --items shows the counts.
COUNTED = ()
STACK_REGS = {X.X86_REG_ESP, X.X86_REG_EBP}
ARG_REGS = {X.X86_REG_ECX, X.X86_REG_EDX}
COND = {
    "l": "lt", "ge": "lt", "le": "le", "g": "le",
    "b": "ult", "ae": "ult", "be": "ule", "a": "ule",
    "e": "eq", "ne": "eq", "s": "sign", "ns": "sign",
}
# Instructions an epilogue runs between setting eax and ret.
EPILOGUE = re.compile(r"^(pop|leave|mov esp, ebp|mov ecx, dword ptr \[e[bs]p|xor ecx, e[bs]p|"
                      r"lea esp|add esp|mov esp|call)")


def short(sym):
    """A readable form of a decorated name, for the report."""
    m = re.match(r"\?([^@?]+)@([^@]+)@@", sym)
    if m:
        return f"{m.group(2)}::{m.group(1)}"
    m = re.match(r"\?([^@?]+)@@", sym)
    if m:
        return m.group(1)
    return sym[1:] if sym.startswith("_") else sym


class Image:
    def __init__(self, path):
        self.pe = pefile.PE(str(path))
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.mem = self.pe.get_memory_mapped_image()
        self.sections = {}
        for s in self.pe.sections:
            name = s.Name.rstrip(b"\0").decode()
            lo = self.base + s.VirtualAddress
            self.sections[name] = (lo, lo + max(s.Misc_VirtualSize, s.SizeOfRawData))
        self.imports = {}
        for entry in getattr(self.pe, "DIRECTORY_ENTRY_IMPORT", []):
            for imp in entry.imports:
                name = imp.name.decode() if imp.name else f"ord{imp.ordinal}"
                self.imports[imp.address] = name

    def section_of(self, va):
        for name, (lo, hi) in self.sections.items():
            if lo <= va < hi:
                return name
        return None

    def bytes(self, va, n):
        off = va - self.base
        if off < 0:
            return b""
        return self.mem[off:off + n]

    def u32(self, va):
        b = self.bytes(va, 4)
        return struct.unpack("<I", b)[0] if len(b) == 4 else None

    def cstring(self, va):
        raw = self.bytes(va, 300)
        end = raw.find(b"\0")
        if end <= 0:
            return None
        s = raw[:end]
        # Printable ASCII, tabs and line breaks, or Shift-JIS lead/trail bytes.
        if all(c >= 0x20 or c in (9, 10, 13) for c in s):
            return s.decode("cp932", "replace")
        return None


class Names:
    """Both builds' addresses in one name space: the original's addresses
    where known, otherwise our symbol names."""

    def __init__(self, orig, ours):
        self.orig, self.ours = orig, ours
        funcs = [l.split() for l in (ROOT / "build/functions.txt").read_text().splitlines()]
        self.functions = [(int(o, 16), int(r, 16), s) for o, r, s in funcs]
        self.sym_to_orig = {s: o for o, _, s in self.functions}
        self.orig_name = {o: short(s) for o, _, s in self.functions}
        lib = ROOT / "build/lib.csv"
        if lib.exists():
            rows = (l for l in lib.read_text().splitlines() if l and not l.startswith("#"))
            for row in csv.DictReader(rows, delimiter="|"):
                a = int(row["address"], 16)
                self.sym_to_orig.setdefault(row["symbol"], a)
                self.orig_name.setdefault(a, short(row["symbol"]))

        # Our symbols: code from the map's section 1, everything else as data.
        code, data = [], []
        for line in (ROOT / "build/th16.map").read_text(errors="replace").splitlines():
            m = re.match(r"\s*([0-9a-f]{4}):[0-9a-f]+\s+(\S+)\s+([0-9a-f]{8})\s", line)
            if not m:
                continue
            (code if m.group(1) == "0001" else data).append((int(m.group(3), 16), m.group(2)))
        code.sort()
        data.sort()
        self.ours_code = {a: n for a, n in code}
        self.ours_code_starts = [a for a, _ in code]
        self.ours_data_addr = [a for a, _ in data]
        self.ours_data_name = [n for _, n in data]

        # Globals and vtables with a known original address.
        self.data_orig = {}
        decorated = set(self.ours_data_name)
        for src in (ROOT / "src").rglob("*.[ch]*"):
            lines = src.read_text(errors="replace").splitlines()
            for i, line in enumerate(lines[:-1]):
                m = re.search(r"//\s*(GLOBAL|VTABLE):\s*TH16\s+(0x[0-9a-fA-F]+)", line)
                if not m:
                    continue
                addr = int(m.group(2), 16)
                if m.group(1) == "VTABLE":
                    c = re.search(r"(?:class|struct)\s+(\w+)", lines[i + 1])
                    if c and f"??_7{c.group(1)}@@6B@" in decorated:
                        self.data_orig[f"??_7{c.group(1)}@@6B@"] = addr
                    continue
                var = re.search(r"(\w+)\s*(?:\[[^\]]*\]\s*)*(?:=|;|\()", lines[i + 1])
                if not var:
                    continue
                name = var.group(1)
                if f"_{name}" in decorated:
                    self.data_orig[f"_{name}"] = addr
                for d in decorated:
                    if d.startswith(f"?{name}@@3"):
                        self.data_orig[d] = addr
        for sym, a in self.sym_to_orig.items():
            if sym in decorated:
                self.data_orig.setdefault(sym, a)
        spans = []
        for k, (a, n) in enumerate(data):
            if n in self.data_orig:
                nxt = data[k + 1][0] if k + 1 < len(data) else a + 4
                spans.append((self.data_orig[n], self.data_orig[n] + max(nxt - a, 1), n))
        spans.sort()
        self.orig_spans = spans
        self.orig_span_starts = [s for s, _, _ in spans]

        # Function bounds in the original: the next known function start.
        starts = {o for o, _, _ in self.functions} | set(self.orig_name)
        self.orig_starts = sorted(starts)

    def orig_bound(self, va):
        k = bisect.bisect_right(self.orig_starts, va)
        return self.orig_starts[k] if k < len(self.orig_starts) else va + 0x4000

    def ours_bound(self, va):
        k = bisect.bisect_right(self.ours_code_starts, va)
        return self.ours_code_starts[k] if k < len(self.ours_code_starts) else va + 0x4000

    def code(self, side, va):
        """A call or tail jump target."""
        img = self.orig if side == "orig" else self.ours
        if va in img.imports:
            return img.imports[va]
        if side == "orig":
            return self.orig_name.get(va, f"sub_{va:x}")
        sym = self.ours_code.get(va)
        if sym is None:
            return f"ours_{va:x}"
        o = self.sym_to_orig.get(sym)
        return self.orig_name.get(o, short(sym)) if o is not None else short(sym)

    def orig_global(self, a):
        """The annotated global of the original that holds a, as (symbol,
        offset). Both builds' addresses go through this as addresses of the
        original, so a loop bound just past an array names the same thing in
        both."""
        k = bisect.bisect_right(self.orig_span_starts, a) - 1
        if k < 0:
            return None
        start, end, sym = self.orig_spans[k]
        return (sym, a - start) if a < end else None

    def data(self, side, va, size, float_op):
        """A data address: a global as the original's symbol and offset
        (our addresses are first moved to where the original has that
        global), a constant by its value, a string by its contents."""
        img = self.orig if side == "orig" else self.ours
        if va in img.imports:
            return f"import {img.imports[va]}"
        sect = img.section_of(va)
        if sect == ".text":
            return "code"
        sym, orig_va = None, None
        if side == "orig":
            hit = self.orig_global(va)
            if hit:
                sym, orig_va = hit[0], va
        else:
            k = bisect.bisect_right(self.ours_data_addr, va) - 1
            if k >= 0:
                sym = self.ours_data_name[k]
                if sym in self.data_orig:
                    orig_va = self.data_orig[sym] + va - self.ours_data_addr[k]
        if sym and sym.startswith(("__real@", "__xmm@")):
            return self.float_constant(img, va, sym)
        if orig_va is not None and not sym.startswith("??_C@"):
            hit = self.orig_global(orig_va)
            if hit is None:
                return "unannotated global"
            return f"{short(hit[0])}+{hit[1]:#x}"
        if sect == ".rdata" or (sym and sym.startswith("??_C@")):
            if float_op and size in (4, 8, 16):
                raw = img.bytes(va, size)
                if size == 4:
                    return f"f32 {struct.unpack('<f', raw)[0]!r}"
                if size == 8:
                    return f"f64 {struct.unpack('<d', raw)[0]!r}"
                return "xmm " + " ".join(f"{v!r}" for v in struct.unpack("<4f", raw))
            target = img.u32(va)
            if not float_op and target is not None and img.section_of(target) and target >= img.base:
                # A table of pointers (a vtable, a function table): name its first entry.
                if img.section_of(target) == ".text":
                    return f"table of {self.code(side, target)}"
                return "table of data"
            s = img.cstring(va)
            if s is not None and not float_op:
                return f"str {s!r}"
            if size in (1, 2, 4, 8):
                return f"const{size * 8} {int.from_bytes(img.bytes(va, size), 'little'):#x}"
            return "rdata"
        return "unannotated global"

    @staticmethod
    def float_constant(img, va, sym):
        """A __real@ or __xmm@ constant (or a part of one) by its value."""
        digits = sym.split("@")[1]
        if sym.startswith("__xmm@"):
            return "xmm " + " ".join(f"{v!r}" for v in struct.unpack("<4f", img.bytes(va, 16)))
        if len(digits) == 8:
            return f"f32 {struct.unpack('<f', img.bytes(va, 4))[0]!r}"
        return f"f64 {struct.unpack('<d', img.bytes(va, 8))[0]!r}"

    def constant(self, side, value):
        """An immediate: an address as data(), anything else as a number."""
        img = self.orig if side == "orig" else self.ours
        v = value & 0xFFFFFFFF
        if img.section_of(v) and v >= img.base:
            return self.data(side, v, 0, False)
        return str(value)


def walk(img, start, bound):
    """The instructions reachable from start without leaving [start, bound),
    following jump tables. Returns them in address order, the set of
    addresses that start a basic block and the jump table cases that have no
    target (left out with __assume(0))."""
    insns = {}
    leaders = {start}
    missing_cases = []
    todo = [start]
    while todo:
        a = todo.pop()
        while start <= a < bound and a not in insns:
            code = img.bytes(a, 16)
            insn = next(MD.disasm(code, a), None)
            if insn is None:
                break
            insns[a] = insn
            m = insn.mnemonic
            if m in ("ret", "int3", "hlt", "ud2"):
                break
            if insn.group(capstone.CS_GRP_JUMP):
                op = insn.operands[0]
                if op.type == X.X86_OP_IMM:
                    if start <= op.imm < bound:
                        todo.append(op.imm)
                        leaders.add(op.imm)
                    leaders.add(a + insn.size)
                    if m == "jmp":
                        break
                elif m == "jmp":
                    if op.type == X.X86_OP_MEM and op.mem.scale == 4 and op.mem.disp:
                        k = 0
                        while True:
                            t = img.u32((op.mem.disp & 0xFFFFFFFF) + 4 * k)
                            nxt = img.u32((op.mem.disp & 0xFFFFFFFF) + 4 * (k + 1))
                            if t == 0 and nxt is not None and start <= nxt < bound:
                                # A case left out with __assume(0). (A zero
                                # followed by something else is the byte
                                # index table of a two-level switch.)
                                missing_cases.append(k)
                                k += 1
                                continue
                            if t is None or not start <= t < bound:
                                break
                            todo.append(t)
                            leaders.add(t)
                            k += 1
                    break
            a += insn.size
    return [insns[a] for a in sorted(insns)], leaders, missing_cases


def float_immediate(value):
    """An immediate that is most likely a float constant stored as an
    integer (`mov dword ptr [esp + 0x38], 0x43960000` for 300.0f): a
    magnitude between 1/1024 and 2^25 with at most 12 significant bits."""
    v = value & 0xFFFFFFFF
    exponent = (v >> 23) & 0xFF
    if not 117 <= exponent <= 152 or v & 0x7FF:
        return None
    return struct.unpack("<f", struct.pack("<I", v))[0]


def is_float_op(insn):
    m = insn.mnemonic
    return (m.endswith(("ss", "sd", "ps", "pd")) and not m.startswith(("movs", "cmps", "lods", "stos", "scas"))
            or m.startswith(("cvt", "f", "ucomi", "comi", "movap", "movup", "movlp", "movhp", "movq", "movd"))
            or m in ("movss", "movsd", "andps", "xorps", "orps", "andnps"))


def constant_arg(side, insn, names):
    """The constant a push or a mov to ecx/edx passes, if it is one. An
    address in ecx is left out: mostly `this`, which LTCG may drop."""
    ops = insn.operands
    if insn.mnemonic == "push" and ops[0].type == X.X86_OP_IMM:
        return names.constant(side, ops[0].imm)
    if (insn.mnemonic == "mov" and len(ops) == 2 and ops[0].type == X.X86_OP_REG and ops[0].reg in ARG_REGS
            and ops[1].type == X.X86_OP_IMM):
        arg = names.constant(side, ops[1].imm)
        if not re.match(r"\w+\+0x|unannotated", arg):
            return arg
    return None


def incoming_args(side, insns, leaders, va, bound, names):
    """For each basic block, the constants its predecessors pushed after
    their last call: arguments of a call the compiler shared between paths
    (each case pushes its own script number, then jumps to one call)."""
    incoming = {}
    pending = []
    for k, insn in enumerate(insns):
        if insn.address in leaders:
            pending = []
        if insn.mnemonic == "call":
            pending = []
            continue
        arg = constant_arg(side, insn, names)
        if arg is not None:
            pending.append(arg)
        nxt = insns[k + 1].address if k + 1 < len(insns) else None
        ends = nxt is None or nxt in leaders or insn.group(capstone.CS_GRP_JUMP) or insn.mnemonic == "ret"
        if not ends or not pending:
            continue
        succ = []
        op = insn.operands[0] if insn.operands else None
        if insn.group(capstone.CS_GRP_JUMP) and op is not None and op.type == X.X86_OP_IMM:
            if va <= op.imm < bound:
                succ.append(op.imm)
            if insn.mnemonic != "jmp" and nxt is not None:
                succ.append(nxt)
        elif insn.mnemonic not in ("jmp", "ret") and nxt is not None:
            succ.append(nxt)
        for t in succ:
            incoming.setdefault(t, []).append(list(pending))
    return incoming


def fingerprint(side, va, names):
    img = names.orig if side == "orig" else names.ours
    bound = names.orig_bound(va) if side == "orig" else names.ours_bound(va)
    insns, leaders, missing_cases = walk(img, va, bound)
    incoming = incoming_args(side, insns, leaders, va, bound, names)
    by_addr = {insn.address: k for k, insn in enumerate(insns)}
    # A frame realigned through ebx (push ebx; mov ebx, esp; ...; and esp, -8)
    # reaches its arguments and return address through ebx.
    frame_regs = set(STACK_REGS)
    if len(insns) > 1 and insns[0].op_str == "ebx" and insns[1].op_str == "ebx, esp":
        frame_regs.add(X.X86_REG_EBX)
    items = Counter()
    returns = set()
    pending_args = []
    from_preds = []
    prev_call = None
    import_in = {}  # register -> import whose address it holds
    for idx, insn in enumerate(insns):
        m = insn.mnemonic
        ops = insn.operands
        if insn.address in leaders:
            pending_args = []
            from_preds = incoming.get(insn.address, [])
            prev_call = None
        float_op = is_float_op(insn)
        if (m == "mov" and len(ops) == 2 and ops[0].type == X.X86_OP_REG and ops[1].type == X.X86_OP_MEM
                and ops[1].mem.base == 0 and ops[1].mem.index == 0 and ops[1].mem.disp & 0xFFFFFFFF in img.imports):
            import_in[ops[0].reg] = img.imports[ops[1].mem.disp & 0xFFFFFFFF]
            continue
        if m != "call":
            for reg in insn.regs_access()[1]:
                import_in.pop(reg, None)

        # Calls and tail jumps out of the function.
        if m == "call" or (m == "jmp" and ops and ops[0].type == X.X86_OP_IMM
                           and not va <= ops[0].imm < bound):
            op = ops[0]
            if op.type == X.X86_OP_IMM:
                target = names.code(side, op.imm)
            elif op.type == X.X86_OP_MEM and op.mem.base == 0 and op.mem.index == 0:
                target = names.data(side, op.mem.disp & 0xFFFFFFFF, 4, False)
                target = target[7:] if target.startswith("import ") else f"[{target}]"
            elif op.type == X.X86_OP_MEM:
                target = f"vcall+{op.mem.disp:#x}"
            elif op.type == X.X86_OP_REG and op.reg in import_in:
                target = import_in[op.reg]
            else:
                target = "indirect"
            if target in IGNORED_CALLS:
                continue
            for before in from_preds or [[]]:
                label = f"{'call' if m == 'call' else 'tailcall'} {target}"
                args = [a for a in before + pending_args if a != "0"]
                if args:
                    label += "(" + ", ".join(args) + ")"
                items["call: " + label] += 1
            from_preds = []
            if prev_call is not None:
                items[f"order: {prev_call} -> {target}"] += 1
            prev_call = target
            pending_args = []
            continue

        # Constants pushed or put in ecx/edx: arguments of the next call.
        arg = constant_arg(side, insn, names)
        if arg is not None:
            pending_args.append(arg)

        # Constant returns: eax set, then only the epilogue (or a jump to it).
        if (len(ops) >= 1 and ops[0].type == X.X86_OP_REG and ops[0].reg == X.X86_REG_EAX
                and (m == "xor" and ops[1].type == X.X86_OP_REG and ops[1].reg == X.X86_REG_EAX
                     or m == "mov" and ops[1].type == X.X86_OP_IMM
                     or m == "or" and ops[1].type == X.X86_OP_IMM and ops[1].imm == -1)):
            value = "0" if m == "xor" else names.constant(side, ops[1].imm)
            k, steps = idx + 1, 0
            while k < len(insns) and steps < 12:
                nxt = insns[k]
                text = f"{nxt.mnemonic} {nxt.op_str}"
                if nxt.mnemonic == "ret":
                    returns.add(value)
                    break
                if nxt.mnemonic == "jmp" and nxt.operands[0].type == X.X86_OP_IMM and nxt.operands[0].imm in by_addr:
                    k = by_addr[nxt.operands[0].imm]
                elif EPILOGUE.match(text) and "eax" not in nxt.op_str:
                    k += 1
                else:
                    break
                steps += 1

        if m.startswith(("j", "loop")) or m in ("ret", "nop", "int3"):
            continue
        sp_dest = ops and ops[0].type == X.X86_OP_REG and ops[0].reg in frame_regs

        # Comparisons with a constant and how their result is used.
        if m == "cmp" and len(ops) == 2 and ops[1].type == X.X86_OP_IMM:
            # The flags' user, past instructions that leave the flags alone.
            for nxt in insns[idx + 1:idx + 5]:
                cc = re.match(r"(?:j|set|cmov)(n?[a-z]{1,2})$", nxt.mnemonic)
                if cc or nxt.mnemonic not in ("mov", "lea", "push", "pop", "movss", "movd", "movzx", "movsx"):
                    break
            if cc and cc.group(1) in COND:
                items[f"compare: {COND[cc.group(1)]} {names.constant(side, ops[1].imm)}"] += 1

        for op in ops:
            if op.type == X.X86_OP_IMM:
                v = op.imm & 0xFFFFFFFF
                if img.section_of(v) and v >= img.base:
                    items[f"address: {names.data(side, v, 0, False)}"] += 1
                elif not sp_dest and m == "mov" and float_immediate(v) is not None:
                    items[f"fconst: f32 {float_immediate(v)!r}"] += 1
                elif not sp_dest:
                    items[f"imm: {op.imm}"] += 1
            elif op.type == X.X86_OP_MEM:
                mem = op.mem
                access = "store" if op.access & capstone.CS_AC_WRITE else "load"
                disp = mem.disp & 0xFFFFFFFF
                if m == "lea":
                    if mem.base not in frame_regs and mem.index == 0 and mem.disp and not img.section_of(disp):
                        items[f"imm: {mem.disp}"] += 1
                        continue
                    access = "addr"
                if img.section_of(disp) and disp >= img.base:
                    kind = "global" if mem.base == 0 and mem.index == 0 else "table"
                    what = names.data(side, disp, op.size, float_op)
                    if IGNORED_GLOBALS.search(what):
                        continue
                    if kind == "global" and access == "load" and what.startswith(("f32 ", "f64 ", "xmm ")):
                        # However the build gets it: loaded, or (below) an immediate.
                        items[f"fconst: {what}"] += 1
                        continue
                    if kind == "table" or access == "addr":
                        # An indexed table and a pointer walked through it
                        # start at the same address.
                        items[f"address: {what}"] += 1
                        continue
                    items[f"{kind} {access}{op.size * 8}: {what}"] += 1
                elif mem.base not in frame_regs and not (mem.base == 0 and mem.index in frame_regs):
                    if mem.base == 0 and mem.index == 0:
                        continue
                    items[f"field {access}{op.size * 8}: {mem.disp:+#x}"] += 1
    for v in returns:
        items[f"return: {v}"] += 1
    for k in missing_cases:
        items[f"switch: case {k} has no code (undefined if reached)"] += 1
    return items, len(insns)


def compare(o, r, names):
    a, na = fingerprint("orig", o, names)
    b, nb = fingerprint("ours", r, names)
    only_orig, only_ours = Counter(), Counter()
    # A load of something the function also stores: reloading it after the
    # store or keeping it in a register is register allocation.
    stored = {re.sub(r"^(\w+) store\d+", r"\1", k) for k in set(a) | set(b) if " store" in k.split(":")[0]}
    for k in set(a) | set(b):
        if " load" in k.split(":")[0] and re.sub(r"^(\w+) load\d+", r"\1", k) in stored:
            continue
        if k.startswith("order: "):
            # Only calls made in the opposite order: whether two calls share
            # a basic block depends on the layout.
            first, second = k[7:].split(" -> ")
            reverse = f"order: {second} -> {first}"
            if k in a and k not in b and reverse in b:
                only_orig[k] = 1
            if k in b and k not in a and reverse in a:
                only_ours[k] = 1
        elif COUNTED and k.split(":")[0].startswith(COUNTED):
            if a[k] > b[k]:
                only_orig[k] = a[k] - b[k]
            elif b[k] > a[k]:
                only_ours[k] = b[k] - a[k]
        elif (k in a) != (k in b):
            (only_orig if k in a else only_ours)[k] = 1
    shifted_fields(only_orig, only_ours)
    return only_orig, only_ours, na, nb


def shifted_fields(only_orig, only_ours):
    """Drop field accesses that are the same fields reached from another base:
    ours through `this` + 0xc88, the original through a pointer to that inner
    struct, which shifts every offset by one constant. A shift counts when it
    pairs at least three accesses of the same kind and size."""
    def fields(items):
        out = {}
        for k in items:
            m = re.match(r"field (\w+?)(\d+): ([+-]0x[0-9a-f]+)$", k)
            if m:
                out[k] = (m.group(1) + m.group(2), int(m.group(3), 16))
        return out
    for _ in range(3):
        fo, fu = fields(only_orig), fields(only_ours)
        deltas = Counter(u_off - o_off for o_kind, o_off in fo.values() for u_kind, u_off in fu.values()
                         if o_kind == u_kind and u_off != o_off)
        if not deltas:
            return
        delta, n = deltas.most_common(1)[0]
        if n < 3:
            return
        for ko, (kind, off) in fo.items():
            match = next((k for k, (uk, uo) in fu.items() if uk == kind and uo == off + delta and k in only_ours), None)
            if match is not None:
                del only_orig[ko]
                del only_ours[match]


def report(o, sym, only_orig, only_ours, na, nb):
    n = sum(only_orig.values()) + sum(only_ours.values())
    print(f"{o:#x} {short(sym)}: {n} difference{'s' if n != 1 else ''} ({na} vs {nb} instructions)")
    keys = sorted(set(only_orig) | set(only_ours))
    for k in keys:
        if only_orig[k]:
            print(f"    - {k}" + (f" (x{only_orig[k]})" if only_orig[k] > 1 else ""))
        if only_ours[k]:
            print(f"    + {k}" + (f" (x{only_ours[k]})" if only_ours[k] > 1 else ""))


def main():
    args = sys.argv[1:]
    names = Names(Image(ROOT / "orig/th16.exe"), Image(ROOT / "build/th16.exe"))
    wanted = {int(a, 16) for a in args if a.startswith("0x")}
    results = {}
    for o, r, sym in names.functions:
        if wanted and o not in wanted:
            continue
        if not wanted and names.orig.bytes(o, 1) == b"":
            continue
        only_orig, only_ours, na, nb = compare(o, r, names)
        results[o] = (sym, only_orig, only_ours, na, nb)

    if "--items" in args:
        for o, (sym, _, _, _, _) in sorted(results.items()):
            r = next(r for oo, r, _ in names.functions if oo == o)
            a, _ = fingerprint("orig", o, names)
            b, _ = fingerprint("ours", r, names)
            print(f"{o:#x} {short(sym)}: orig ours item")
            for k in sorted(set(a) | set(b)):
                print(f"  {a[k]:4d} {b[k]:4d}  {k}")
        return
    if "--save" in args:
        data = {f"{o:#x}": sorted([f"- {k} x{v}" for k, v in oo.items()] + [f"+ {k} x{v}" for k, v in ou.items()])
                for o, (_, oo, ou, _, _) in results.items()}
        BASELINE.write_text(json.dumps(data, indent=0))
        n = sum(1 for v in data.values() if v)
        print(f"saved {len(data)} functions ({n} with differences) to {BASELINE.relative_to(ROOT)}")
        return
    if "--check" in args:
        base = json.loads(BASELINE.read_text())
        changed = 0
        for o, (sym, oo, ou, na, nb) in sorted(results.items()):
            now = sorted([f"- {k} x{v}" for k, v in oo.items()] + [f"+ {k} x{v}" for k, v in ou.items()])
            before = base.get(f"{o:#x}", [])
            if now != before:
                changed += 1
                print(f"{o:#x} {short(sym)}: differences changed")
                for line in sorted(set(now) - set(before)):
                    print(f"    new  {line}")
                for line in sorted(set(before) - set(now)):
                    print(f"    gone {line}")
        print(f"{changed} function{'s' if changed != 1 else ''} changed since --save")
        return

    rows = [(sum(oo.values()) + sum(ou.values()), o) for o, (_, oo, ou, _, _) in results.items()]
    shown = 0
    for n, o in sorted(rows, key=lambda t: (-t[0], t[1])):
        if n == 0 and not wanted:
            continue
        sym, oo, ou, na, nb = results[o]
        report(o, sym, oo, ou, na, nb)
        shown += 1
    if not wanted:
        print(f"{shown} of {len(results)} functions differ in behaviour items")


if __name__ == "__main__":
    main()
