# t1002.py <save.dat> [field...]: dumps a version 1002 save's root fields, with the tables of objects inside them.
import struct, sys, zlib
d = open(sys.argv[1], 'rb').read()
assert struct.unpack_from('<I', d, 4)[0] == 1002
b = zlib.decompress(d[12:]); L = len(b)
A = struct.unpack_from('<I', b, L - 4)[0]
n = struct.unpack_from('<I', b, A)[0]; p = A + 4; tables = {}
for _ in range(n):
    key, c = struct.unpack_from('<2I', b, p)
    tables.setdefault(key, []).append([struct.unpack_from('<2I', b, p + 8 + 8*i) for i in range(c)]); p += 8 + 8*c
root = tables[0][-1]
starts = sorted(o for _, o in root) + [A]
want = {int(x) for x in sys.argv[2:]}
for f, o in sorted(root, key=lambda x: x[1]):
    e = starts[starts.index(o) + 1]
    if want and f not in want: continue
    inner = sorted((k, [x for x, _ in t[0]]) for k, t in tables.items() if o <= k < e and k)
    print(f'#{f} [{o}:{e}] {e-o}b {b[o:min(e,o+40)].hex(" ")}')
    for k, ids in inner[:6]: print(f'    obj@{k} fields {ids}')
    if len(inner) > 6: print(f'    ... {len(inner)} objects')
