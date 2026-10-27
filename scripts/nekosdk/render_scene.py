#!/usr/bin/env python3
"""Feasibility prototype: run a NekoSDK game from cfg/init.ini's FirstFlow
through its flow script into the first scene, and write one PNG per text box.
Headless, no window and no sound. Sound commands are only logged.

usage: render_scene.py GAME_DIR OUT_DIR [FRAMES [SCENE_SCRIPT]]
"""
import io, os, re, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import advscript, nekopak
from PIL import Image, ImageChops, ImageDraw, ImageFont

W, H = 1920, 1080
POS_X = [0.5, 0.3, 0.7, 0.1, 0.9, 0.2, 0.8]
FONT = '/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc'


def s32(v):
    return v - 2**32 if v >= 2**31 else v


class VFS:
    """Game paths like bg\\x.bmp. Entries of bg.pak are bare names, so the pak
    name is the folder. sys_N paks and sys_N folders are patches and win."""

    def __init__(self, root):
        self.map = {}
        layers = []
        for dp, dn, fn in os.walk(root):
            for f in fn:
                p = os.path.join(dp, f)
                rel = os.path.relpath(p, root)
                parts = rel.split(os.sep)
                if f.lower().endswith('.pak'):
                    try:
                        m, mode, ents = nekopak.open_pak(p)
                    except ValueError:
                        continue
                    stem = f[:-4].lower()
                    prio = 2 if re.match(r'sys_\d+$', stem) or any(re.match(r'sys_\d+$', x.lower()) for x in parts[:-1]) else 0
                    for n, o, s in ents:
                        key = n.lower() if '\\' in n else stem + '\\' + n.lower()
                        layers.append((prio, key, ('pak', m, o, s)))
                else:
                    # Loose files: drop the top folder (the game's own folder)
                    # and any sys_N patch folder in front of the game path.
                    idx = 0
                    for i, x in enumerate(parts[:-1]):
                        if re.match(r'sys_\d+$', x.lower()) or x.lower() in ('scr', 'cfg', 'movie'):
                            idx = i
                            break
                    sub = parts[idx:]
                    prio = 1
                    if re.match(r'sys_\d+$', sub[0].lower()):
                        sub, prio = sub[1:], 3
                    layers.append((prio, '\\'.join(sub).lower(), ('file', p)))
        for prio, key, src in sorted(layers, key=lambda t: t[0]):
            self.map[key] = src

    def read(self, path):
        src = self.map.get(path.replace('/', '\\').lower())
        if src is None:
            return None
        if src[0] == 'file':
            return open(src[1], 'rb').read()
        return nekopak.read_entry(*src[1:])


class Sprite:
    def __init__(self, img):
        self.img, self.x, self.y, self.sx, self.sy, self.alpha = img, 0, 0, 1000, 1000, 256


