#!/usr/bin/env python3
"""Compare build/th16.exe against the original with reccmp.

Runs reccmp-reccmp in-process with two adjustments for this project:

- its cvdump.exe runs inside this project's Wine prefix (not ~/.wine) and
  without a display;
- C++ EH data is recognized in its VS2017 form. reccmp only knows the VC6
  FuncInfo magic (0x19930520); VS2005 and later write 0x19930522, with the
  same leading fields. And reccmp finds handler thunks by their
  `mov eax, <FuncInfo>; jmp __CxxFrameHandler3` tail, but with /GS the thunk
  begins earlier with a stack cookie check (`mov edx,[esp+8];
  lea eax,[edx+N]; mov ecx,[edx-M]; xor ecx,eax; call
  __security_check_cookie`), and code refers to that start.

Arguments are passed through, e.g.:

  scripts/compare.py                  # summary of every annotated function
  scripts/compare.py -v 0x401300      # assembly diff for one function
  scripts/compare.py --html build/report.html
"""

import os
import re
import sys
from pathlib import Path

import capstone

sys.path.insert(0, str(Path(__file__).resolve().parent))
import toolchain as tc  # noqa: E402

MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

# Prefix instructions a /GS EH handler thunk may start with.
COOKIE_CHECK_MNEMONICS = {"mov", "lea", "xor", "call"}


def thunk_start(image, handler_addr):
    """Walk back from the `mov eax, FuncInfo` to the start of a cookie-check
    prefix that begins with `mov edx, [esp+8]` and runs straight into it."""
    for back in range(12, 48):
        start = handler_addr - back
        try:
            code = image.read(start, back)
        except Exception:
            continue
        if not code.startswith(b"\x8b\x54\x24\x08"):
            continue
        insns = list(MD.disasm(code, start))
        if insns and insns[-1].address + insns[-1].size == handler_addr and \
                all(i.mnemonic in COOKIE_CHECK_MNEMONICS for i in insns):
            return start
    return handler_addr


def patch_reccmp():
    import reccmp.analysis.funcinfo as funcinfo
    import reccmp.compare.analyze as analyze

    funcinfo.FUNCINFO_MAGIC_RE = re.compile(rb"[\x20\x21\x22]\x05\x93\x19", flags=re.S)

    original = analyze.find_eh_handlers

    def find_eh_handlers(image):
        for handler_addr, funcinfo in original(image):
            yield thunk_start(image, handler_addr), funcinfo

    analyze.find_eh_handlers = find_eh_handlers


def main():
    os.environ.update(tc.env())
    for var in ("DISPLAY", "WAYLAND_DISPLAY"):
        os.environ.pop(var, None)
    os.chdir(tc.ROOT)
    args = sys.argv[1:]
    if "--target" not in args and "--paths" not in args:
        args = ["--target", "TH16", "--nolib", *args]
    patch_reccmp()
    from reccmp.tools.asmcmp import main as reccmp_main
    sys.argv = ["reccmp-reccmp", *args]
    sys.exit(reccmp_main())


if __name__ == "__main__":
    main()
