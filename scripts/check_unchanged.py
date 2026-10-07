#!/usr/bin/env python3
"""Check that a source change did not change the generated code.

For readability work (renames, comments, enums) the build must stay as it
was. Save a baseline first, change the source, rebuild, then compare:

  scripts/check_unchanged.py --save            # after a build: baseline
  ... edit, scripts/build.py ...
  scripts/check_unchanged.py                   # compare against it

The baseline keeps every function's quickdiff result (status and
similarity against the original) and the bytes of each section of
build/th16.exe. A rename may move functions or data, so section bytes can
differ while every function still compiles the same; the per-function
results are the authority, and the section comparison only reports.
"""

import json
import subprocess
import sys
from pathlib import Path

import pefile

ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / "build/unchanged_baseline.json"


def quickdiff():
    out = subprocess.run([sys.executable, str(ROOT / "scripts/quickdiff.py")], capture_output=True,
                         text=True, check=True).stdout
    result = {}
    for line in out.splitlines():
        addr, rest = line.split(" ", 1)
        result[addr] = rest.split(": ", 1)[1] if ": " in rest else rest
    return result


def sections():
    pe = pefile.PE(str(ROOT / "build/th16.exe"))
    debug = [(e.struct.AddressOfRawData, e.struct.AddressOfRawData + e.struct.SizeOfData)
             for e in getattr(pe, "DIRECTORY_ENTRY_DEBUG", [])]
    out = {}
    for s in pe.sections:
        data = bytearray(s.get_data())
        # The PDB signature in the debug directory changes on every link.
        for lo, hi in debug:
            for rva in range(max(lo, s.VirtualAddress), min(hi, s.VirtualAddress + len(data))):
                data[rva - s.VirtualAddress] = 0
        out[s.Name.rstrip(b"\0").decode()] = data.hex()
    return out


def main():
    if "--save" in sys.argv:
        BASE.write_text(json.dumps({"functions": quickdiff(), "sections": sections()}))
        print(f"baseline saved to {BASE.relative_to(ROOT)}")
        return
    base = json.loads(BASE.read_text())
    now = quickdiff()
    changed = [a for a in sorted(set(base["functions"]) | set(now))
               if base["functions"].get(a) != now.get(a)]
    for a in changed:
        print(f"{a}: {base['functions'].get(a, 'missing')} -> {now.get(a, 'missing')}")
    secs = sections()
    for name, data in secs.items():
        old = base["sections"].get(name)
        if old != data:
            print(f"section {name}: bytes differ (functions or data moved)")
    if changed:
        sys.exit(f"{len(changed)} functions changed")
    print(f"all {len(now)} functions unchanged")


if __name__ == "__main__":
    main()
