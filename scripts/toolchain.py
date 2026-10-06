"""Paths and Wine invocation for the MSVC 14.10.25017 toolchain in prefix/."""

import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PREFIX = ROOT / "prefix"
WINEPREFIX = PREFIX / "wine"

MSVC = PREFIX / "vs/VC/Tools/MSVC/14.10.25017"
BIN = MSVC / "bin/Hostx86/x86"
SDK71A = PREFIX / "sdk/Program Files/Microsoft SDKs/Windows/v7.1A"
UCRT_VER = "10.0.10240.0"
KITS10 = PREFIX / "sdk/Windows Kits/10"
DXSDK = PREFIX / "dxsdk"

ORIG_EXE = ROOT / "orig/th16.exe"

# Include and library search order of the v141_xp platform toolset, with the
# DirectX SDK appended the way a project's VC++ Directories would add it.
INCLUDE = [
    MSVC / "include",
    KITS10 / "Include" / UCRT_VER / "ucrt",
    SDK71A / "Include",
    DXSDK / "Include",
]
LIB = [
    MSVC / "lib/x86",
    KITS10 / "Lib" / UCRT_VER / "ucrt/x86",
    SDK71A / "Lib",
    DXSDK / "Lib/x86",
]


def winpath(p):
    return "Z:" + str(Path(p).resolve()).replace("/", "\\")


def env():
    e = dict(os.environ)
    e["WINEPREFIX"] = str(WINEPREFIX)
    e["WINEDEBUG"] = "-all"
    e["WINEDLLOVERRIDES"] = "mscoree,mshtml="
    # Console tools only; no display needed.
    e.pop("DISPLAY", None)
    e.pop("WAYLAND_DISPLAY", None)
    e["INCLUDE"] = ";".join(winpath(p) for p in INCLUDE)
    e["LIB"] = ";".join(winpath(p) for p in LIB)
    # Keep cl from looking for a shared PDB server between invocations.
    e["_CL_"] = ""
    return e


def run(tool, args, cwd=None):
    """Run cl/link/lib under Wine; returns (returncode, output text).

    Output goes through a file rather than a pipe: cl /Zi spawns mspdbsrv.exe,
    which outlives the compiler and would hold a pipe open indefinitely.
    """
    log_dir = ROOT / "build"
    log_dir.mkdir(exist_ok=True)
    with tempfile.TemporaryFile(dir=log_dir) as out:
        rc = subprocess.run(
            ["wine", str(BIN / f"{tool}.exe"), *args],
            env=env(), cwd=cwd, stdin=subprocess.DEVNULL, stdout=out, stderr=subprocess.STDOUT,
        ).returncode
        out.seek(0)
        return rc, out.read().decode("utf-8", "replace")
