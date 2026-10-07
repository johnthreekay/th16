#!/usr/bin/env python3
"""Build build/th16.exe (+ .pdb, .map) from src/ with MSVC 14.10.25017.

ZUN's build used whole-program optimization: every object is compiled with
/GL and code generation happens at link time (/LTCG). Until the whole game is
decompiled nothing calls most of our functions, so the linker would discard
them. To keep each annotated function alive we pass /INCLUDE:<decorated name>.
/GL objects carry no symbol table, so the decorated names come from a second,
cheap compile of each file without /GL.
"""

import argparse
import functools
import hashlib
import re
import struct
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import toolchain as tc  # noqa: E402

ROOT = tc.ROOT
SRC = ROOT / "src"
BUILD = ROOT / "build"

# Visual Studio 2017 "Release | Win32" defaults with the v141_xp toolset,
# which is what th16.exe's Rich header, POGO record and code all point to:
# whole-program optimization, /GS cookies, C++ EH, SSE2 math, static CRT.
CFLAGS = [
    "/nologo", "/c", "/GS", "/GL", "/W3", "/Gy", "/Zc:wchar_t", "/Zi", "/Gm-",
    "/O2", "/Oy-", "/Zc:inline", "/fp:precise", "/Zc:forScope", "/Gd", "/Oi", "/MT",
    "/EHsc", "/FC", "/WX-",
    "/DWIN32", "/DNDEBUG", "/D_WINDOWS", "/D_USING_V110_SDK71_", "/D_MBCS",
    # Only silences the CRT deprecation warnings; no effect on code.
    "/D_CRT_SECURE_NO_WARNINGS",
]
LFLAGS = [
    "/nologo", "/LTCG", "/INCREMENTAL:NO", "/NXCOMPAT", "/DYNAMICBASE:NO",
    # /OPT:NOICF: th16.exe keeps several sets of byte-identical functions
    # (twelve copies of one fsincos helper) that identical COMDAT folding
    # would have merged.
    "/MACHINE:X86", "/SAFESEH", "/OPT:REF", "/OPT:NOICF",
    "/SUBSYSTEM:WINDOWS,5.01", "/DEBUG",
]
LIBS = [
    "kernel32.lib", "user32.lib", "gdi32.lib", "winmm.lib", "ole32.lib",
    "d3d9.lib", "d3dx9.lib", "dinput8.lib", "dsound.lib", "dxguid.lib",
]

ANNOTATION = re.compile(r"//\s*FUNCTION:\s*TH16\s+(0x[0-9a-fA-F]+)")
SYNTHETIC = re.compile(r"//\s*SYNTHETIC:\s*TH16\s+(0x[0-9a-fA-F]+)")


def sources():
    return sorted(SRC.rglob("*.cpp"))


def rel(p):
    return p.relative_to(ROOT)


def is_stub(src):
    """src/stub/ holds placeholders for functions not decompiled yet. They are
    compiled without /GL so link-time code generation cannot look inside them:
    to the rest of the program they stay opaque external calls (which may
    throw, and keep the standard calling convention), like the real thing."""
    return SRC / "stub" in src.parents


@functools.lru_cache(maxsize=None)
def project_files():
    """Lowercased path -> path of every file under src/."""
    return {str(p).lower(): p for p in SRC.rglob("*") if p.is_file()}


def input_key(flags, src, deps):
    """Hash of everything an object depends on: flags, source and the
    project headers it included (SDK and CRT headers never change)."""
    h = hashlib.sha256(("deps-v2\0" + "\0".join(flags)).encode())
    for path in [src, *deps]:
        p = Path(path)
        h.update(str(p).encode() + b"\0")
        h.update(p.read_bytes() if p.is_file() else b"\0missing\0")
    return h.hexdigest()