class Game:
    def __init__(self, root, out, frames):
        self.vfs, self.out, self.left = VFS(root), out, frames
        self.ini = self.parse_ini(self.vfs.read('cfg\\init.ini'))
        self.sprites, self.shown, self.window = {}, {}, True
        self.log = open(os.path.join(out, 'run.log'), 'w')
        self.count = 0
        self.font = ImageFont.truetype(FONT, int(self.ini.get('MsgFontSize', 28)),
                                       layout_engine=ImageFont.Layout.BASIC)
        self.msgwin = self.image(self.ini['MsgWinFilename'])
        # The canvas is the window size, except that some games set a shorter
        # window than their art (1020 for 1080 art). The message window sits
        # on the bottom edge, so take the height from it.
        global W, H
        W = int(self.ini['WindowWidth'])
        H = max(int(self.ini['WindowHeight']), int(self.ini['MsgWinPosY']) + self.msgwin.height)

    @staticmethod
    def parse_ini(data):
        d = {}
        for line in data.decode('cp932').splitlines():
            line = line.split('//')[0].strip()
            if line and not line.startswith('#'):
                k, _, v = line.partition('\t')
                d[k] = v.strip()
        return d

    def image(self, path):
        data = self.vfs.read(path)
        if data is None:
            self.log.write('missing image %s\n' % path)
            return None
        base = None
        if data[:4] == b'RIFF' and data[8:12] == b'WEBP':
            data, base = self.split_webp(data, path)
        img = Image.open(io.BytesIO(data)).convert('RGBA')
        if base is not None or data[:4] == b'RIFF':
            # WebP sprites use magenta as a colour key. In a difference image,
            # magenta means keep the base pixel.
            key = self.magenta_mask(img)
            if base is not None:
                img = Image.composite(base, img, key)
            else:
                img.putalpha(Image.composite(Image.new('L', img.size, 0), img.getchannel('A'), key))
        alp = self.vfs.read(re.sub(r'\.\w+$', '.alp', path))
        if alp is not None:
            w, h = img.size
            stride = w if len(alp) == w * h else (w + 3) & ~3
            a = Image.frombytes('L', (w, h), bytes(alp[:stride * h]), 'raw', 'L', stride, 1)
            img.putalpha(a)
        return img

    @staticmethod
    def magenta_mask(img):
        r, g, b, _ = img.split()
        m = ImageChops.multiply(r.point(lambda v: 255 if v == 255 else 0),
                                g.point(lambda v: 255 if v == 0 else 0))
        return ImageChops.multiply(m, b.point(lambda v: 255 if v == 255 else 0))

    def split_webp(self, data, path):
        # One older game's WebP files may carry an OVER chunk naming a base
        # image ("a700_009.webp"). The file is then a difference image drawn
        # over that base. libwebp rejects the extra chunk, so drop it.
        pos, chunks, base = 12, [], None
        while pos + 8 <= len(data):
            tag, size = data[pos:pos + 4], struct.unpack_from('<I', data, pos + 4)[0]
            body = data[pos + 8:pos + 8 + size]
            if tag == b'OVER':
                name = body.split(b'\0')[0].decode('cp932')
                folder = path.replace('/', '\\').rsplit('\\', 1)[0]
                base = self.image(folder + '\\' + re.sub(r'\.webp$', '.bmp', name))
                self.log.write('OVER %s -> %s\n' % (path, name))
            else:
                chunks.append(data[pos:pos + 8 + size + (size & 1)])
            pos += 8 + size + (size & 1)
        body = b''.join(chunks)
        return b'RIFF' + struct.pack('<I', 4 + len(body)) + b'WEBP' + body, base

    def compose(self):
        canvas = Image.new('RGBA', (W, H), (0, 0, 0, 255))
        for slot in sorted(self.shown):
            sp = self.shown[slot]
            if sp.img is None or sp.alpha <= 0:
                continue
            w = max(1, sp.img.width * sp.sx // 1000)
            h = max(1, sp.img.height * sp.sy // 1000)
            im = sp.img.resize((w, h), Image.BILINEAR) if (w, h) != sp.img.size else sp.img
            if sp.alpha < 256:
                a = im.getchannel('A').point(lambda v: v * sp.alpha // 256)
                im = im.copy()
                im.putalpha(a)
            # Assumed anchor: image centre at screen centre plus (x, y).
            # paste, not alpha_composite, because offsets can be negative.
            canvas.paste(im, (W // 2 + sp.x - w // 2, H // 2 + sp.y - h // 2), im)
        return canvas

    def text_frame(self, name, text):
        canvas = self.compose()
        if self.window and self.msgwin is not None:
            canvas.paste(self.msgwin, (int(self.ini['MsgWinPosX']), int(self.ini['MsgWinPosY'])), self.msgwin)
        text = text.replace('|i', '').replace('^i', '').replace('\r', '')
        layer = Image.new('RGBA', (W, H), (0, 0, 0, 0))
        d = ImageDraw.Draw(layer)
        sz = self.font.size
        line_h = sz + int(self.ini.get('MsgLineInterval', 0)) + 8
        if name:
            d.text((int(self.ini['MsgNamePosX']), int(self.ini['MsgNamePosY'])), name, font=self.font,
                   fill='white', stroke_width=3, stroke_fill='black')
        x, y = int(self.ini['MsgStringPosX']), int(self.ini['MsgStringPosY'])
        for i, line in enumerate(text.split('\n')):
            d.text((x, y + i * line_h), line, font=self.font, fill='white', stroke_width=3, stroke_fill='black')
        canvas.alpha_composite(layer)
        self.count += 1
        path = os.path.join(self.out, 'frame%03d.png' % self.count)
        canvas.convert('RGB').save(path)
        self.log.write('FRAME %s name=%r text=%r\n' % (os.path.basename(path), name, text))
        self.left -= 1

    def run_scene(self, path):
        data = self.vfs.read(path)
        if data is None:
            self.log.write('missing script %s\n' % path)
            return
        nodes, _ = advscript.parse(data)
        byid = {n.id: n for n in nodes}
        n = nodes[0]
        while n is not None and self.left > 0:
            p = [s32(v) for v in struct.unpack('<32I', n.pre)]
            op, s = n.opcode, n.strs
            self.log.write('%s #%d op=%d %s\n' % (os.path.basename(path), n.id, op,
                           advscript.cmd_name(s[0])))
            if op in (10, 11):                            # old API: background or CG load
                self.sprites[0] = Sprite(self.image(s[1]))
            elif op == 13:                                # old API: character at position 位置:N_M
                pos = int(re.search(r'位置:(\d+)', s[0]).group(1))
                sp = Sprite(self.image(s[1]))
                if sp.img is not None:
                    # Positions follow FuckGalEngine's converter (centre, left,
                    # right ...), top-aligned. Not verified against the PC game.
                    sp.x = int(W * POS_X[pos]) - W // 2
                    sp.y = sp.img.height // 2 - H // 2
                self.sprites[10 + pos] = sp
            elif op == 14:
                self.sprites.pop(10 + int(re.search(r'位置:(\d+)', s[0]).group(1)), None)
            elif op == 21:
                for k in [k for k in self.sprites if 10 <= k < 20]:
                    del self.sprites[k]
            elif op == 15:                                # image load into slot p3
                self.sprites[p[3]] = Sprite(self.image(s[1]))
            elif op in (18, 25) and p[3] in self.sprites:  # transform; EX animates, we take the end state
                sp = self.sprites[p[3]]
                if op == 18 or p[16]:
                    sp.x, sp.y = p[1], p[2]
                if op == 18 or p[17]:
                    sp.sx, sp.sy = p[4] or sp.sx, p[5] or sp.sy
                if (op == 18 and p[15]) or p[19]:
                    sp.alpha = p[15]
                if op == 25:
                    self.shown = dict(self.sprites)
            elif op == 17:                                # delete slot p0
                self.sprites.pop(p[0], None)
            elif op == 22:                                # delete all
                self.sprites.clear()
            elif op == 9:                                 # screen update: commit back buffer
                self.shown = dict(self.sprites)
            elif op == 4:
                self.window = False
            elif op == 5:
                self.window = True
                self.text_frame(s[1], s[2])
            n = byid.get(n.next_id)

    def run_flow(self, path):
        nodes, _ = advscript.parse(self.vfs.read(path))
        byid = {n.id: n for n in nodes}
        n = nodes[0]
        while n is not None and self.left > 0:
            self.log.write('FLOW %s #%d op=%d %s\n' % (path, n.id, n.opcode, advscript.cmd_name(n.strs[0])))
            if n.opcode == 1:
                self.run_scene(n.strs[1])
            elif n.opcode == 31:
                return self.run_flow(n.strs[1])
            n = byid.get(n.next_id)


def main():
    root, out = sys.argv[1], sys.argv[2]
    frames = int(sys.argv[3]) if len(sys.argv) > 3 else 12
    os.makedirs(out, exist_ok=True)
    g = Game(root, out, frames)
    if len(sys.argv) > 4:
        # Older games start in exe code. Their first scene is given by hand.
        g.run_scene(sys.argv[4])
    else:
        g.log.write('FirstFlow %s\n' % g.ini['FirstFlow'])
        g.run_flow(g.ini['FirstFlow'])
    g.log.close()


if __name__ == '__main__':
    main()
