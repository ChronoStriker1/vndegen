import struct, sys
def u32(b, o): return struct.unpack_from('<I', b, o)[0]
def as_object(b):
    """b = an object's bytes *after* its size field. Returns {fid: bytes} or None."""
    if len(b) < 4: return None
    n = u32(b, 0)
    if n == 0 or n > 200 or 4 + 8*n > len(b): return None
    tab = [struct.unpack_from('<2I', b, 4 + 8*i) for i in range(n)]
    offs = [o for _, o in tab]
    start = 4 + 8*n
    if any(o < start or o > len(b) for o in offs): return None
    if sorted(offs) != sorted(offs, reverse=False) or len(set(f for f, _ in tab)) != n: return None
    s = sorted(offs)
    if s[0] != start: return None
    out = {}
    for f, o in tab:
        e = min([x for x in s if x > o] + [len(b)])
        out[f] = b[o:e]
    return out
def sized_object(b):
    if len(b) >= 8 and u32(b, 0) == len(b) - 4:
        return as_object(b[4:])
    return None
def text(b):
    if len(b) >= 4 and u32(b, 0) == len(b) - 4:
        try: return b[4:].decode('cp932')
        except: return None
    return None
def show(b, ind=0, path=''):
    p = '  ' * ind
    o = sized_object(b)
    if o is not None and sorted(o) == [2, 3, 4] and text(o[3]) is not None and as_list(o[4]) is not None:
        lst = as_list(o[4]); line = struct.unpack('<i', o[2])[0] if len(o[2]) == 4 else o[2]
        print(f"{p}{path} TAG [{text(o[3])} " + " ".join(f"{k}={v!r}" for k, v in lst) + f"] line={line}")
        return
    if o is not None:
        print(f"{p}{path} object({len(b)}) fields {sorted(o)}")
        for f in sorted(o): show(o[f], ind+1, f"{path}.{f}")
        return
    t = text(b)
    if t is not None and t.isprintable():
        print(f"{p}{path} str {t!r}"); return
    if len(b) == 4: print(f"{p}{path} i32 {struct.unpack('<i', b)[0]}"); return
    if len(b) == 1: print(f"{p}{path} u8 {b[0]}"); return
    if len(b) == 8: print(f"{p}{path} 8b {b.hex()} f64={struct.unpack('<d', b)[0]:.6g}"); return
    kt = as_keyed_tags(b)
    if kt is not None:
        print(f"{p}{path} keyed-tags[{len(kt)}]")
        for k, tags in kt:
            print(f"{p}  [{k!r}]")
            for t in tags: show(t, ind+2, "")
        return
    objs = as_object_list(b)
    if objs is not None:
        print(f"{p}{path} objects[{len(objs)}]")
        for i, v in enumerate(objs): show(v, ind+1, f"{path}[{i}]")
        return
    lst = as_list(b)
    if lst is not None:
        print(f"{p}{path} list[{len(lst)}]")
        for i, (k, v) in enumerate(lst):
            if isinstance(v, bytes):
                show(v, ind+1, f"{path}[{k!r}]")
            else:
                print(f"{p}  {path}[{k!r}] = {v!r}")
        return
    print(f"{p}{path} raw({len(b)}) {b[:48].hex()}")
def read_str(b, o):
    if o + 4 > len(b): return None, o
    n = u32(b, o)
    if n > 1_000_000 or o + 4 + n > len(b): return None, o
    raw = b[o+4:o+4+n]
    if b'\0' in raw: return f'<bin {n}>', o + 4 + n
    try: return raw.decode('cp932'), o + 4 + n
    except: return f'<undecodable {raw[:16].hex()}>', o + 4 + n
def as_list(b):
    """count, then entries of (string key, string value) or (string key, sized object)."""
    if len(b) < 4: return None
    n = u32(b, 0); o = 4; out = []
    if n == 0 and len(b) == 4: return []
    if n > 100000: return None
    for _ in range(n):
        k, o2 = read_str(b, o)
        if k is None: return None
        o = o2
        if o + 4 <= len(b) and u32(b, o) >= 8 and o + 4 + u32(b, o) <= len(b) and sized_object(b[o:o+4+u32(b, o)]) is not None:
            sz = u32(b, o); out.append((k, b[o:o+4+sz])); o += 4 + sz; continue
        v, o2 = read_str(b, o)
        if v is None: return None
        out.append((k, v)); o = o2
    return out if o == len(b) else None
def as_object_list(b):
    if len(b) < 8: return None
    n = u32(b, 0); o = 4; out = []
    if n == 0 or n > 10000: return None
    for _ in range(n):
        if o + 4 > len(b): return None
        sz = u32(b, o)
        if o + 4 + sz > len(b) or sized_object(b[o:o+4+sz]) is None: return None
        out.append(b[o:o+4+sz]); o += 4 + sz
    return out if o == len(b) else None

def as_keyed_tags(b):
    if len(b) < 8: return None
    n = u32(b, 0); o = 4; out = []
    if n == 0 or n > 100000: return None
    for _ in range(n):
        k, o2 = read_str(b, o)
        if k is None or o2 + 4 > len(b): return None
        o = o2; m = u32(b, o); o += 4
        if m > 10000: return None
        tags = []
        for _ in range(m):
            if o + 4 > len(b): return None
            sz = u32(b, o)
            if o + 4 + sz > len(b) or sized_object(b[o:o+4+sz]) is None: return None
            tags.append(b[o:o+4+sz]); o += 4 + sz
        out.append((k, tags))
    return out if o == len(b) else None

if __name__ == '__main__':
    from bow import sections
    d = open(sys.argv[1], 'rb').read()
    root = as_object(d[8:])
    secs = [int(x) for x in sys.argv[2:]] or sorted(root)
    for f in secs: show(root[f], 0, f"#{f}")
