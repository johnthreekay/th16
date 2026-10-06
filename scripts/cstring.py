#!/usr/bin/env python3
"""Print a C string literal for the NUL-terminated string at an address in
th16.exe, with non-ASCII bytes escaped so the source encoding cannot change
them (Shift-JIS text stays byte-exact). The decoded text is printed as a
comment above it.

Usage: cstring.py 0x490da8
"""

import sys
from pathlib import Path

import pefile

ROOT = Path(__file__).resolve().parent.parent
ESCAPES = {ord("\\"): "\\\\", ord('"'): '\\"', ord("\n"): "\\n", ord("\r"): "\\r", ord("\t"): "\\t"}


def c_literal(data):
    out = '"'
    prev_hex = False
    for b in data:
        ch = chr(b)
        if b in ESCAPES:
            out += ESCAPES[b]
            prev_hex = False
        elif 0x20 <= b < 0x7F:
            if prev_hex and ch in "0123456789abcdefABCDEF":
                out += '" "'
            out += ch
            prev_hex = False
        else:
            out += f"\\x{b:02x}"
            prev_hex = True
    return out + '"'


def main():
    pe = pefile.PE(str(ROOT / "orig/th16.exe"))
    img = pe.get_memory_mapped_image()
    for arg in sys.argv[1:]:
        va = int(arg, 16)
        data = img[va - pe.OPTIONAL_HEADER.ImageBase:].split(b"\0")[0]
        print(f"// {data.decode('cp932', 'replace').strip()}")
        print(c_literal(data))


if __name__ == "__main__":
    main()
