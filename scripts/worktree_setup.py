#!/usr/bin/env python3
"""Prepare an extra git worktree of this repository for building.

The toolchain (prefix/), the original executable (orig/) and the Python
environment (.venv/) are not in git. This links them from the main
checkout, the first worktree `git worktree list` reports, and writes the
per-user reccmp file. Builds then go to the worktree's own build/.

Usage (from inside the new worktree):
  python3 scripts/worktree_setup.py
"""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SHARED = ("prefix", "orig", ".venv")


def main_checkout():
    out = subprocess.run(["git", "-C", str(ROOT), "worktree", "list", "--porcelain"],
                         capture_output=True, text=True, check=True).stdout
    first = next(l for l in out.splitlines() if l.startswith("worktree "))
    return Path(first.split(" ", 1)[1])


def main():
    main = main_checkout()
    if main.resolve() == ROOT:
        sys.exit("this is the main checkout; run scripts/setup.py here instead")
    for name in SHARED:
        link = ROOT / name
        target = main / name
        if not target.exists():
            sys.exit(f"{target} is missing; run scripts/setup.py in the main checkout first")
        if link.is_symlink() or link.exists():
            continue
        link.symlink_to(target)
        print(f"linked {name} -> {target}")
    (ROOT / "reccmp-user.yml").write_text(f"targets:\n  TH16:\n    path: {ROOT / 'orig' / 'th16.exe'}\n")
    print("worktree ready")


if __name__ == "__main__":
    main()
