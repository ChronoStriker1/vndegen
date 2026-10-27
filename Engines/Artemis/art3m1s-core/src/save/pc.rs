//! The PC Artemis engine's save files, so saves move between it and VNDegen (VNDegen).
//!
//! Every file is a container: a four-byte magic, the version 1000, a section
//! count, and a table of `(tag, offset)` pairs, where an offset counts from
//! byte 8. A section runs to the next section's start or the end of the file.
//! Integers are little-endian 32-bit, and strings are a length and bytes in
//! the game's charset.
//!
//! - `system.dat`, magic `BOWX`: section 1 holds the system variables as
//!   name and value pairs.
//! - `saveg.dat`, magic `BOWG`: section 1 lists, for each script, the
//!   indexes of the instructions already run, and section 2 holds the global
//!   variables.
//!
//! Variable values are text, except Pluto data (see
//! [`asb_interpreter::pluto`]), which the core keeps in its text form.

use asb_interpreter::Value;
use encoding_rs::Encoding;
use std::collections::HashMap;

pub const SYSTEM_MAGIC: &[u8; 4] = b"BOWX";
pub const GLOBAL_MAGIC: &[u8; 4] = b"BOWG";
const VERSION: u32 = 1000;
const OFFSET_BASE: usize = 8;

/// Sections of a container in the order of its table.
pub type Sections<'a> = Vec<(u32, &'a [u8])>;

/// Reads a container, checking its magic and version.
pub fn read_container<'a>(bytes: &'a [u8], magic: &[u8; 4]) -> Result<Sections<'a>, String> {
    let mut reader = Reader::new(bytes);
    if reader.take(4)? != magic {
        return Err(format!("not a {} file", String::from_utf8_lossy(magic)));
    }
    let version = reader.u32()?;
    if version != VERSION {
        return Err(format!("save version {version}, expected {VERSION}"));
    }
    let count = reader.u32()? as usize;
    let mut table = Vec::with_capacity(count.min(256));
    for _ in 0..count {
        let tag = reader.u32()?;
        let start = (reader.u32()? as usize)
            .checked_add(OFFSET_BASE)
            .filter(|start| *start <= bytes.len())
            .ok_or_else(|| format!("section {tag} starts past the end"))?;
        table.push((tag, start));
    }
    let mut starts: Vec<usize> = table.iter().map(|(_, start)| *start).collect();
    starts.sort_unstable();
    Ok(table
        .into_iter()
        .map(|(tag, start)| {
            let end = starts
                .iter()
                .copied()
                .find(|&next| next > start)
                .unwrap_or(bytes.len());
            (tag, &bytes[start..end])
        })
        .collect())
}

/// Writes a container. The table lists `table_order`, and the sections follow
/// in `sections` order.
pub fn write_container(
    magic: &[u8; 4],
    table_order: &[u32],
    sections: &[(u32, Vec<u8>)],
) -> Vec<u8> {
    let header = 12 + 8 * table_order.len();
    let mut starts = HashMap::new();
    let mut at = header;
    for (tag, data) in sections {
        starts.insert(*tag, at);
        at += data.len();
    }
    let mut out = Vec::with_capacity(at);
    out.extend_from_slice(magic);
    out.extend_from_slice(&VERSION.to_le_bytes());
    out.extend_from_slice(&(table_order.len() as u32).to_le_bytes());
    for tag in table_order {
        out.extend_from_slice(&tag.to_le_bytes());
        out.extend_from_slice(&((starts[tag] - OFFSET_BASE) as u32).to_le_bytes());
    }
    for (_, data) in sections {
        out.extend_from_slice(data);
    }
    out
}

/// The system or global variables, without their `s.` or `g.` prefix.
pub type Variables = Vec<(String, Value)>;
/// Instruction indexes already run, by script.
pub type ReadLines = HashMap<String, Vec<usize>>;

/// Reads `system.dat`.
pub fn read_system(bytes: &[u8], encoding: &'static Encoding) -> Result<Variables, String> {
    let sections = read_container(bytes, SYSTEM_MAGIC)?;
    let section = find(&sections, 1)?;
    read_variables(section, "s.", encoding)
}

