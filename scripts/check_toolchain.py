#!/usr/bin/env python3
"""Sanity-check the toolchain against th16.exe.

Builds tests/crt_probe (an empty WinMain) with the project's compiler and
linker flags, then looks for every static CRT/UCRT function it links inside
the original executable. With the right MSVC and UCRT versions every
testable function is found.
"""

import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build  # noqa: E402
import toolchain as tc  # noqa: E402

OUT = tc.ROOT / "build/crt_probe"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    src = tc.ROOT / "tests/crt_probe/main.cpp"
    obj = OUT / "main.obj"
    rc, out = tc.run("cl", build.CFLAGS + [f"/Fo{tc.winpath(obj)}", f"/Fd{tc.winpath(OUT / 'main.pdb')}", tc.winpath(src)])
    if rc:
        sys.exit(out)
    exe = OUT / "probe.exe"
    # /DYNAMICBASE so the scan can mask absolute addresses via relocations.
    flags = [f for f in build.LFLAGS if f != "/DYNAMICBASE:NO"] + ["/DYNAMICBASE"]
    rc, out = tc.run("link", flags + [
        f"/OUT:{tc.winpath(exe)}", f"/PDB:{tc.winpath(exe.with_suffix('.pdb'))}",
        f"/MAP:{tc.winpath(exe.with_suffix('.map'))}", tc.winpath(obj), *build.LIBS,
    ])
    if rc:
        sys.exit(out)
    banner = subprocess.run(["wine", str(tc.BIN / "cl.exe")], env=tc.env(), capture_output=True, text=True).stderr.splitlines()[0]
    print(banner)
    result = subprocess.run(
        [sys.executable, str(tc.ROOT / "scripts/sigscan.py"), str(exe), str(exe.with_suffix(".map"))],
        capture_output=True, text=True)
    print(result.stdout, end="")
    if " 0 not found" not in result.stdout:
        sys.exit("toolchain check FAILED: some library functions are missing from th16.exe")
    print("toolchain check passed")


if __name__ == "__main__":
    main()
