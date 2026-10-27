#!/usr/bin/env python3
"""NEKOPACK4 reader. Layout from GARbro ArcFormats/NekoSDK/ArcPAK.cs (MIT).

usage: nekopak.py list PAK
       nekopak.py extract PAK OUTDIR [GLOB]
"""
import fnmatch, mmap, os, struct, sys, zlib


def open_pak(path):
    f = open(path, 'rb')
    m = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
    if m[:9] != b'NEKOPACK4':
        raise ValueError('not NEKOPACK4: %r' % m[:10])
    mode = m[9]
    if mode == ord('A'):
        pos = 0xE
        end = pos + struct.unpack_from('<I', m, 0xA)[0]
    elif mode == ord('S'):
        pos = 0xA
        end = pos + struct.unpack_from('<I', m, 0xA)[0] + 12
    else:
        raise ValueError('unknown mode %r' % chr(mode))
    entries = []
    while pos < end:
        n = struct.unpack_from('<I', m, pos)[0]
        pos += 4
        if n == 0:
            break
        raw = m[pos:pos + n]
        pos += n
        key = sum(b - 256 if b > 127 else b for b in raw) & 0xFFFFFFFF
        off, size = struct.unpack_from('<II', m, pos)
        pos += 8
        name = raw.split(b'\0', 1)[0].decode('cp932')
        entries.append((name, off ^ key, size ^ key))
    return m, chr(mode), entries


def read_entry(m, off, size):
    key = (size // 8 + 0x22) & 0xFFFFFFFF
    head = bytearray(m[off:off + min(4, size)])
    for i in range(len(head)):
        head[i] ^= key & 0xFF
        key = (key << 3) & 0xFFFFFFFF
    if size < 8:
        return bytes(head)
    expect = struct.unpack_from('<I', m, off + size - 4)[0]
    data = zlib.decompress(bytes(head) + m[off + 4:off + size - 4])
    if len(data) != expect:
        raise ValueError('size mismatch %d != %d' % (len(data), expect))
    return data


def main():
    cmd, path = sys.argv[1], sys.argv[2]
    m, mode, entries = open_pak(path)
    if cmd == 'list':
        print('mode', mode, 'entries', len(entries))
        for name, off, size in entries:
            print('%10d %10d %s' % (off, size, name))
    elif cmd == 'extract':
        out = sys.argv[3]
        pat = sys.argv[4] if len(sys.argv) > 4 else '*'
        for name, off, size in entries:
            if not fnmatch.fnmatch(name.lower(), pat.lower()):
                continue
            rel = name.replace('\\', '/')
            dst = os.path.join(out, rel)
            os.makedirs(os.path.dirname(dst) or out, exist_ok=True)
            with open(dst, 'wb') as fo:
                fo.write(read_entry(m, off, size))


if __name__ == '__main__':
    main()