def compile_one(src, obj_dir, no_gl):
    obj = obj_dir / rel(src).with_suffix(".obj")
    obj.parent.mkdir(parents=True, exist_ok=True)
    flags = [f for f in CFLAGS if not ((no_gl or is_stub(src)) and f == "/GL")]
    # Reuse the object while its inputs are unchanged: a build otherwise runs
    # cl twice for every file under Wine.
    stamp = obj.with_suffix(".dep")
    if obj.exists() and stamp.exists():
        key, *deps = stamp.read_text().splitlines()
        if key == input_key(flags, src, deps):
            return src, obj, 0, []
    rc, out = tc.run("cl", flags + [
        "/showIncludes",
        f"/Fo{tc.winpath(obj)}", f"/Fd{tc.winpath(obj.with_suffix('.pdb'))}", tc.winpath(src),
    ])
    deps, lines = [], []
    for l in out.splitlines():
        m = re.match(r"Note: including file:\s*(.+)", l)
        if m:
            # cl lowercases the paths of quoted includes.
            path = re.sub(r"^[A-Za-z]:", "", m.group(1).strip()).replace("\\", "/")
            real = project_files().get(path.lower())
            if real:
                deps.append(str(real))
        elif l.strip() and l.strip() != src.name:
            lines.append(l)
    deps = sorted(set(deps))
    if rc == 0:
        stamp.write_text("\n".join([input_key(flags, src, deps), *deps]) + "\n")
    else:
        stamp.unlink(missing_ok=True)
    return src, obj, rc, lines


def compile_all(obj_dir, no_gl=False):
    with ThreadPoolExecutor() as pool:
        results = list(pool.map(lambda s: compile_one(s, obj_dir, no_gl), sources()))
    ok = True
    for src, obj, rc, lines in results:
        for l in lines:
            print(l)
        ok &= rc == 0
    if not ok:
        sys.exit("compile failed")
    return [obj for _, obj, _, _ in results]


def coff_functions(obj, storage_class=2):
    """Function symbols defined in a (non-/GL) COFF object: external ones by
    default, static (internal linkage) ones with storage_class=3."""
    data = obj.read_bytes()
    _, _, _, symtab, nsyms, _, _ = struct.unpack_from("<HHIIIHH", data, 0)
    strtab = symtab + 18 * nsyms
    out = []
    i = 0
    while i < nsyms:
        off = symtab + 18 * i
        raw, _, section, typ, storage, naux = struct.unpack_from("<8sIhHBB", data, off)
        if raw[:4] == b"\0\0\0\0":
            start = strtab + struct.unpack_from("<I", raw, 4)[0]
            name = data[start:data.index(b"\0", start)]
        else:
            name = raw.rstrip(b"\0")
        if section > 0 and storage == storage_class and typ == 0x20:
            out.append(name.decode())
        i += 1 + naux
    return out


def undecorate(names):
    """Map decorated -> qualified function name (no return type or params)."""
    if not names:
        return {}
    # Wine caps a command line at 32767 characters, so undecorate in batches.
    out = ""
    batch = []
    for name in names + [None]:
        if name is None or sum(len(n) + 1 for n in batch) + len(name) > 24000:
            if batch:
                out += tc.run("undname", batch)[1]
            batch = []
        if name is not None:
            batch.append(name)
    result = {}
    for m in re.finditer(r'Undecoration of :- "(.+?)"\s*is :- "(.+?)"', out):
        full = m.group(2)
        head = full.split("(", 1)[0].split()
        result[m.group(1)] = head[-1] if head else full
    for name in names:
        # This undname cannot decode some newer manglings (__vectorcall's
        # "YQ"), so read the qualified name out of the decorated one.
        if result.get(name, name) == name:
            qualified = qualified_from_decorated(name)
            if qualified:
                result[name] = qualified
    return result