/// Writes `system.dat`.
pub fn write_system(variables: &Variables, encoding: &'static Encoding) -> Vec<u8> {
    let section = write_variables(variables, "s.", encoding);
    write_container(SYSTEM_MAGIC, &[1], &[(1, section)])
}

/// Reads `saveg.dat`.
pub fn read_global(
    bytes: &[u8],
    encoding: &'static Encoding,
) -> Result<(Variables, ReadLines), String> {
    let sections = read_container(bytes, GLOBAL_MAGIC)?;
    let read_lines = read_read_lines(find(&sections, 1)?, encoding)?;
    let variables = read_variables(find(&sections, 2)?, "g.", encoding)?;
    Ok((variables, read_lines))
}

/// Writes `saveg.dat`, with the table order the PC engine writes.
pub fn write_global(
    variables: &Variables,
    read_lines: &ReadLines,
    encoding: &'static Encoding,
) -> Vec<u8> {
    let mut lines = Vec::new();
    let mut scripts: Vec<_> = read_lines.iter().collect();
    scripts.sort_by(|a, b| a.0.cmp(b.0));
    put_u32(&mut lines, scripts.len() as u32);
    for (script, indexes) in scripts {
        put_string(&mut lines, &encoding.encode(script).0);
        put_u32(&mut lines, indexes.len() as u32);
        for index in indexes {
            put_u32(&mut lines, *index as u32);
        }
    }
    let globals = write_variables(variables, "g.", encoding);
    write_container(GLOBAL_MAGIC, &[2, 1], &[(1, lines), (2, globals)])
}

fn find<'a>(sections: &Sections<'a>, tag: u32) -> Result<&'a [u8], String> {
    sections
        .iter()
        .find(|(candidate, _)| *candidate == tag)
        .map(|(_, data)| *data)
        .ok_or_else(|| format!("no section {tag}"))
}

fn read_variables(
    section: &[u8],
    prefix: &str,
    encoding: &'static Encoding,
) -> Result<Variables, String> {
    let mut reader = Reader::new(section);
    let count = reader.u32()?;
    let mut variables = Vec::new();
    for _ in 0..count {
        let name = encoding.decode(reader.string()?).0.into_owned();
        let value = variable_value(reader.string()?, encoding);
        let name = name
            .strip_prefix(prefix)
            .map(str::to_string)
            .unwrap_or(name);
        variables.push((name, value));
    }
    Ok(variables)
}

fn write_variables(variables: &Variables, prefix: &str, encoding: &'static Encoding) -> Vec<u8> {
    let mut out = Vec::new();
    put_u32(&mut out, variables.len() as u32);
    for (name, value) in variables {
        put_string(&mut out, &encoding.encode(&format!("{prefix}{name}")).0);
        put_string(&mut out, &variable_bytes(value, encoding));
    }
    out
}

/// A value as the core keeps it. Pluto data holds NUL bytes, which text
/// never does. A whole number becomes an integer, as the core's own
/// variables are.
pub fn variable_value(bytes: &[u8], encoding: &'static Encoding) -> Value {
    if bytes.contains(&0) {
        return Value::String(asb_interpreter::pluto::to_variable(bytes));
    }
    let text = encoding.decode(bytes).0.into_owned();
    match text.parse::<i64>() {
        Ok(number) if number.to_string() == text => Value::Int(number),
        _ => Value::String(text),
    }
}

/// A value's bytes in a PC save.
pub fn variable_bytes(value: &Value, encoding: &'static Encoding) -> Vec<u8> {
    let text = value.as_string();
    asb_interpreter::pluto::from_variable(&text)
        .unwrap_or_else(|| encoding.encode(&text).0.into_owned())
}

fn read_read_lines(section: &[u8], encoding: &'static Encoding) -> Result<ReadLines, String> {
    let mut reader = Reader::new(section);
    let count = reader.u32()?;
    let mut read_lines = HashMap::new();
    for _ in 0..count {
        let script = encoding.decode(reader.string()?).0.into_owned();
        let lines = reader.u32()?;
        let mut indexes = Vec::new();
        for _ in 0..lines {
            indexes.push(reader.u32()? as usize);
        }
        read_lines.insert(script, indexes);
    }
    Ok(read_lines)
}

