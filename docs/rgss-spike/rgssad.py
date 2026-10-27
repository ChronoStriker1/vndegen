"""Reads RGSS archives (Game.rgssad and Game.rgss2a are version 1, Game.rgss3a is version 3).
   python3 -I rgssad.py list <archive>
   python3 -I rgssad.py extract <archive> <name> <out file>
   python3 -I rgssad.py scripts <archive or Scripts file> <out dir>   writes each game script as a .rb file"""
import os
import struct
import sys
import zlib

MASK = 0xFFFFFFFF


def step(k):
    return (k * 7 + 3) & MASK


def entries_v1(f):
    key = 0xDEADCAFE
    while True:
        raw = f.read(4)
        if len(raw) < 4:
            return
        name_len = struct.unpack("<I", raw)[0] ^ key
        key = step(key)
        name = bytearray(f.read(name_len))
        for i in range(name_len):
            name[i] ^= key & 0xFF
            key = step(key)
        size = struct.unpack("<I", f.read(4))[0] ^ key
        key = step(key)
        yield bytes(name), f.tell(), size, key
        f.seek(size, 1)


def entries_v3(f):
    key = (struct.unpack("<I", f.read(4))[0] * 9 + 3) & MASK
    kb = struct.pack("<I", key)
    while True:
        offset, size, filekey, name_len = (v ^ key for v in struct.unpack("<IIII", f.read(16)))
        if offset == 0:
            return
        name = bytes(b ^ kb[i % 4] for i, b in enumerate(f.read(name_len)))
        yield name, offset, size, filekey


def entries(f):
    magic = f.read(8)
    if magic[:7] != b"RGSSAD\0":
        sys.exit("not an RGSS archive")
    for name, offset, size, key in (entries_v3 if magic[7] == 3 else entries_v1)(f):
        yield name.decode("utf-8", "replace").replace("\\", "/"), offset, size, key


def decrypt(f, offset, size, key):
    f.seek(offset)
    data = bytearray(f.read(size))
    padded = data + b"\0" * (-len(data) % 4)
    words = list(struct.unpack(f"<{len(padded) // 4}I", padded))
    for i in range(len(words)):
        words[i] ^= key
        key = step(key)
    return bytes(struct.pack(f"<{len(words)}I", *words))[:size]


def read_member(archive, wanted):
    with open(archive, "rb") as f:
        for name, offset, size, key in entries(f):
            if name.lower() == wanted.lower():
                return decrypt(f, offset, size, key)
    sys.exit(f"{wanted} not in archive")


class Marshal:
    """Just enough of Ruby's Marshal format to read Scripts.rxdata, .rvdata and .rvdata2."""

    def __init__(self, data):
        self.d, self.i, self.syms = data, 2, []

    def byte(self):
        b = self.d[self.i]
        self.i += 1
        return b

    def int(self):
        c = struct.unpack("b", bytes([self.byte()]))[0]
        if c == 0:
            return 0
        if 4 < c < 128:
            return c - 5
        if -129 < c < -4:
            return c + 5
        n = 0
        for k in range(abs(c)):
            n |= self.byte() << (8 * k)
        return n - (1 << (8 * abs(c))) if c < 0 else n

    def bytes_(self):
        n = self.int()
        b = self.d[self.i:self.i + n]
        self.i += n
        return b

    def load(self):
        t = chr(self.byte())
        if t == "[":
            return [self.load() for _ in range(self.int())]
        if t == "i":
            return self.int()
        if t == '"':
            return self.bytes_()
        if t == "I":
            v = self.load()
            for _ in range(self.int()):
                self.load()
                self.load()
            return v
        if t == ":":
            s = self.bytes_()
            self.syms.append(s)
            return s
        if t == ";":
            return self.syms[self.int()]
        if t in "0TF":
            return None
        sys.exit(f"unsupported Marshal type {t!r} at {self.i}")


def main():
    cmd, src = sys.argv[1], sys.argv[2]
    if cmd == "list":
        with open(src, "rb") as f:
            for name, _, size, _ in entries(f):
                print(f"{size}\t{name}")
    elif cmd == "extract":
        with open(sys.argv[4], "wb") as out:
            out.write(read_member(src, sys.argv[3]))
    elif cmd == "scripts":
        if src.lower().endswith((".rgssad", ".rgss2a", ".rgss3a")):
            with open(src, "rb") as f:
                names = [n for n, *_ in entries(f) if n.lower().startswith("data/scripts.")]
            data = read_member(src, names[0])
        else:
            data = open(src, "rb").read()
        os.makedirs(sys.argv[3], exist_ok=True)
        for n, (_, title, code) in enumerate(Marshal(data).load()):
            name = title.decode("utf-8", "replace").replace("/", "_")
            with open(os.path.join(sys.argv[3], f"{n:03d} {name}.rb"), "wb") as out:
                out.write(zlib.decompress(code))
    else:
        sys.exit(__doc__)


main()
