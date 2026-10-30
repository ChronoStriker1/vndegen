//! The object format of the PC Artemis engine's save files (VNDegen).
//!
//! A file is a four-byte magic, a version, and a root object of numbered
//! fields. A field holds bytes, such as ints, strings, and counts, and
//! objects nested among them. Integers are little-endian 32-bit, and a
//! string is its byte length and its bytes. The engine writes two versions:
//!
//! - 1000 writes an object as its field count, a table of `(field, offset)`
//!   pairs, then the fields' data, with offsets counting from the field
//!   count. An object nested in a field has its byte length in front.
//! - 1002 follows the version with the uncompressed length and compresses
//!   the rest with zlib. An object is only its fields' data. The tables come
//!   after all the data, in the order the objects end, so the root's comes
//!   last: the number of tables, then for each the object's offset, its
//!   field count, and `(field, offset)` pairs with offsets from the start.
//!   The file ends with the offset of the tables. The root's offset is 0.

use super::pc::Reader;
use std::collections::HashMap;
use std::io::{Read, Write};
use std::ops::Range;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Version {
    V1000,
    V1002,
}

impl Version {
    fn number(self) -> u32 {
        match self {
            Self::V1000 => 1000,
            Self::V1002 => 1002,
        }
    }
}

/// A file's fields, by number, as byte ranges of [`Document::bytes`].
#[derive(Debug, Clone)]
pub struct Object {
    fields: Vec<(u32, Range<usize>)>,
}

impl Object {
    pub fn field(&self, number: u32) -> Option<Range<usize>> {
        self.fields
            .iter()
            .find(|(candidate, _)| *candidate == number)
            .map(|(_, range)| range.clone())
    }

    pub fn require(&self, number: u32) -> Result<Range<usize>, String> {
        self.field(number)
            .ok_or_else(|| format!("no field {number}"))
    }

    /// Fields from `(number, start)` pairs. A field runs to the next one's
    /// start, or to the end of the object.
    fn from_starts(table: &[(u32, usize)], end: usize) -> Result<Self, String> {
        let mut starts: Vec<usize> = table.iter().map(|(_, start)| *start).collect();
        starts.sort_unstable();
        let fields = table
            .iter()
            .map(|&(number, start)| {
                let next = starts
                    .iter()
                    .copied()
                    .find(|&next| next > start)
                    .unwrap_or(end);
                if start > end {
                    return Err(format!("field {number} starts past its object"));
                }
                Ok((number, start..next))
            })
            .collect::<Result<_, String>>()?;
        Ok(Self { fields })
    }
}

/// An opened save file.
pub struct Document {
    pub version: Version,
    body: Vec<u8>,
    /// Version 1002's tables, by object offset.
    tables: HashMap<usize, Vec<(u32, usize)>>,
    /// Where the root object's data ends.
    root_end: usize,
}

impl Document {
    /// Opens a file, checking its magic.
    pub fn open(bytes: &[u8], magic: &[u8; 4]) -> Result<Self, String> {
        let mut reader = Reader::new(bytes);
        if reader.take(4)? != magic {
            return Err(format!("not a {} file", String::from_utf8_lossy(magic)));
        }
        match reader.u32()? {
            1000 => {
                let body = bytes[8..].to_vec();
                let root_end = body.len();
                Ok(Self {
                    version: Version::V1000,
                    body,
                    tables: HashMap::new(),
                    root_end,
                })
            }
            1002 => {
                let len = reader.u32()? as usize;
                let mut body = Vec::with_capacity(len.min(64 << 20));
                flate2::read::ZlibDecoder::new(&bytes[12..])
                    .read_to_end(&mut body)
                    .map_err(|e| format!("save data doesn't decompress: {e}"))?;
                if body.len() != len {
                    return Err(format!("save data is {} bytes, expected {len}", body.len()));
                }
                let mut tail =
                    Reader::new(body.get(body.len().saturating_sub(4)..).unwrap_or_default());
                let root_end = tail.u32()? as usize;
                let mut tables = Reader::new(body.get(root_end..).ok_or("tables past the end")?);
                let mut by_offset = HashMap::new();
                for _ in 0..tables.u32()? {
                    let offset = tables.u32()? as usize;
                    let count = tables.u32()?;
                    let mut table = Vec::with_capacity(count.min(256) as usize);
                    for _ in 0..count {
                        let number = tables.u32()?;
                        let start = tables.u32()? as usize;
                        table.push((number, start));
                    }
                    by_offset.insert(offset, table);
                }
                Ok(Self {
                    version: Version::V1002,
                    body,
                    tables: by_offset,
                    root_end,
                })
            }
            version => Err(format!("save version {version}, expected 1000 or 1002")),
        }
    }