fn put_u32(out: &mut Vec<u8>, value: u32) {
    out.extend_from_slice(&value.to_le_bytes());
}

fn put_string(out: &mut Vec<u8>, bytes: &[u8]) {
    put_u32(out, bytes.len() as u32);
    out.extend_from_slice(bytes);
}

pub(crate) struct Reader<'a> {
    bytes: &'a [u8],
    at: usize,
}

impl<'a> Reader<'a> {
    pub(crate) fn new(bytes: &'a [u8]) -> Self {
        Self { bytes, at: 0 }
    }

    pub(crate) fn take(&mut self, len: usize) -> Result<&'a [u8], String> {
        let end = self
            .at
            .checked_add(len)
            .filter(|end| *end <= self.bytes.len())
            .ok_or_else(|| format!("save data ends at byte {}", self.bytes.len()))?;
        let bytes = &self.bytes[self.at..end];
        self.at = end;
        Ok(bytes)
    }

    pub(crate) fn u32(&mut self) -> Result<u32, String> {
        Ok(u32::from_le_bytes(self.take(4)?.try_into().unwrap()))
    }

    pub(crate) fn string(&mut self) -> Result<&'a [u8], String> {
        let len = self.u32()? as usize;
        self.take(len)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use encoding_rs::SHIFT_JIS;

    fn string(text: &[u8]) -> Vec<u8> {
        let mut out = (text.len() as u32).to_le_bytes().to_vec();
        out.extend_from_slice(text);
        out
    }

    /// `system.dat` as the PC engine writes it: one section at byte 20.
    fn pc_system() -> Vec<u8> {
        let mut out = b"BOWX".to_vec();
        for value in [1000u32, 1, 1, 12, 2] {
            out.extend_from_slice(&value.to_le_bytes());
        }
        out.extend(string(b"s.bgmvol"));
        out.extend(string(b"500"));
        out.extend(string(b"s.title"));
        out.extend(string(&SHIFT_JIS.encode("監獄").0));
        out
    }

    #[test]
    fn reads_and_writes_system_variables_like_the_pc_engine() {
        let variables = read_system(&pc_system(), SHIFT_JIS).unwrap();
        assert_eq!(
            variables,
            vec![
                ("bgmvol".to_string(), Value::Int(500)),
                ("title".to_string(), Value::String("監獄".to_string())),
            ]
        );
        assert_eq!(write_system(&variables, SHIFT_JIS), pc_system());
    }

    #[test]
    fn reads_and_writes_global_variables_and_read_lines() {
        let blob = [1u8, 0, 0, 0, 1, 0, 0, 0, 5, 0, 0, 0];
        let variables = vec![
            ("endflag".to_string(), Value::Int(0)),
            (
                "lua.scr".to_string(),
                Value::String(asb_interpreter::pluto::to_variable(&blob)),
            ),
        ];
        let read_lines = HashMap::from([("scenario/a.txt".to_string(), vec![0, 1, 2378])]);
        let bytes = write_global(&variables, &read_lines, SHIFT_JIS);
        assert_eq!(&bytes[..4], b"BOWG");
        // Table: section 2 then 1, with section 1's data right after the table.
        assert_eq!(&bytes[12..16], &2u32.to_le_bytes());
        assert_eq!(&bytes[20..28], &[1, 0, 0, 0, 20, 0, 0, 0]);
        assert!(bytes.windows(blob.len()).any(|window| window == blob));
        let (read_variables, read_back) = read_global(&bytes, SHIFT_JIS).unwrap();
        assert_eq!(read_variables, variables);
        assert_eq!(read_back, read_lines);
    }

    #[test]
    fn rejects_other_files() {
        assert!(read_system(b"{\"bgmvol\": 1}", SHIFT_JIS).is_err());
        let mut truncated = pc_system();
        truncated.truncate(30);
        assert!(read_system(&truncated, SHIFT_JIS).is_err());
    }
}
