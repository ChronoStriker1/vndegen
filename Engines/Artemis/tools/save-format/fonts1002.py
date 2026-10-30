# fonts1002.py <save.dat>...: prints each message layer's font object (field 25) of a version 1002 save.
import struct, zlib, sys
for path in sys.argv[1:]:
    d = open(path, 'rb').read(); b = zlib.decompress(d[12:]); L = len(b)
    A = struct.unpack_from('<I', b, L - 4)[0]; n = struct.unpack_from('<I', b, A)[0]; p = A + 4; T = {}
    for _ in range(n):
        k, c = struct.unpack_from('<2I', b, p); T[k] = dict(struct.unpack_from('<2I', b, p + 8 + 8*i) for i in range(c)); p += 8 + 8*c
    root = T[0]; o = root[25]; cnt = struct.unpack_from('<I', b, o)[0]; o += 4
    for _ in range(cnt):
        kl = struct.unpack_from('<I', b, o)[0]; key = b[o+4:o+4+kl].decode('cp932'); o += 4 + kl + 4
        f = T[o]; f32 = lambda x: struct.unpack_from('<f', b, f[x])[0]; i32 = lambda x: struct.unpack_from('<i', b, f[x])[0]
        face = b[f[1]+4:f[1]+4+struct.unpack_from('<I', b, f[1])[0]].decode('cp932')
        print(path.split('/')[-1], key, 'pos', [f32(x) for x in (3, 4, 5, 6)], 'size', b[f[20]], 'space', [f32(x) for x in (22, 23, 24)], 'color', hex(i32(25)), 'style', i32(29), 'face', face)
        o = max(f.values()) + 1  # last field is a byte