    pub fn bytes(&self, range: Range<usize>) -> &[u8] {
        &self.body[range]
    }

    pub fn root(&self) -> Result<Object, String> {
        match self.version {
            Version::V1000 => self.object_body(0, self.root_end),
            Version::V1002 => self.object_at(0, self.root_end),
        }
    }

    /// The object a field holds.
    pub fn object(&self, field: Range<usize>) -> Result<Object, String> {
        match self.version {
            Version::V1000 => {
                let mut cursor = self.cursor(field);
                let len = cursor.u32()? as usize;
                let start = cursor.position();
                cursor.take(len)?;
                self.object_body(start, start + len)
            }
            Version::V1002 => self.object_at(field.start, field.end),
        }
    }

    pub fn cursor(&self, range: Range<usize>) -> Cursor<'_> {
        Cursor {
            document: self,
            at: range.start,
            end: range.end,
        }
    }

    /// A version 1000 object body, from its field count to `end`.
    fn object_body(&self, start: usize, end: usize) -> Result<Object, String> {
        let mut reader = Reader::new(self.body.get(start..end).ok_or("an object past the end")?);
        let count = reader.u32()?;
        let mut table = Vec::with_capacity(count.min(256) as usize);
        for _ in 0..count {
            let number = reader.u32()?;
            table.push((number, start + reader.u32()? as usize));
        }
        Object::from_starts(&table, end)
    }

    /// A version 1002 object at `start`, whose last field runs to `end`.
    fn object_at(&self, start: usize, end: usize) -> Result<Object, String> {
        let table = self
            .tables
            .get(&start)
            .ok_or_else(|| format!("no object at {start}"))?;
        Object::from_starts(table, end)
    }
}

/// Reads a field's bytes in order.
pub struct Cursor<'a> {
    document: &'a Document,
    at: usize,
    end: usize,
}

impl<'a> Cursor<'a> {
    pub fn document(&self) -> &'a Document {
        self.document
    }

    pub fn position(&self) -> usize {
        self.at
    }

    fn take(&mut self, len: usize) -> Result<&'a [u8], String> {
        let end = self
            .at
            .checked_add(len)
            .filter(|end| *end <= self.end)
            .ok_or_else(|| format!("save data ends at {}", self.end))?;
        let bytes = &self.document.body[self.at..end];
        self.at = end;
        Ok(bytes)
    }

    pub fn u32(&mut self) -> Result<u32, String> {
        Ok(u32::from_le_bytes(self.take(4)?.try_into().unwrap()))
    }

    pub fn string(&mut self) -> Result<&'a [u8], String> {
        let len = self.u32()? as usize;
        self.take(len)
    }

    /// The next object. A version 1002 object has no length, so `end_of`
    /// gives where it ends from its fields; the cursor moves past it.
    pub fn object(
        &mut self,
        end_of: impl FnOnce(&'a Document, &Object) -> Result<usize, String>,
    ) -> Result<Object, String> {
        match self.document.version {
            Version::V1000 => {
                let len = self.u32()? as usize;
                let start = self.at;
                self.take(len)?;
                self.document.object_body(start, start + len)
            }
            Version::V1002 => {
                let object = self.document.object_at(self.at, self.end)?;
                self.at = end_of(self.document, &object)?;
                Ok(object)
            }
        }
    }
}

/// A field's contents to write: bytes and nested objects, in order.
#[derive(Debug, Clone)]
pub enum Node {
    Bytes(Vec<u8>),
    Object(ObjectNode),
}

/// An object to write. Fields go in the order given, and the table follows
/// `table_order` when it is set, as the PC engine's does.
#[derive(Debug, Clone, Default)]
pub struct ObjectNode {
    pub fields: Vec<(u32, Vec<Node>)>,
    pub table_order: Vec<u32>,
}

impl ObjectNode {
    pub fn new(fields: Vec<(u32, Vec<Node>)>) -> Self {
        Self {
            fields,
            table_order: Vec::new(),
        }
    }

    fn table(&self, starts: &HashMap<u32, usize>, out: &mut Vec<u8>) {
        let order: Vec<u32> = if self.table_order.is_empty() {
            self.fields.iter().map(|(number, _)| *number).collect()
        } else {
            self.table_order.clone()
        };
        for number in order {
            out.extend(number.to_le_bytes());
            out.extend((starts[&number] as u32).to_le_bytes());
        }
    }

