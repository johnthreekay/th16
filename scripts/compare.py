#!/usr/bin/env python3
"""Compare build/th16.exe against the original with reccmp.

Thin wrapper around reccmp-reccmp that runs its cvdump.exe inside this
project's Wine prefix (not ~/.wine) and without a display. Arguments are
passed through, e.g.:

  scripts/compare.py                  # summary of every annotated function
  scripts/compare.py -v 0x401300      # assembly diff for one function
  scripts/compare.py --html build/report.html
"""

import os
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import toolchain as tc  # noqa: E402


def main():
    reccmp = shutil.which("reccmp-reccmp") or str(Path(sys.executable).parent / "reccmp-reccmp")
    env = tc.env()
    args = sys.argv[1:]
    if "--target" not in args and "--paths" not in args:
        args = ["--target", "TH16", "--nolib", *args]
    os.chdir(tc.ROOT)
    os.execve(reccmp, [reccmp, *args], env)


if __name__ == "__main__":
    main()
