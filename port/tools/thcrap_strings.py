#!/usr/bin/env python3
"""Writes the table of the game's strings at their original addresses, for
the port's thcrap support (port/src/thcrap/strings.cpp).

thcrap's stringlocs.js names strings by their address in the original
th16.exe ("Rx9290c": "th10_ascii_stage_1"); the port's strings are its own
literals at addresses of its own. This script finds every string literal of
the game sources (src/) in the original executable's data sections, where
the matching decompilation put them byte for byte, and writes their
addresses and texts as C++ initializers:

    {0x0049290c, "Stage 1"},

    thcrap_strings.py <th16.exe> <src dir> <output .inc>

The build (port/CMakeLists.txt) runs it when the original executable is
there (orig/th16.exe, or TH16_ORIG_EXE); otherwise it uses the copy in
port/src/thcrap/th16_strings.inc, which this script also writes.
"""
import os
import re
import struct
import sys

SIMPLE_ESCAPES = {
    "n": 0x0A, "t": 0x09, "r": 0x0D, "0": 0x00, "a": 0x07, "b": 0x08, "f": 0x0C, "v": 0x0B,
    "\\": 0x5C, '"': 0x22, "'": 0x27, "?": 0x3F,
}


def sections(exe):
    """(virtual address, raw bytes) of the initialized data sections."""
    pe = struct.unpack_from("<I", exe, 0x3C)[0]
    count = struct.unpack_from("<H", exe, pe + 6)[0]
    opt_size = struct.unpack_from("<H", exe, pe + 20)[0]
    image_base = struct.unpack_from("<I", exe, pe + 24 + 28)[0]
    out = []
    for i in range(count):
        o = pe + 24 + opt_size + i * 40
        name, vsize, va, rsize, raw = struct.unpack_from("<8sIIII", exe, o)
        name = name.rstrip(b"\0").decode("ascii", "replace")
        if name in (".rdata", ".data"):
            out.append((image_base + va, exe[raw:raw + min(vsize, rsize)]))
    return out


def literals(text):
    """The string literals of a C++ source, adjacent ones joined, as bytes."""
    out = []
    i = 0
    n = len(text)
    current = None
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
            continue
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
            continue
        if c == "#" and (i == 0 or text[i - 1] == "\n" or text[:i].rstrip(" \t").endswith("\n")):
            line_end = text.find("\n", i)
            line = text[i:line_end if line_end >= 0 else n]
            if re.match(r"#\s*include", line):
                if current is not None:
                    out.append(current)
                    current = None
                i = n if line_end < 0 else line_end
                continue
        if c == "'":
            # A character literal (also multi-character ones).
            j = i + 1
            while j < n and text[j] != "'":
                j += 2 if text[j] == "\\" else 1
            i = j + 1
            if current is not None:
                out.append(current)
                current = None
            continue
        if c == '"':
            prefix = text[max(0, i - 2):i]
            wide = prefix.endswith(("L", "u", "U")) and not prefix.endswith("u8")
            j = i + 1
            data = bytearray()
            while j < n and text[j] != '"':
                if text[j] == "\\":
                    e = text[j + 1]
                    if e == "x":
                        k = j + 2
                        while k < n and text[k] in "0123456789abcdefABCDEF":
                            k += 1
                        data.append(int(text[j + 2:k], 16) & 0xFF)
                        j = k
                    elif e in "01234567":
                        k = j + 1
                        while k < n and k < j + 4 and text[k] in "01234567":
                            k += 1
                        data.append(int(text[j + 1:k], 8) & 0xFF)
                        j = k
                    elif e == "\n":
                        j += 2
                    else:
                        data.append(SIMPLE_ESCAPES.get(e, ord(e)))
                        j += 2
                else:
                    data += text[j].encode("utf-8")
                    j += 1
            i = j + 1
            if wide:
                if current is not None:
                    out.append(current)
                    current = None
                continue
            current = (current or b"") + bytes(data)
            continue
        if c in " \t\r\n":
            i += 1
            continue
        if current is not None:
            out.append(current)
            current = None
        i += 1
    if current is not None:
        out.append(current)
    return out


def c_literal(data):
    out = '"'
    prev_hex = False
    for b in data:
        ch = chr(b)
        if ch in '\\"':
            out += "\\" + ch
            prev_hex = False
        elif ch == "\n":
            out += "\\n"
            prev_hex = False
        elif ch == "\r":
            out += "\\r"
            prev_hex = False
        elif ch == "\t":
            out += "\\t"
            prev_hex = False
        elif 0x20 <= b < 0x7F:
            if prev_hex and ch in "0123456789abcdefABCDEF":
                out += '" "'
            # No trigraphs.
            if ch == "?" and out.endswith("?"):
                out += '" "'
            out += ch
            prev_hex = False
        else:
            out += "\\x%02x" % b
            prev_hex = True
    return out + '"'


def main():
    if len(sys.argv) != 4:
        sys.stderr.write(__doc__)
        return 2
    exe_path, src_dir, out_path = sys.argv[1:]
    with open(exe_path, "rb") as f:
        exe = f.read()
    found = {}
    strings = set()
    for root, _, files in os.walk(src_dir):
        for name in sorted(files):
            if name.endswith((".cpp", ".h")):
                with open(os.path.join(root, name), encoding="latin-1") as f:
                    strings.update(s for s in literals(f.read()) if s and b"\0" not in s)
    for va, data in sections(exe):
        for s in strings:
            needle = s + b"\0"
            start = 0
            while True:
                p = data.find(needle, start)
                if p < 0:
                    break
                # A string of its own: after another's terminator or padding,
                # or aligned (not the tail of a longer string).
                if p == 0 or data[p - 1] == 0 or p % 4 == 0:
                    found[va + p] = s
                start = p + 1
    lines = [
        "// Generated by port/tools/thcrap_strings.py from the game sources and the",
        "// original th16.exe: each string literal of src/ at its address there.",
        "// clang-format off",
    ]
    for address in sorted(found):
        lines.append("{0x%08x, %s}," % (address, c_literal(found[address])))
    lines.append("// clang-format on")
    text = "\n".join(lines) + "\n"
    old = None
    if os.path.exists(out_path):
        with open(out_path, encoding="latin-1") as f:
            old = f.read()
    if old != text:
        with open(out_path, "w", encoding="latin-1", newline="\n") as f:
            f.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
