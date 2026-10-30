import struct, sys
def sections(d):
    magic = d[:4]; ver, n = struct.unpack_from('<2I', d, 4)
    ents = [struct.unpack_from('<2I', d, 12 + 8*i) for i in range(n)]
    starts = sorted(o + 8 for _, o in ents)
    out = {}
    for tag, off in ents:
        a = off + 8
        b = min([s for s in starts if s > a] + [len(d)])
        out[tag] = d[a:b]
    return magic, ver, [t for t, _ in ents], out
if __name__ == '__main__':
    d = open(sys.argv[1], 'rb').read()
    magic, ver, order, secs = sections(d)
    print(magic, ver, 'order', order, 'table end', 12 + 8*len(order))
    for t in sorted(secs):
        print(t, len(secs[t]), secs[t][:48].hex())
