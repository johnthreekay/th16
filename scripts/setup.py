#!/usr/bin/env python3
"""Download, verify and unpack the TH16 toolchain into prefix/, and copy the
original executable into orig/.

Toolchain (see scripts/toolchain.json for pinned URLs and SHA-256 hashes):
  - MSVC 14.10.25017 (Visual Studio 2017 15.0), x86 host and target
  - Windows 7.1A SDK (the v141_xp platform toolset's Windows headers/libs)
  - Universal CRT headers and static libraries
  - DirectX SDK (June 2010), for d3dx9

Everything Windows-side runs under Wine, in a private prefix at prefix/wine.

Usage: scripts/setup.py --game-dir "/path/to/Touhou 16 install"
"""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PREFIX = ROOT / "prefix"
DOWNLOADS = PREFIX / "downloads"
WINEPREFIX = PREFIX / "wine"

# th16.exe 1.00a, the only release.
ORIG_SHA256 = "c11776019f083978e66027e7394dafb1fb9543afca986f28049a49417e341929"


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def download(url, dest, expected):
    if dest.exists() and (expected is None or sha256(dest) == expected):
        return
    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp = dest.with_suffix(dest.suffix + ".part")
    print(f"  downloading {dest.name}")
    with urllib.request.urlopen(url) as r, open(tmp, "wb") as f:
        shutil.copyfileobj(r, f, 1 << 20)
    got = sha256(tmp)
    if expected is not None and got != expected:
        tmp.unlink()
        sys.exit(f"SHA-256 mismatch for {dest.name}: got {got}, expected {expected}")
    tmp.rename(dest)
    if expected is None:
        print(f"    (unpinned) sha256 {got}")


def wine_env():
    env = dict(os.environ)
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"
    env["WINEDLLOVERRIDES"] = "mscoree,mshtml="
    return env


def winpath(p):
    return "Z:" + str(p).replace("/", "\\")


def ensure_wineprefix():
    if (WINEPREFIX / "system.reg").exists():
        return
    print("Creating Wine prefix")
    subprocess.run(["wineboot", "-i"], env=wine_env(), check=True)
    # Console tools only: the null graphics driver keeps Wine from touching
    # the display on every compiler invocation.
    subprocess.run(["wine", "reg", "add", r"HKCU\Software\Wine\Drivers", "/v", "Graphics", "/d", "null", "/f"],
                   env=wine_env(), check=True, capture_output=True)
    subprocess.run(["wineserver", "-w"], env=wine_env(), check=True)


def extract_vsix(archive, dest):
    # VSIX payloads keep their install layout under Contents/, with
    # percent-encoded member names.
    with zipfile.ZipFile(archive) as z:
        for info in z.infolist():
            name = urllib.parse.unquote(info.filename)
            if not name.startswith("Contents/") or name.endswith("/"):
                continue
            out = dest / name[len("Contents/"):]
            out.parent.mkdir(parents=True, exist_ok=True)
            with z.open(info) as src, open(out, "wb") as dst:
                shutil.copyfileobj(src, dst)


def extract_msi(msi, dest):
    # Administrative install unpacks the MSI and its external cabs without
    # touching the registry.
    dest.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        ["wine", "msiexec", "/a", winpath(msi), "/qn", f"TARGETDIR={winpath(dest)}"],
        env=wine_env(), check=True,
    )
    subprocess.run(["wineserver", "-w"], env=wine_env(), check=True)


def extract_dxsdk(exe, dest):
    # DXSDK_Jun10.exe is a cab-based self-extractor. Only the headers and x86
    # import libraries are needed.
    stage = DOWNLOADS / "dxsdk-unpacked"
    if not stage.exists():
        stage.mkdir(parents=True)
        subprocess.run(["cabextract", "-q", "-d", str(stage), str(exe)], check=True)
    src = stage / "DXSDK"
    for sub in ("Include", "Lib/x86"):
        shutil.copytree(src / sub, dest / sub, dirs_exist_ok=True)
    shutil.rmtree(stage)


def setup_toolchain():
    packages = json.loads((ROOT / "scripts" / "toolchain.json").read_text())
    stamp_dir = PREFIX / "stamps"
    stamp_dir.mkdir(parents=True, exist_ok=True)
    for pkg in packages:
        key = f"{pkg['id']}-{pkg['version']}"
        stamp = stamp_dir / key
        if stamp.exists():
            continue
        print(f"{pkg['id']} {pkg['version']}")
        pkg_dir = DOWNLOADS / key
        for pl in pkg["payloads"]:
            download(pl["url"], pkg_dir / pl["file"], pl["sha256"])
        first = pkg_dir / pkg["payloads"][0]["file"]
        if pkg["kind"] == "vsix":
            extract_vsix(first, PREFIX / "vs")
        elif pkg["kind"] == "msi":
            ensure_wineprefix()
            extract_msi(first, PREFIX / "sdk")
        elif pkg["kind"] == "dxsdk":
            extract_dxsdk(first, PREFIX / "dxsdk")
        stamp.touch()


def setup_orig(game_dir):
    exe = Path(game_dir) / "th16.exe"
    dest = ROOT / "orig" / "th16.exe"
    if not exe.exists():
        sys.exit(f"{exe} not found")
    h = sha256(exe)
    if h != ORIG_SHA256:
        sys.exit(f"{exe} has SHA-256 {h}, expected {ORIG_SHA256} (th16.exe 1.00a)")
    dest.parent.mkdir(exist_ok=True)
    shutil.copy2(exe, dest)
    (ROOT / "reccmp-user.yml").write_text(f"targets:\n  TH16:\n    path: {dest}\n")
    print(f"Copied {exe} -> {dest}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--game-dir", help="directory containing the original th16.exe")
    args = ap.parse_args()
    if args.game_dir:
        setup_orig(args.game_dir)
    elif not (ROOT / "orig" / "th16.exe").exists():
        sys.exit("pass --game-dir on the first run so orig/th16.exe can be set up")
    setup_toolchain()
    ensure_wineprefix()
    print("Toolchain ready.")


if __name__ == "__main__":
    main()
