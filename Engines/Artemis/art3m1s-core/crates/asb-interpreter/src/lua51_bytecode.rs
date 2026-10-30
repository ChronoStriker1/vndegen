//! Lua 5.1 bytecode from a Windows `luac` on this Lua (VNDegen).
//!
//! Many Artemis games ship their Lua as `.lub` chunks compiled by a 32-bit
//! Windows `luac`, whose header records a 4-byte `size_t`. A 64-bit Lua 5.1
//! rejects them with "bad header in precompiled chunk". `size_t` appears only in
//! the header and as the length before each string, so rewriting those lengths
//! gives the same chunk for this Lua. The layout follows `lundump.c` in Lua 5.1.
//!
//! The chunk's strings are in the game's charset, often Shift_JIS, while the
//! interpreter decodes scripts to UTF-8. A tag filter keyed by a Shift_JIS
//! string would never match the tag a script names, so strings that aren't
//! valid UTF-8 are decoded with the game's charset as the chunk is rewritten.

use encoding_rs::Encoding;

const SIGNATURE: &[u8] = b"\x1bLua\x51\x00";
const HEADER_LEN: usize = 12;
const SIZE_T_AT: usize = 8;

/// Returns `chunk` rewritten for this Lua's `size_t`, with its strings in UTF-8,
/// when it is a little-endian Lua 5.1 chunk with 4-byte `int` and instructions,
/// 8-byte numbers, and a 4- or 8-byte `size_t`, and the rewrite changes it.
/// Returns `None` for anything else, which is then loaded unchanged.
pub fn adapt(chunk: &[u8], encoding: &'static Encoding) -> Option<Vec<u8>> {
    let host = std::mem::size_of::<usize>();
    let header = chunk.get(..HEADER_LEN)?;
    let size_t = header[SIZE_T_AT] as usize;
    if !header.starts_with(SIGNATURE)
        || header[6] != 1
        || header[7] != 4
        || !matches!(size_t, 4 | 8)
        || header[9] != 4
        || header[10] != 8
    {
        return None;
    }
    let mut writer = Writer {
        input: chunk,
        at: HEADER_LEN,
        output: Vec::with_capacity(chunk.len() + chunk.len() / 8),
        size_t,
        host,
        encoding,
    };
    writer.output.extend_from_slice(header);
    writer.output[SIZE_T_AT] = host as u8;
    writer.function()?;
    (writer.at == chunk.len() && writer.output != chunk).then_some(writer.output)
}

struct Writer<'a> {
    input: &'a [u8],
    at: usize,
    output: Vec<u8>,
    size_t: usize,
    host: usize,
    encoding: &'static Encoding,
}

impl Writer<'_> {
    fn take(&mut self, len: usize) -> Option<&[u8]> {
        let bytes = self.input.get(self.at..self.at.checked_add(len)?)?;
        self.at += len;
        Some(bytes)
    }

    fn copy(&mut self, len: usize) -> Option<()> {
        let bytes = self.input.get(self.at..self.at.checked_add(len)?)?;
        self.output.extend_from_slice(bytes);
        self.at += len;
        Some(())
    }

    fn int(&mut self) -> Option<usize> {
        let bytes = self.input.get(self.at..self.at + 4)?;
        let value = i32::from_le_bytes(bytes.try_into().ok()?);
        self.copy(4)?;
        usize::try_from(value).ok()
    }

    fn byte(&mut self) -> Option<u8> {
        let value = *self.input.get(self.at)?;
        self.copy(1)?;
        Some(value)
    }

    /// A string is its length, counting a trailing NUL, then its bytes; a
    /// length of 0 stands for no string.
    fn string(&mut self) -> Option<()> {
        let size_t = self.size_t;
        let mut len = [0u8; 8];
        len[..size_t].copy_from_slice(self.take(size_t)?);
        let len = usize::try_from(u64::from_le_bytes(len)).ok()?;
        let encoding = self.encoding;
        let text = match len {
            0 => Vec::new(),
            _ => {
                let bytes = self.take(len)?;
                let (body, nul) = bytes.split_at(len - 1);
                if nul != [0] {
                    return None;
                }
                let mut text = match std::str::from_utf8(body) {
                    Ok(_) => body.to_vec(),
                    Err(_) => encoding.decode(body).0.into_owned().into_bytes(),
                };
                text.push(0);
                text
            }
        };
        let mut widened = (text.len() as u64).to_le_bytes().to_vec();
        widened.truncate(self.host);
        self.output.extend_from_slice(&widened);
        self.output.extend_from_slice(&text);
        Some(())
    }

    fn function(&mut self) -> Option<()> {
        self.string()?; // source
        self.int()?; // linedefined
        self.int()?; // lastlinedefined
        self.copy(4)?; // nups, numparams, is_vararg, maxstacksize
        let code = self.int()?;
        self.copy(code.checked_mul(4)?)?;
        for _ in 0..self.int()? {
            match self.byte()? {
                0 => {}              // nil
                1 => self.copy(1)?,  // boolean
                3 => self.copy(8)?,  // number
                4 => self.string()?, // string
                _ => return None,
            }
        }
        for _ in 0..self.int()? {
            self.function()?;
        }
        let lines = self.int()?;
        self.copy(lines.checked_mul(4)?)?;
        for _ in 0..self.int()? {
            self.string()?; // local name
            self.copy(8)?; // startpc, endpc
        }
        for _ in 0..self.int()? {
            self.string()?; // upvalue name
        }
        Some(())
    }
}

