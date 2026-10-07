#!/usr/bin/env python3
"""Lists or extracts files from th16.dat (the game's archive), for tests.

    th16dat.py <th16.dat>                      list the entries
    th16dat.py <th16.dat> <out dir> [names..]  extract (all, or the names)

A Python copy of the game's reader: Arcfile.cpp (directory and entry keys),
Crypt.cpp (zun_decrypt) and Lzss.cpp (lzss_decompress). It only reads the
archive.
"""
import os
import struct
import sys

# Arcfile.cpp g_arcfile_keys: key, step, block, limit.
KEYS = [
    (0x1B, 0x73, 0x100, 0x3800), (0x12, 0x43, 0x200, 0x3E00), (0x35, 0x79, 0x400, 0x3C00),
    (0x03, 0x91, 0x80, 0x6400), (0xAB, 0xDC, 0x80, 0x7000), (0x51, 0x9E, 0x100, 0x4000),
    (0xC1, 0x15, 0x400, 0x2C00), (0x99, 0x7D, 0x80, 0x4400),
]


def decrypt(data, key, step, block, limit):
    size = len(data)
    out = bytearray(data)
    tail = size % block
    if tail >= block // 4:
        tail = 0
    remaining = (size & ~1) - tail
    src = 0
    pos = 0
    while remaining > 0 and limit > 0:
        if remaining < block:
            block = remaining
        p = pos + block - 1
        for _ in range((block + 1) // 2):
            out[p] = data[src] ^ key
            src += 1
            p -= 2
            key = (key + step) & 0xFF
        p = pos + block - 2
        for _ in range(block // 2):
            out[p] = data[src] ^ key
            src += 1
            p -= 2
            key = (key + step) & 0xFF
        pos += block
        remaining -= block
        limit -= block
    return bytes(out)


class BitReader:
    """MSB-first bits; past the end the reader sees zeros (as Lzss.cpp)."""

    def __init__(self, data):
        self.data = data
        self.pos = 0

    def read(self, n):
        value = 0
        for _ in range(n):
            byte = self.pos >> 3
            bit = 0
            if byte < len(self.data):
                bit = (self.data[byte] >> (7 - (self.pos & 7))) & 1
            value = (value << 1) | bit
            self.pos += 1
        return value


def lzss_decompress(data, out_size):
    window = bytearray(0x2000)
    out = bytearray()
    bits = BitReader(data)
    cur = 1
    while len(out) < out_size:
        if bits.read(1):
            c = bits.read(8)
            window[cur] = c
            out.append(c)
            cur = (cur + 1) & 0x1FFF
        else:
            match = bits.read(13)
            if match == 0:
                break
            length = bits.read(4) + 2
            for i in range(length + 1):
                c = window[(match + i) & 0x1FFF]
                window[cur] = c
                out.append(c)
                cur = (cur + 1) & 0x1FFF
    return bytes(out)


def read_directory(f):
    f.seek(0, 2)
    file_size = f.tell()
    f.seek(0)
    header = decrypt(f.read(16), 0x1B, 0x37, 16, 16)
    magic, unpacked, packed, count = struct.unpack("<4sIII", header)
    if magic != b"THA1":
        raise ValueError("not a TH16 archive")
    unpacked = (unpacked - 123456789) & 0xFFFFFFFF
    packed = (packed - 987654321) & 0xFFFFFFFF
    count = (count - 135792468) & 0xFFFFFFFF
    dir_offset = file_size - packed
    f.seek(dir_offset)
    table = lzss_decompress(decrypt(f.read(packed), 0x3E, 0x9B, 0x80, packed), unpacked)
    entries = []
    p = 0
    for _ in range(count):
        end = table.index(b"\0", p)
        name = table[p:end].decode("ascii")
        length = end - p + 1
        length += -length % 4
        p += length
        offset, size, _ = struct.unpack_from("<III", table, p)
        p += 12
        entries.append([name, offset, size])
    for i, entry in enumerate(entries):
        next_offset = entries[i + 1][1] if i + 1 < count else dir_offset
        entry.append(next_offset - entry[1])
    return entries


def read_entry(f, entry):
    name, offset, size, packed_size = entry
    f.seek(offset)
    data = f.read(packed_size)
    key, step, block, limit = KEYS[sum(name.encode("ascii")) & 7]
    data = decrypt(data, key, step, block, limit)
    if packed_size != size:
        data = lzss_decompress(data, size)
    return data[:size]


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    with open(sys.argv[1], "rb") as f:
        entries = read_directory(f)
        if len(sys.argv) == 2:
            for name, offset, size, packed_size in entries:
                print(f"{name:32} {offset:10} {size:10} {packed_size:10}")
            return 0
        out_dir = sys.argv[2]
        os.makedirs(out_dir, exist_ok=True)
        wanted = {n.lower() for n in sys.argv[3:]}
        for entry in entries:
            if wanted and entry[0].lower() not in wanted:
                continue
            with open(os.path.join(out_dir, entry[0]), "wb") as out:
                out.write(read_entry(f, entry))
    return 0


if __name__ == "__main__":
    sys.exit(main())