# MSVC's codes for special member functions and operators: ??<code><scope>@@
OPERATOR_CODES = {
    "2": "operator new", "3": "operator delete", "4": "operator=", "5": "operator>>", "6": "operator<<",
    "7": "operator!", "8": "operator==", "9": "operator!=", "A": "operator[]", "C": "operator->",
    "D": "operator*", "E": "operator++", "F": "operator--", "G": "operator-", "H": "operator+",
    "I": "operator&", "J": "operator->*", "K": "operator/", "L": "operator%", "M": "operator<",
    "N": "operator<=", "O": "operator>", "P": "operator>=", "Q": "operator,", "R": "operator()",
    "S": "operator~", "T": "operator^", "U": "operator|", "V": "operator&&", "W": "operator||",
    "X": "operator*=", "Y": "operator+=", "Z": "operator-=", "_0": "operator/=", "_1": "operator%=",
    "_2": "operator>>=", "_3": "operator<<=", "_4": "operator&=", "_5": "operator|=", "_6": "operator^=",
}


def qualified_from_decorated(name):
    """Qualified name (no parameters) of a non-template decorated name."""
    m = re.match(r"\?([A-Za-z_]\w*)((?:@[A-Za-z_]\w*)*)@@", name)
    if m:
        scopes = [p for p in m.group(2).split("@") if p]
        return "::".join(list(reversed(scopes)) + [m.group(1)])
    m = re.match(r"\?\?(_[0-9]|[0-9A-Z])([A-Za-z_]\w*(?:@[A-Za-z_]\w*)*)@@", name)
    if m:
        scopes = list(reversed(m.group(2).split("@")))
        code = m.group(1)
        if code == "0":
            member = scopes[-1]
        elif code == "1":
            member = "~" + scopes[-1]
        elif code in OPERATOR_CODES:
            member = OPERATOR_CODES[code]
        else:
            return None
        return "::".join(scopes + [member])
    return None


def annotated_functions():
    """[(address, qualified name, source file)] from // FUNCTION: annotations."""
    found = []
    for src in sources():
        lines = src.read_text(errors="replace").splitlines()
        for i, line in enumerate(lines):
            m = ANNOTATION.search(line)
            if not m:
                continue
            decl = ""
            for nxt in lines[i + 1:]:
                decl += " " + nxt.strip()
                if "(" in decl:
                    break
            name = decl.split("(", 1)[0].split()[-1].lstrip("*&")
            found.append((int(m.group(1), 16), name, rel(src), "HARNESS_CALLED" in decl))
    return found


def keepalive_symbols():
    """[(original address, decorated symbol, harness_called)] for every
    annotated function, plus (None, symbol, False) for every function
    defined in src/harness/ (stand-in callers stay alive the same way)."""
    sym_dir = BUILD / "sym"
    objs = compile_all(sym_dir, no_gl=True)
    harness_objs = [o for o in objs if (sym_dir / "src" / "harness") in o.parents]
    decorated = sorted({n for o in objs for n in coff_functions(o)})
    qualified = undecorate(decorated)
    by_name = {}
    for d in decorated:
        by_name.setdefault(qualified.get(d, d), []).append(d)
    statics = {}
    for o in objs:
        for sym in coff_functions(o, storage_class=3):
            statics.setdefault(o.stem, []).append(sym)
    static_qualified = undecorate(sorted({d for syms in statics.values() for d in syms}))
    include = []
    for addr, name, src, harness_called in annotated_functions():
        matches = by_name.get(name, [])
        if not matches:
            # A function with internal linkage (a per-file static copy) can't
            # be forced alive with /INCLUDE, but exists wherever it is used.
            local = [d for d in statics.get(src.stem, []) if static_qualified.get(d, d) == name]
            if len(local) == 1:
                include.append((addr, local[0], "static:" + src.stem))
                continue
        if len(matches) != 1:
            sys.exit(f"{src}: {addr:#x} {name}: {len(matches)} matching symbols {matches}")
        include.append((addr, matches[0], harness_called))
    for obj in harness_objs:
        include.extend((None, sym, False) for sym in coff_functions(obj))
    return include