#[cfg(test)]
mod tests {
    use super::adapt;
    use encoding_rs::{SHIFT_JIS, UTF_8};

    fn int(value: i32) -> Vec<u8> {
        value.to_le_bytes().to_vec()
    }

    /// A chunk with `size_t` bytes per length, holding one string constant, a
    /// number, a nested function, and debug names.
    fn chunk(size_t: usize, constant: &[u8]) -> Vec<u8> {
        let string = |text: &[u8]| {
            let mut bytes = ((text.len() + 1) as u64).to_le_bytes()[..size_t].to_vec();
            bytes.extend_from_slice(text);
            bytes.push(0);
            bytes
        };
        let function = |inner: Vec<u8>| {
            let mut bytes = string(b"@a.lua");
            bytes.extend(int(0));
            bytes.extend(int(0));
            bytes.extend([0, 0, 2, 2]);
            bytes.extend(int(1));
            bytes.extend([0x1e, 0, 0x80, 0]);
            bytes.extend(int(2));
            bytes.extend([4]);
            bytes.extend(string(constant));
            bytes.extend([3]);
            bytes.extend(1.5f64.to_le_bytes());
            bytes.extend(int(if inner.is_empty() { 0 } else { 1 }));
            bytes.extend(inner);
            bytes.extend(int(1));
            bytes.extend(int(7));
            bytes.extend(int(1));
            bytes.extend(string(b"x"));
            bytes.extend(int(0));
            bytes.extend(int(1));
            bytes.extend(int(0));
            bytes
        };
        let mut bytes = b"\x1bLua\x51\x00\x01\x04".to_vec();
        bytes.extend_from_slice(&[size_t as u8, 4, 8, 0]);
        bytes.extend(function(function(Vec::new())));
        bytes
    }

    #[test]
    fn widens_every_string_length_and_the_header() {
        assert_eq!(adapt(&chunk(4, b"hello"), UTF_8), Some(chunk(8, b"hello")));
    }

    #[test]
    fn decodes_strings_in_the_game_charset() {
        let (sjis, _, _) = SHIFT_JIS.encode("行頭");
        assert_eq!(
            adapt(&chunk(4, &sjis), SHIFT_JIS),
            Some(chunk(8, "行頭".as_bytes()))
        );
        assert_eq!(
            adapt(&chunk(8, &sjis), SHIFT_JIS),
            Some(chunk(8, "行頭".as_bytes()))
        );
    }

    #[test]
    fn leaves_other_chunks_and_source_alone() {
        assert_eq!(adapt(&chunk(8, b"hello"), UTF_8), None);
        assert_eq!(adapt(b"return 1", UTF_8), None);
        let mut truncated = chunk(4, b"hello");
        truncated.pop();
        assert_eq!(adapt(&truncated, UTF_8), None);
    }
}
