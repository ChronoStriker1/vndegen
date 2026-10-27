//! Lua 5.1 bytecode from 32-bit `luac` on a 64-bit Lua (VNDegen).
//!
//! Many Artemis games ship their Lua as `.lub` chunks compiled by a 32-bit
//! Windows `luac`, whose header records a 4-byte `size_t`. A 64-bit Lua 5.1
//! rejects them with "bad header in precompiled chunk". `size_t` appears only in
//! the header and as the length before each string, so widening those lengths
//! gives the same chunk for this Lua. The layout follows `lundump.c` in Lua 5.1.

const SIGNATURE: &[u8] = b"\x1bLua\x51\x00";
const HEADER_LEN: usize = 12;
const SIZE_T_AT: usize = 8;

/// Returns `chunk` rewritten for this Lua's `size_t` when it is a little-endian
/// Lua 5.1 chunk with 4-byte `int`, `size_t` and instructions and 8-byte
/// numbers, and this Lua's `size_t` is wider. Returns `None` for anything else,
/// which is then loaded unchanged.
pub fn widen_size_t(chunk: &[u8]) -> Option<Vec<u8>> {
    let host = std::mem::size_of::<usize>() as u8;
    let header = chunk.get(..HEADER_LEN)?;
    if !header.starts_with(SIGNATURE)
        || header[6] != 1
        || header[7] != 4
        || header[SIZE_T_AT] != 4
        || header[9] != 4
        || header[10] != 8
        || host <= 4
    {
        return None;
    }
    let mut writer = Writer {
        input: chunk,
        at: HEADER_LEN,
        output: Vec::with_capacity(chunk.len() + chunk.len() / 8),
        size_t: host as usize,
    };
    writer.output.extend_from_slice(header);
    writer.output[SIZE_T_AT] = host;
    writer.function()?;
    (writer.at == chunk.len()).then_some(writer.output)
}

struct Writer<'a> {
    input: &'a [u8],
    at: usize,
    output: Vec<u8>,
    size_t: usize,
}

impl Writer<'_> {
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

    fn string(&mut self) -> Option<()> {
        let bytes = self.input.get(self.at..self.at + 4)?;
        let len = u32::from_le_bytes(bytes.try_into().ok()?) as usize;
        self.at += 4;
        let mut widened = (len as u64).to_le_bytes().to_vec();
        widened.truncate(self.size_t);
        self.output.extend_from_slice(&widened);
        self.copy(len)
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
                0 => {}                     // nil
                1 => self.copy(1)?,         // boolean
                3 => self.copy(8)?,         // number
                4 => self.string()?,        // string
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
    use super::widen_size_t;

    fn string32(text: &[u8]) -> Vec<u8> {
        let mut bytes = ((text.len() + 1) as u32).to_le_bytes().to_vec();
        bytes.extend_from_slice(text);
        bytes.push(0);
        bytes
    }

    fn int(value: i32) -> Vec<u8> {
        value.to_le_bytes().to_vec()
    }

    /// A 32-bit chunk with one string constant, one nested function, and debug
    /// names, and the same chunk as 64-bit `luac` would write it.
    fn chunks() -> (Vec<u8>, Vec<u8>) {
        let header = |size_t: u8| {
            let mut bytes = b"\x1bLua\x51\x00\x01\x04".to_vec();
            bytes.extend_from_slice(&[size_t, 4, 8, 0]);
            bytes
        };
        let string64 = |text: &[u8]| {
            let mut bytes = ((text.len() + 1) as u64).to_le_bytes().to_vec();
            bytes.extend_from_slice(text);
            bytes.push(0);
            bytes
        };
        let function = |string: &dyn Fn(&[u8]) -> Vec<u8>, empty: Vec<u8>, inner: Vec<u8>| {
            let mut bytes = string(b"@a.lua");
            bytes.extend(int(0));
            bytes.extend(int(0));
            bytes.extend([0, 0, 2, 2]);
            bytes.extend(int(1));
            bytes.extend([0x1e, 0, 0x80, 0]);
            bytes.extend(int(2));
            bytes.extend([4]);
            bytes.extend(string(b"hello"));
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
            bytes.extend(empty);
            bytes
        };
        let inner32 = function(&string32, Vec::new(), Vec::new());
        let inner64 = function(&string64, Vec::new(), Vec::new());
        let mut old = header(4);
        old.extend(function(&string32, Vec::new(), inner32));
        let mut new = header(8);
        new.extend(function(&string64, Vec::new(), inner64));
        (old, new)
    }

    #[test]
    fn widens_every_string_length_and_the_header() {
        let (old, new) = chunks();
        assert_eq!(widen_size_t(&old), Some(new));
    }

    #[test]
    fn leaves_other_chunks_and_source_alone() {
        let (_, new) = chunks();
        assert_eq!(widen_size_t(&new), None);
        assert_eq!(widen_size_t(b"return 1"), None);
        let (mut truncated, _) = chunks();
        truncated.pop();
        assert_eq!(widen_size_t(&truncated), None);
    }
}
