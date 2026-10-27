#!/usr/bin/env python3
"""NEKOSDK_ADVSCRIPT2 parser. Node layout from FuckGalEngine's
nekosdk_advscript2.ksy, checked against real files.

usage: advscript.py dump FILE
       advscript.py stats FILE...   (opcode and command-name histogram)
"""
import collections, struct, sys

MAGIC = b'NEKOSDK_ADVSCRIPT2\0'


class Node:
    __slots__ = ('id', 'type1', 'some_ofs', 'opcode', 'pre', 'next_id',
                 'post', 'strs')


def parse(data):
    if not data.startswith(MAGIC):
        raise ValueError('bad magic %r' % data[:20])
    pos = len(MAGIC)
    count = struct.unpack_from('<I', data, pos)[0]
    pos += 4
    nodes = []
    for _ in range(count):
        n = Node()
        n.id, n.type1, n.some_ofs, n.opcode = struct.unpack_from('<4I', data, pos)
        pos += 16
        n.pre = data[pos:pos + 0x80]
        pos += 0x80
        n.next_id = struct.unpack_from('<I', data, pos)[0]
        pos += 4
        n.post = data[pos:pos + 0x40]
        pos += 0x40
        strs = []
        # A fan translation of Test Game 94 breaks the length prefixes both
        # ways: three scripts have prefixes one byte short, and two have NULs
        # inside correctly counted text. Trust the length unless its span
        # doesn't end in a NUL, then extend to the next NUL.
        for _ in range(33):
            ln = struct.unpack_from('<I', data, pos)[0]
            pos += 4
            end = pos + ln
            if ln and data[end - 1] != 0:
                end = data.index(b'\0', end) + 1
            strs.append(data[pos:end].replace(b'\0', b'').decode('cp932', 'replace'))
            pos = end
        n.strs = strs
        nodes.append(n)
    return nodes, len(data) - pos


def words(b):
    return [w for w in struct.unpack('<%dI' % (len(b) // 4), b)]


def nz(b):
    return [(i, w) for i, w in enumerate(words(b)) if w]


def cmd_name(s):
    s = s.strip()
    if s.startswith('['):
        return s[:s.find(']') + 1] if ']' in s else s
    return '(text)' if s else '(empty)'


def main():
    cmd = sys.argv[1]
    if cmd == 'dump':
        data = open(sys.argv[2], 'rb').read()
        nodes, rest = parse(data)
        print('nodes', len(nodes), 'trailing', rest)
        for n in nodes:
            print('#%d t=%d ofs=%d op=%d next=%d pre=%s post=%s' % (
                n.id, n.type1, n.some_ofs, n.opcode, n.next_id, nz(n.pre), nz(n.post)))
            for i, s in enumerate(n.strs):
                if s:
                    print('  s%d: %s' % (i, s.replace('\r\n', '\\n').replace('\n', '\\n')))
    elif cmd == 'stats':
        ops = collections.Counter()
        names = collections.defaultdict(collections.Counter)
        bad = 0
        for f in sys.argv[2:]:
            try:
                nodes, rest = parse(open(f, 'rb').read())
            except Exception as e:
                print('FAIL', f, e)
                bad += 1
                continue
            if rest:
                print('TRAILING', f, rest)
            for n in nodes:
                ops[(n.type1, n.opcode)] += 1
                names[(n.type1, n.opcode)][cmd_name(n.strs[0])] += 1
        for k, c in sorted(ops.items()):
            print('type=%d op=%3d %7d  %s' % (k[0], k[1], c,
                  ', '.join('%s x%d' % kv for kv in names[k].most_common(4))))
        print('files', len(sys.argv) - 2, 'failed', bad)


if __name__ == '__main__':
    main()
