# splice.py <base.dat> <donor.dat> <out.dat> field...: a BOWS save with the given fields taken from the donor.
import struct, sys
from tree import as_object
def table(d):
    n = struct.unpack_from('<I', d, 8)[0]
    return [struct.unpack_from('<I', d, 12 + 8*i)[0] for i in range(n)]
base = open(sys.argv[1], 'rb').read(); donor = open(sys.argv[2], 'rb').read()
fb = as_object(base[8:]); fd = as_object(donor[8:])
for f in map(int, sys.argv[4:]): fb[f] = fd[f]
order = table(base); start = 4 + 8*len(order); offs = {}; data = b''
for f in sorted(fb):
    offs[f] = start + len(data); data += fb[f]
body = struct.pack('<I', len(order)) + b''.join(struct.pack('<2I', f, offs[f]) for f in order) + data
open(sys.argv[3], 'wb').write(base[:8] + body)