def synthetic_functions():
    """[(address, decorated symbol)] for // SYNTHETIC: annotations, whose next
    line names a compiler-generated function. Only scalar deleting
    destructors so far."""
    found = []
    for src in sources():
        lines = src.read_text(errors="replace").splitlines()
        for i, line in enumerate(lines):
            m = SYNTHETIC.search(line)
            if not m:
                continue
            name = lines[i + 1].strip().lstrip("/").strip()
            sdd = re.fullmatch(r"((?:\w+::)*\w+)::`scalar deleting destructor'", name)
            if not sdd:
                sys.exit(f"{rel(src)}:{i + 2}: unsupported synthetic function {name!r}")
            mangled = "@".join(reversed(sdd.group(1).split("::")))
            found.append((int(m.group(1), 16), f"??_G{mangled}@@"))
    return found


def write_function_map(include, map_path, out_path):
    """build/functions.txt: original address, our address, symbol."""
    ours = {}
    per_obj = {}
    for line in map_path.read_text(errors="replace").splitlines():
        m = re.match(r"\s*0001:[0-9a-f]+\s+(\S+)\s+([0-9a-f]{8})\s+f?\s*i?\s*(\S+)\.obj", line)
        if m:
            ours.setdefault(m.group(1), int(m.group(2), 16))
            per_obj[(m.group(1), m.group(3))] = int(m.group(2), 16)
    with open(out_path, "w") as f:
        for addr, sym, keep in include:
            if addr is None:
                continue
            if isinstance(keep, str) and keep.startswith("static:"):
                va = per_obj.get((sym, keep[len("static:"):]))
            else:
                va = ours.get(sym)
            if va is not None:
                f.write(f"{addr:#x} {va:#x} {sym}\n")
        for addr, prefix in synthetic_functions():
            for sym, va in ours.items():
                if sym.startswith(prefix):
                    f.write(f"{addr:#x} {va:#x} {sym}\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.parse_args()
    objs = compile_all(BUILD / "obj")
    include = keepalive_symbols()
    exe = BUILD / "th16.exe"
    link_args = LFLAGS + [f"/INCLUDE:{s}" for _, s, keep in include if not keep] + [
        f"/OUT:{tc.winpath(exe)}", f"/PDB:{tc.winpath(exe.with_suffix('.pdb'))}",
        f"/MAP:{tc.winpath(exe.with_suffix('.map'))}",
        *[tc.winpath(o) for o in objs], *LIBS,
    ]
    # Wine caps a command line at 32767 characters, which the object list
    # and /INCLUDE options outgrew; link reads them from a response file.
    rsp = BUILD / "link.rsp"
    rsp.write_text("\n".join(f'"{a}"' if " " in a else a for a in link_args) + "\n")
    rc, out = tc.run("link", [f"@{tc.winpath(rsp)}"])
    out = "\n".join(l for l in out.splitlines() if l.strip() not in ("Generating code", "Finished generating code"))
    if out.strip():
        print(out)
    if rc != 0:
        sys.exit("link failed")
    write_function_map(include, exe.with_suffix(".map"), BUILD / "functions.txt")
    print(f"built {rel(exe)} ({sum(1 for a, _, _ in include if a is not None)} annotated functions)")

    (ROOT / "reccmp-build.yml").write_text(
        f"project: {ROOT}\ntargets:\n  TH16:\n    path: build/th16.exe\n    pdb: build/th16.pdb\n")

    # Locate the static library code we linked inside the original, so the
    # comparison can resolve calls into the CRT.
    subprocess.run([sys.executable, str(ROOT / "scripts/sigscan.py"), str(exe), str(exe.with_suffix(".map")),
                    "--only-lib", "--csv", str(BUILD / "lib.csv"), "--pairs", str(BUILD / "functions.txt")], check=True)


if __name__ == "__main__":
    main()
