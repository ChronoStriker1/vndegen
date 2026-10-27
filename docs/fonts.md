# Windows fonts

PC games ask for Windows fonts by name, such as MS PGothic, MS Mincho, Arial, or Microsoft YaHei. VNDegen can't ship
Microsoft's fonts. It does what Proton does instead: it ships freely licensed fonts with the same metrics and maps each
Windows name to one, so text wraps and fits the way the game was laid out. One table holds the mapping, and every
engine reads it.

## The fonts

The app bundles the Latin and Japanese fonts in `Engines/Shared/fonts/`, unmodified and under their upstream names.
They take 27.2 MB in the app and about 13 MB in the zipped IPA. The Chinese and Korean fonts are large, so Settings,
Fonts offers them as downloads.

| Windows names | Substitute | File | Source | License |
|---|---|---|---|---|
| Arial, Microsoft Sans Serif, MS Sans Serif | Liberation Sans | `LiberationSans-Regular.ttf`, `-Bold.ttf` | Liberation 2.1.5 release | SIL OFL 1.1 |
| Times New Roman, Georgia, MS Serif | Liberation Serif | `LiberationSerif-Regular.ttf`, `-Bold.ttf` | Liberation 2.1.5 release | SIL OFL 1.1 |
| Courier New, Lucida Console | Liberation Mono | `LiberationMono-Regular.ttf`, `-Bold.ttf` | Liberation 2.1.5 release | SIL OFL 1.1 |
| Tahoma, MS Shell Dlg 2 | DejaVu Sans Condensed | `DejaVuSansCondensed.ttf`, `-Bold.ttf` | DejaVu 2.37 release | Bitstream Vera license, DejaVu changes public domain |
| MS Gothic, ＭＳ ゴシック | Ume Gothic | `ume-tgo4.ttf` | Ume font 670 | Ume font license |
| MS PGothic, ＭＳ Ｐゴシック | Ume P Gothic | `ume-pgo4.ttf` | Ume font 670 | Ume font license |
| MS UI Gothic | Ume UI Gothic | `ume-ugo4.ttf` | Ume font 670 | Ume font license |
| MS Mincho, ＭＳ 明朝 | Ume Mincho | `ume-tmo3.ttf` | Ume font 670 | Ume font license |
| MS PMincho, ＭＳ Ｐ明朝 | Ume P Mincho | `ume-pmo3.ttf` | Ume font 670 | Ume font license |
| Meiryo, メイリオ, Meiryo UI, Yu Gothic, 游ゴシック, Yu Gothic UI | Ume P Gothic (closest, not a metric match) | `ume-pgo4.ttf` | Ume font 670 | Ume font license |
| Yu Mincho, 游明朝 | Ume P Mincho (closest, not a metric match) | `ume-pmo3.ttf` | Ume font 670 | Ume font license |
| Microsoft YaHei, 微软雅黑, SimHei, 黑体, SimSun, 宋体, NSimSun, 新宋体 | Source Han Sans CN, download | `SourceHanSansCN-Regular.otf`, `-Bold.otf` | Source Han Sans 2.005R, `19_SourceHanSansCN.zip` | SIL OFL 1.1 |
| Malgun Gothic, 맑은 고딕, Dotum, 돋움, DotumChe, 돋움체, Gulim, 굴림, GulimChe, 굴림체 | Source Han Sans KR, download | `SourceHanSansKR-Regular.otf`, `-Bold.otf` | Source Han Sans 2.005R, `18_SourceHanSansKR.zip` | SIL OFL 1.1 |

Where each comes from:

- Liberation: `liberation-fonts-ttf-2.1.5.tar.gz` from the 2.1.5 release of github.com/liberationfonts/liberation-fonts
  (SHA-256 `7191c669bf38899f73a2094ed00f7b800553364f90e2637010a69c0e268f25d0`). Proton builds its Arial, Times New
  Roman, Georgia, and Courier New from the same Liberation 2.1.5 sources, renamed.
- Ume font 670: the upstream tarball, as Debian keeps it, `fonts-horai-umefont_670.orig.tar.xz` (SHA-256
  `49ecb8f556c2448687d27bb485009ce51b90df26bb4f0c24c9e63b170c647c81`). The project's own download site, OSDN, no
  longer exists. Its Gothic files are byte for byte the ones Proton keeps in `fonts/ume/` and renames into its
  `msgothic.ttc`. The license allows use, copying, and distribution, with or without changes, commercial or not.
- DejaVu: `dejavu-fonts-ttf-2.37.tar.bz2` from the 2.37 release of github.com/dejavu-fonts/dejavu-fonts (SHA-256
  `fa9ca4d13871dd122f61258a80d01751d603b4d3ee14095d65453b4e846e17d7`). Its license allows selling the fonts as part
  of a larger software package. Wine's own Tahoma is LGPL, so VNDegen doesn't use it.
- Source Han Sans: Adobe's 2.005R release on github.com/adobe-fonts/source-han-sans, pinned by SHA-256 in
  `FontPack.swift`. The download keeps Regular, Bold, and Adobe's `LICENSE.txt`. Proton builds its Microsoft YaHei,
  SimSun, and Malgun Gothic from the same fonts.

Proton also has Microsoft Sans Serif and Nirmala UI built from merged Noto fonts. VNDegen maps Microsoft Sans Serif
to Liberation Sans and leaves out Nirmala UI, which covers Indic scripts that the library's games don't use.

## How well they match

Measured with the advance widths in each font, against the real Windows fonts:

- Ume Gothic, Ume P Gothic, and Ume UI Gothic have exactly the widths and vertical metrics of MS Gothic, MS PGothic,
  and MS UI Gothic for Japanese text, Latin letters, and half-width kana.
- Ume Mincho has MS Gothic's fixed widths and vertical metrics, which MS Mincho shares. Ume P Mincho's proportional
  widths differ from MS PGothic's by 0.018 em a glyph on average. MS PMincho itself wasn't available to compare.
- Liberation Sans, Serif, and Mono have the widths of Arial, Times New Roman, and Courier New. Lucida Console's
  widths differ from Liberation Mono's by 0.0024 em a glyph; its line is shorter (1.0 em against 1.13 em).
- DejaVu Sans Condensed is the closest permissive match for Tahoma: a line of English text is 2.0% wider in regular
  and 1.3% wider in bold, and the line is 1.16 em tall against Tahoma's 1.21 em. Liberation Sans is closer in
  regular (0.5%) but 5.4% narrower in bold, and its letters differ more one by one.
- Meiryo and Yu Gothic have no free metric match. They get Ume P Gothic, a Japanese proportional Gothic.
- Source Han Sans doesn't match the metrics of the Chinese and Korean Windows fonts. Proton uses it for the same
  reason VNDegen does: it is the free font that covers them.

## The table

`Engines/Shared/fonts/fonts.tsv` has one line per Windows name and style: the name, the style (`regular`, `bold`,
`italic`, or `bold-italic`), the substitute's file name, and its face index in a collection. Lookups ignore ASCII
case, spaces, full-width forms of ASCII letters, and a leading `@` (Windows' vertical variant), so "ＭＳ Ｐゴシック",
"MS Pゴシック", and "ms pgothic" find the same lines. A style with no line of its own falls back to regular, and the
engine draws it bolder or slanted, as Windows does. Lines whose file isn't installed are skipped.

To add a name, add its lines. To add a font, put the unmodified file in `Engines/Shared/fonts/`, its license in
`licenses/`, a line for it in `scripts/fonts-licenses.sh`, and its lines in the table. `FontPackTests` fails when a
bundled font has no line in the table.

## The C interface

`Engines/Shared/vnfont.h` is what native engines call. Each engine compiles `vnfont.c` from that folder.

```c
int vnfont_init(const char *table_path, const char *const *font_dirs, int dir_count);
int vnfont_resolve(const char *name, int bold, int italic, vnfont_match *match);
int vnfont_normalize(const char *name, char *out, size_t size);
int vnfont_count(void);
int vnfont_entry_at(int index, vnfont_entry *entry);
```

The engine calls `vnfont_init` once with the table and the font folders the app passes it: the bundled folder first,
then each installed download. `vnfont_resolve` returns the file path, the face index, and whether to embolden or
slant the regular face. It returns 0 for a name the table doesn't have, and the engine keeps its own default then.
The interface is stable: new functions may be added, and existing ones keep their meaning. `vnfont_test.c` tests it,
and `scripts/build.sh test` runs it.

## The Swift API

The `FontPack` module wraps the same C code. The app calls `FontPack.configure(installedFolder:)` at launch, and
Settings calls it again after a download or removal. `FontPack.resolve(_:bold:italic:)` and `FontPack.entries` read
the table, and `FontPack.folders` and `FontPack.table` are what a native engine passes to `vnfont_init`.
`FontPack.packages` are the two downloads.

## Engines

- **RPG Maker MV and MZ, TyranoScript.** The page gets a font face for each Windows name, and for its `@` name, whose
  source is the device's own font of that name when it has one, then the substitute. macOS and iOS have Arial, Times
  New Roman, Courier New, and Georgia, so those games use the real fonts there; macOS also has Tahoma. The substitutes
  serve from `vndegen://game/.vndegen/fonts/`. A face loads when the page first uses it. Canvas text keeps the font it
  was first drawn in, so the RPG Maker plugin looks for the table's names in the game's plugins and `System.json` and
  loads those fonts up front, and the boot scene waits for them. MV's Chinese and Korean games ask for SimHei and
  Dotum through the core, which the plugin adds from the database's locale. TyranoScript draws text as HTML, so a late
  font only redraws it; the plugin loads the font `Config.tjs` names up front so the first lines don't change.
- **KiriKiri and RGSS** read the table through `vnfont.h`. See their notes for where.
- **Ren'Py** games ship their own fonts, so Ren'Py doesn't use the table.

CSS matches family names exactly, apart from ASCII case, as Windows does. A game that asks for "ＭＳＰ明朝" without
the space gets no font on Windows either and falls back to the next family in its list, and VNDegen does the same.

## Checked

Measured in the page with `measureText`, against the widths computed from the font files with HarfBuzz:

- Test Game 35 (TyranoScript) names メイリオ. On an iPad Pro 13-inch simulator and in the macOS app, its text measured
  652.734 px at 100 px, Ume P Gothic's width, and ＭＳ ゴシック measured 650.000 px once loaded, Ume Gothic's width.
  On the iPad, its dialogue fits the message window on one line.
- No Socks RPG (RPG Maker MZ) lists Arial, Georgia, and Tahoma in its options. On the iPad, the boot scene ended only
  after those loaded, and with Tahoma chosen the options use DejaVu Sans Condensed (908.105 px, its kerned width) with
  every label inside its window. In the macOS app, Tahoma measured 899.268 px, the system's real Tahoma.
- The Korean download installs from Settings on the iPad, Dotum then measures 466.300 px, Source Han Sans KR's width,
  and Remove deletes it.
