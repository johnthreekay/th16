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
- static functions named by their decorated string in the PDB (dynamic
  initializers and atexit destructors) get that string as their symbol.
- vtables stay vtables. VS2017 PDBs also list each vtable among the global
  variables, and reccmp's handling of those retypes it as plain data, so it
  never pairs with the // VTABLE: annotation. And a vtable is taken to end at
  the first entry that does not point into .text: reccmp otherwise sizes it
  by the distance to the next symbol and reads neighbouring data as slots.

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

    import reccmp.cvdump.analysis as cvdump_analysis
    from reccmp.types import EntityType, ImageId

    def get_node_type(self):
        return self.__dict__.get("_node_type")

    def set_node_type(self, value):
        if value == EntityType.DATA and self.__dict__.get("_node_type") == EntityType.VTABLE:
            return
        self.__dict__["_node_type"] = value

    cvdump_analysis.CvdumpNode.node_type = property(get_node_type, set_node_type)

    from reccmp.compare.core import Compare

    original_compare_vtable = Compare._compare_vtable

    def table_size(image, addr, limit):
        text = next(s for s in image.sections if s.name == ".text")
        lo, hi = image.imagebase + text.virtual_address, image.imagebase + text.virtual_address + text.virtual_size
        n = 0
        while n * 4 < limit:
            entry = int.from_bytes(image.read(addr + 4 * n, 4), "little")
            if not lo <= entry < hi:
                break
            n += 1
        return 4 * n

    def compare_vtable(self, match):
        store = match._kvstore
        guess = max(match.any_size(ImageId.ORIG), match.any_size(ImageId.RECOMP), 4)
        store["orig_size"] = table_size(self.orig_bin, match.orig_addr, guess) or 4
        store["recomp_size"] = table_size(self.recomp_bin, match.recomp_addr, guess) or 4
        return original_compare_vtable(self, match)

    Compare._compare_vtable = compare_vtable

    # Static functions have no public symbol, and the PDB names some of them
    # by their decorated string (dynamic initializers ??__E, atexit
    # destructors ??__F). Treat that as their symbol so the // SYNTHETIC:
    # annotations that name them by symbol can pair.
    import reccmp.compare.core as core
    original_load_cvdump = core.load_cvdump

    def load_cvdump(analysis, db, recomp_bin):
        for node in analysis.nodes:
            if node.decorated_name is None and (node.friendly_name or "").startswith("?"):
                node.decorated_name = node.friendly_name
        return original_load_cvdump(analysis, db, recomp_bin)

    core.load_cvdump = load_cvdump


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
