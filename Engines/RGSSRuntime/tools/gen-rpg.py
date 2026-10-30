# Reads the RGSS help's RPG pages (as text) and prints each data class: superclass, attributes and initial values.
import sys, re, pathlib
for p in sorted(pathlib.Path(sys.argv[1]).glob('gc_rpg_*.txt')):
    lines = p.read_text().splitlines()
    try:
        d = next(i for i, l in enumerate(lines) if l.strip() in ('Definition', '定義'))
    except StopIteration:
        continue
    stack = []; cls = sup = None; attrs = []; inits = []; in_init = False
    for l in lines[d+1:]:
        s = l.strip(); indent = len(l) - len(l.lstrip())
        m = re.match(r'^(module|class)\s+([\w:]+)(?:\s*<\s*(\S+))?', s)
        if m and not in_init:
            while stack and stack[-1][0] >= indent: stack.pop()
            stack.append((indent, m.group(2), m.group(3)))
            continue
        if re.match(r'^def initialize\b', s):
            in_init = True
            names = [n for _, n, _ in stack]
            q = '::'.join(names)
            if not q.startswith('RPG'): q = 'RPG::' + q
            cls = q.replace('RPG::RPG', 'RPG'); sup = stack[-1][2] if stack else None
            if sup and not sup.startswith(('RPG', 'Object')) and len(stack) > 1: sup = 'RPG::' + sup
            continue
        if in_init:
            if s == 'end': in_init = False; continue
            mm = re.match(r'^@(\w+)\s*=\s*(.+)$', s)
            if mm: inits.append((mm.group(1), mm.group(2)))
            continue
        for kind in ('attr_accessor', 'attr_reader'):
            for a in re.findall(kind + r'\s+(.*)', s):
                attrs += [x.strip().lstrip(':') for x in a.split(',')]
    if cls:
        print(f"{cls}|{sup or ''}|{','.join(attrs)}|{' ;; '.join(f'{k}={v}' for k,v in inits)}")
