#!/usr/bin/env python3
"""Command usage by script kind across all games.

usage: opcodes.py GAMES_ROOT > table.tsv
Kind is the pak or folder a script comes from (scr, flow, gt, sys_N ...).
"""
import collections, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import advscript, nekopak


def scripts(root):
    for dp, dn, fn in os.walk(root):
        for f in fn:
            p = os.path.join(dp, f)
            if f.lower().endswith('.pak'):
                try:
                    m, mode, ents = nekopak.open_pak(p)
                except ValueError:
                    continue
                for n, o, s in ents:
                    if n.lower().endswith('.txt'):
                        d = nekopak.read_entry(m, o, s)
                        if d.startswith(advscript.MAGIC):
                            yield f[:-4].lower(), n, d
            elif f.lower().endswith('.txt'):
                with open(p, 'rb') as fh:
                    d = fh.read()
                if d.startswith(advscript.MAGIC):
                    yield os.path.basename(dp).lower() + '(loose)', f, d


def kind_of(src, name):
    n = name.lower().replace('\\', '/')
    if src.startswith('flow') or n.startswith('flow') or n.startswith('pv'):
        return 'flow'
    if src.startswith('gt'):
        return 'gt'
    return 'scene'


def main():
    root = sys.argv[1]
    use = collections.defaultdict(lambda: [0, set(), collections.Counter()])
    totals = collections.Counter()
    for slug in sorted(os.listdir(root)):
        for src, name, d in scripts(os.path.join(root, slug)):
            nodes, _ = advscript.parse(d)
            k = kind_of(src, name)
            totals[k] += 1
            for n in nodes:
                u = use[(k, n.opcode)]
                u[0] += 1
                u[1].add(slug)
                u[2][advscript.cmd_name(n.strs[0])] += 1
    print('# scripts by kind', dict(totals))
    print('kind\topcode\tnodes\tgames\tname')
    for (k, op), (c, games, names) in sorted(use.items()):
        print('%s\t%d\t%d\t%d\t%s' % (k, op, c, len(games),
              ' / '.join(n for n, _ in names.most_common(3))))


if __name__ == '__main__':
    main()
