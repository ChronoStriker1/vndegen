# frames1002.py <save.dat>...: prints a version 1002 save's position and call frames, with the tag each frame records.
import struct, zlib, sys
for path in sys.argv[1:]:
    d = open(path, 'rb').read(); b = zlib.decompress(d[12:]); L = len(b)
    A = struct.unpack_from('<I', b, L - 4)[0]; n = struct.unpack_from('<I', b, A)[0]; p = A + 4; T = {}
    for _ in range(n):
        k, c = struct.unpack_from('<2I', b, p); T[k] = dict(struct.unpack_from('<2I', b, p + 8 + 8*i) for i in range(c)); p += 8 + 8*c
    s = lambda o: b[o + 4:o + 4 + struct.unpack_from('<I', b, o)[0]].decode('cp932')
    i32 = lambda o: struct.unpack_from('<i', b, o)[0]
    root = T[0]
    objs = sorted(k for k, f in T.items() if root[3] <= k < root[6] and set(f) >= {1, 2, 3, 4, 6, 8})
    for k in objs:
        f = T[k]; t = T[f[8]]
        print(path, 'frame' if k != root[3] else 'position', s(f[3]), i32(f[4]), 'wait', i32(f[1]), 'tag', s(t[3]) or '-', 'line', i32(t[2]))