    /// Version 1000: field count, table, data; offsets from the count.
    fn encode_sized(&self) -> Vec<u8> {
        let header = 4 + 8 * self.fields.len();
        let mut data = Vec::new();
        let mut starts = HashMap::new();
        for (number, nodes) in &self.fields {
            starts.insert(*number, header + data.len());
            for node in nodes {
                match node {
                    Node::Bytes(bytes) => data.extend(bytes),
                    Node::Object(object) => {
                        let body = object.encode_sized();
                        data.extend((body.len() as u32).to_le_bytes());
                        data.extend(body);
                    }
                }
            }
        }
        let mut out = (self.fields.len() as u32).to_le_bytes().to_vec();
        self.table(&starts, &mut out);
        out.extend(data);
        out
    }

    /// Version 1002: data at absolute offsets, tables collected as the
    /// objects end.
    fn encode_tabled(&self, out: &mut Vec<u8>, tables: &mut Vec<u8>, count: &mut u32) {
        let start = out.len();
        let mut starts = HashMap::new();
        for (number, nodes) in &self.fields {
            starts.insert(*number, out.len());
            for node in nodes {
                match node {
                    Node::Bytes(bytes) => out.extend(bytes),
                    Node::Object(object) => object.encode_tabled(out, tables, count),
                }
            }
        }
        tables.extend((start as u32).to_le_bytes());
        tables.extend((self.fields.len() as u32).to_le_bytes());
        self.table(&starts, tables);
        *count += 1;
    }
}

/// Writes a file in either version.
pub fn encode(magic: &[u8; 4], version: Version, root: &ObjectNode) -> Vec<u8> {
    let mut out = magic.to_vec();
    out.extend(version.number().to_le_bytes());
    match version {
        Version::V1000 => out.extend(root.encode_sized()),
        Version::V1002 => {
            let mut body = Vec::new();
            let mut tables = Vec::new();
            let mut count = 0;
            root.encode_tabled(&mut body, &mut tables, &mut count);
            let tables_at = body.len() as u32;
            body.extend(count.to_le_bytes());
            body.extend(tables);
            body.extend(tables_at.to_le_bytes());
            out.extend((body.len() as u32).to_le_bytes());
            let mut encoder = flate2::write::ZlibEncoder::new(out, flate2::Compression::default());
            encoder.write_all(&body).expect("writes to memory");
            out = encoder.finish().expect("writes to memory");
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn int(value: u32) -> Node {
        Node::Bytes(value.to_le_bytes().to_vec())
    }

    fn sample() -> ObjectNode {
        let inner = ObjectNode::new(vec![(2, vec![int(7)]), (3, vec![int(8)])]);
        let mut root = ObjectNode::new(vec![
            (1, vec![int(5)]),
            (
                4,
                vec![int(2), Node::Object(inner.clone()), Node::Object(inner)],
            ),
        ]);
        root.table_order = vec![4, 1];
        root
    }

    #[test]
    fn both_versions_read_back() {
        for version in [Version::V1000, Version::V1002] {
            let bytes = encode(b"BOWS", version, &sample());
            let document = Document::open(&bytes, b"BOWS").unwrap();
            assert_eq!(document.version, version);
            let root = document.root().unwrap();
            assert_eq!(document.bytes(root.require(1).unwrap()), 5u32.to_le_bytes());
            let mut list = document.cursor(root.require(4).unwrap());
            assert_eq!(list.u32().unwrap(), 2);
            for _ in 0..2 {
                // Each inner object ends after its 4-byte field 3.
                let inner = list
                    .object(|_, object| Ok(object.require(3)?.start + 4))
                    .unwrap();
                assert_eq!(
                    document.bytes(
                        inner.require(3).unwrap().start..inner.require(3).unwrap().start + 4
                    ),
                    8u32.to_le_bytes()
                );
            }
        }
    }

    #[test]
    fn version_1000_matches_the_old_layout() {
        let bytes = encode(
            b"BOWX",
            Version::V1000,
            &ObjectNode::new(vec![(1, vec![int(9)])]),
        );
        assert_eq!(
            bytes,
            [
                b"BOWX".as_slice(),
                &1000u32.to_le_bytes(),
                &1u32.to_le_bytes(),
                &1u32.to_le_bytes(),
                &12u32.to_le_bytes(),
                &9u32.to_le_bytes()
            ]
            .concat()
        );
    }
}
