#!/usr/bin/env python3
"""Survey every NekoSDK game folder: pak modes, entry extensions, content magic,
loose files, and script command usage.

usage: survey.py GAMES_ROOT OUT_DIR
"""
import collections, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import advscript, nekopak


def magic(b):
    for sig, name in ((b'BM', 'bmp'), (b'\x89PNG', 'png'), (b'RIFF', 'riff'),
                      (b'OggS', 'ogg'), (b'\xff\xd8', 'jpg'),
                      (b'NEKOSDK_ADVSCRIPT2', 'advscript2'), (b'NEKO', 'neko?')):
        if b.startswith(sig):
            if name == 'riff':
                return 'riff:' + b[8:12].decode('latin1')
            return name
    return 'raw:' + b[:8].hex()


def survey_game(root):
    g = {'paks': {}, 'loose': collections.Counter(), 'scripts': 0, 'script_fail': 0,
         'ops': collections.Counter(), 'opnames': {}}
    script_blobs = []
    for dp, dn, fn in os.walk(root):
        for f in fn:
            p = os.path.join(dp, f)
            rel = os.path.relpath(p, root)
            ext = os.path.splitext(f)[1].lower()
            if ext != '.pak':
                g['loose'][ext or '(none)'] += 1
                if ext == '.txt':
                    with open(p, 'rb') as fh:
                        if fh.read(18) == b'NEKOSDK_ADVSCRIPT':
                            script_blobs.append((rel, open(p, 'rb').read()))
                continue
            try:
                m, mode, entries = nekopak.open_pak(p)
            except Exception as e:
                g['paks'][rel] = {'error': str(e)}
                continue
            exts = collections.Counter()
            magics = collections.Counter()
            seen = set()
            for name, off, size in entries:
                e = os.path.splitext(name)[1].lower() or '(none)'
                exts[e] += 1
                if e not in seen or e == '.txt':
                    seen.add(e)
                    try:
                        data = nekopak.read_entry(m, off, size)
                        magics[e + '=' + magic(data[:32])] += 1
                        if data.startswith(b'NEKOSDK_ADVSCRIPT'):
                            script_blobs.append((rel + ':' + name, data))
                    except Exception as ex:
                        magics[e + '=ERR ' + str(ex)[:40]] += 1
            dirs = collections.Counter(n.replace('\\', '/').split('/')[0] if '\\' in n or '/' in n else '.' for n, _, _ in entries)
            g['paks'][rel] = {'mode': mode, 'count': len(entries), 'exts': dict(exts),
                              'magic': dict(magics), 'dirs': dict(dirs.most_common(5))}
    for rel, data in script_blobs:
        try:
            nodes, rest = advscript.parse(data)
        except Exception:
            g['script_fail'] += 1
            continue
        if rest:
            g['script_fail'] += 1
        g['scripts'] += 1
        for n in nodes:
            g['ops'][n.opcode] += 1
            g['opnames'].setdefault(n.opcode, advscript.cmd_name(n.strs[0]))
    g['loose'] = dict(g['loose'])
    g['ops'] = dict(sorted(g['ops'].items()))
    return g


def main():
    root, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    for slug in sorted(os.listdir(root)):
        res = survey_game(os.path.join(root, slug))
        with open(os.path.join(out, slug + '.json'), 'w') as fh:
            json.dump(res, fh, ensure_ascii=False, indent=1)
        print(slug, 'paks', len(res['paks']), 'scripts', res['scripts'],
              'fail', res['script_fail'], 'ops', len(res['ops']), flush=True)


if __name__ == '__main__':
    main()
