//! Bitmap fonts in the `.rft` files some games ship in place of a TrueType
//! font, one file per size: `image/font/<name>_<size>.rft` (VNDegen).
//!
//! A file starts with `rft3` and a table of (offset, size) pairs, little
//! endian. Entry 0 is a text header with no metrics. Each later entry is a
//! grayscale PNG glyph as tall as the size, whose width is the glyph's advance
//! before the engine's outline, shadow, and kerning. The files store no
//! character map: entries 1 to 94 are ASCII `!` to `~`, and the rest follow
//! the source font's coverage. Only the ASCII range is read, and other
//! characters use the stand-in font.

use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};

/// The largest size probed for a face's files.
const MAX_SIZE: u32 = 160;
const FIRST: char = '!';
const LAST: char = '~';

pub struct RftGlyph {
    pub width: u32,
    pub height: u32,
    /// One coverage byte per pixel, row by row.
    pub coverage: Vec<u8>,
}

/// One face's ASCII glyphs, for every size the game ships.
pub struct RftFamily {
    /// Keys this family's glyphs in the renderer's atlas cache, apart from
    /// TrueType fonts' generations.
    pub cache_id: u64,
    sizes: HashMap<u32, HashMap<char, RftGlyph>>,
}

impl RftFamily {
    /// Reads `<face>_<size>.rft` for each size the game has. `query_size`
    /// gives a file's length, or None when it is missing, and `read_range`
    /// fills a buffer from an offset. None when the game has no such file.
    pub fn load(
        face: &str,
        query_size: impl Fn(&str) -> Option<u64>,
        read_range: impl Fn(&str, u64, &mut [u8]) -> Result<usize, String>,
    ) -> Option<Self> {
        let face = face.strip_suffix(".rft").unwrap_or(face);
        static NEXT: AtomicU64 = AtomicU64::new(0);
        let mut family = Self {
            cache_id: (1 << 63) | NEXT.fetch_add(1, Ordering::Relaxed),
            sizes: HashMap::new(),
        };
        for size in 1..=MAX_SIZE {
            let path = format!("{face}_{size}.rft");
            let Some(length) = query_size(&path) else {
                continue;
            };
            match read_ascii(length, |offset, out| read_range(&path, offset, out)) {
                Ok(glyphs) => {
                    family.sizes.insert(size, glyphs);
                }
                Err(error) => {
                    crate::core_warn!("[text] {path}: {error}");
                }
            }
        }
        (!family.sizes.is_empty()).then_some(family)
    }

    /// The glyph for `c` at `size`, when the game ships that size.
    pub fn glyph(&self, c: char, size: f32) -> Option<&RftGlyph> {
        self.sizes.get(&exact_size(size)?)?.get(&c)
    }

    pub fn has_size(&self, size: f32) -> bool {
        exact_size(size).is_some_and(|size| self.sizes.contains_key(&size))
    }
}

fn exact_size(size: f32) -> Option<u32> {
    (size.fract() == 0.0 && size > 0.0).then_some(size as u32)
}

fn read_ascii(
    length: u64,
    read: impl Fn(u64, &mut [u8]) -> Result<usize, String>,
) -> Result<HashMap<char, RftGlyph>, String> {
    let count = LAST as usize - FIRST as usize + 1;
    let mut head = vec![0u8; 4 + 8 * (count + 1)];
    if read(0, &mut head)? != head.len() || &head[..4] != b"rft3" {
        return Err("not an rft3 font".into());
    }
    let entry = |index: usize| {
        let at = 4 + 8 * index;
        let word = |at: usize| u32::from_le_bytes(head[at..at + 4].try_into().unwrap()) as u64;
        (word(at), word(at + 4))
    };
    let mut glyphs = HashMap::new();
    for (index, c) in (FIRST..=LAST).enumerate() {
        let (offset, size) = entry(index + 1);
        if offset + size > length {
            return Err(format!("glyph {c:?} lies past the end of the file"));
        }
        let mut png = vec![0u8; size as usize];
        if read(offset, &mut png)? != png.len() {
            return Err(format!("glyph {c:?} is cut short"));
        }
        let image = image::load_from_memory_with_format(&png, image::ImageFormat::Png)
            .map_err(|error| format!("glyph {c:?}: {error}"))?;
        let (width, height) = (image.width(), image.height());
        let coverage = match image {
            image::DynamicImage::ImageLumaA8(image) => image.pixels().map(|p| p.0[1]).collect(),
            image::DynamicImage::ImageRgba8(image) => image.pixels().map(|p| p.0[3]).collect(),
            image => image.to_luma8().into_raw(),
        };
        glyphs.insert(
            c,
            RftGlyph {
                width,
                height,
                coverage,
            },
        );
    }
    Ok(glyphs)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn png(width: u32, height: u32) -> Vec<u8> {
        let mut out = std::io::Cursor::new(Vec::new());
        image::GrayImage::from_fn(width, height, |x, _| image::Luma([(x * 40) as u8]))
            .write_to(&mut out, image::ImageFormat::Png)
            .unwrap();
        out.into_inner()
    }

    fn rft(size: u32) -> Vec<u8> {
        let header = b"[FONT]\r\nCHARSET = Shift_JIS\r\n".to_vec();
        let mut entries = vec![header];
        for index in 0..94u32 {
            entries.push(png(4 + index % 5, size));
        }
        let mut offset = 4 + 8 * entries.len() as u32;
        let mut out = b"rft3".to_vec();
        for entry in &entries {
            out.extend(offset.to_le_bytes());
            out.extend((entry.len() as u32).to_le_bytes());
            offset += entry.len() as u32;
        }
        for entry in entries {
            out.extend(entry);
        }
        out
    }

    #[test]
    fn reads_the_ascii_glyphs_of_each_size_a_game_ships() {
        let files: HashMap<String, Vec<u8>> = [
            ("image/font/Face_26.rft".to_string(), rft(26)),
            ("image/font/Face_48.rft".to_string(), rft(48)),
        ]
        .into();
        let family = RftFamily::load(
            "image/font/Face",
            |path| files.get(path).map(|file| file.len() as u64),
            |path, offset, out| {
                let file = &files[path];
                let start = offset as usize;
                let end = (start + out.len()).min(file.len());
                out[..end - start].copy_from_slice(&file[start..end]);
                Ok(end - start)
            },
        )
        .unwrap();
        // '!' is entry 1, 4 wide; 'A' is entry 33, 4 + 32 % 5 = 6 wide.
        let bang = family.glyph('!', 26.0).unwrap();
        assert_eq!((bang.width, bang.height), (4, 26));
        assert_eq!(bang.coverage[..4], [0, 40, 80, 120]);
        assert_eq!(family.glyph('A', 48.0).unwrap().width, 6);
        assert!(family.has_size(48.0) && !family.has_size(21.0));
        assert!(family.glyph('あ', 26.0).is_none() && family.glyph(' ', 26.0).is_none());
        assert!(RftFamily::load("image/font/Other", |_| None, |_, _, _| Ok(0)).is_none());
    }
}
