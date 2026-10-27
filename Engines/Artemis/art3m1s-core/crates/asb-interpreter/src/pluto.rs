//! Pluto, the Lua 5.1 persistence format of the PC Artemis engine (VNDegen).
//!
//! Games keep their Lua tables in Artemis variables through
//! `pluto.persist(perms, table)` and `pluto.unpersist(perms, data)`, and the
//! PC engine writes those variables into its saves as Pluto data. Reading and
//! writing the same bytes lets a save move between the PC version and VNDegen.
//!
//! Every object is an `int firstTime` and an `int ref`. A nil is `0 0`, an
//! object seen before is `0 ref`, and a new object is `1 ref type payload`,
//! where `ref` counts the new non-nil objects written so far. Payloads:
//! boolean `int`, number `double`, string `int length` and its bytes, and
//! table `int 0` (not special), its metatable object, key and value objects,
//! and a nil to end them. Integers are little-endian 32-bit, as the 32-bit
//! Windows engine writes them. Strings are in the game's charset. Functions,
//! threads and userdata never appear in the saves this was checked against,
//! so they are left out on write and rejected on read.

use encoding_rs::Encoding;
use mlua::{Lua, Table, Value};
use std::collections::HashMap;

const TYPE_BOOLEAN: i32 = 1;
const TYPE_NUMBER: i32 = 3;
const TYPE_STRING: i32 = 4;
const TYPE_TABLE: i32 = 5;

/// Prefix of the text form a Pluto blob takes inside an Artemis variable,
/// which holds UTF-8 text. Save files store the bytes themselves.
pub const VARIABLE_PREFIX: &str = "\u{1}pluto:";

/// The text form of `bytes` for an Artemis variable.
pub fn to_variable(bytes: &[u8]) -> String {
    let mut text = String::with_capacity(VARIABLE_PREFIX.len() + bytes.len() * 4 / 3 + 4);
    text.push_str(VARIABLE_PREFIX);
    base64_encode(bytes, &mut text);
    text
}

/// The bytes of a variable that holds a Pluto blob, or `None` for any other value.
pub fn from_variable(text: &str) -> Option<Vec<u8>> {
    base64_decode(text.strip_prefix(VARIABLE_PREFIX)?)
}

/// Writes `value` in Pluto's format.
pub fn persist(value: &Value, encoding: &'static Encoding) -> mlua::Result<Vec<u8>> {
    let mut writer = Writer {
        out: Vec::new(),
        refs: HashMap::new(),
        counter: 0,
        encoding,
    };
    writer.object(value)?;
    Ok(writer.out)
}

/// Reads one Pluto object from `bytes`.
pub fn unpersist(lua: &Lua, bytes: &[u8], encoding: &'static Encoding) -> mlua::Result<Value> {
    let mut reader = Reader {
        lua,
        bytes,
        at: 0,
        refs: HashMap::new(),
        counter: 0,
        encoding,
    };
    let value = reader.object()?;
    if reader.at != bytes.len() {
        return Err(error(format!(
            "{} bytes left after the root object",
            bytes.len() - reader.at
        )));
    }
    Ok(value)
}

fn error(message: String) -> mlua::Error {
    mlua::Error::external(format!("pluto: {message}"))
}

struct Writer {
    out: Vec<u8>,
    /// Tables already written, by identity, with their refs.
    refs: HashMap<usize, i32>,
    counter: i32,
    encoding: &'static Encoding,
}

impl Writer {
    fn int(&mut self, value: i32) {
        self.out.extend_from_slice(&value.to_le_bytes());
    }

    fn first_time(&mut self, kind: i32) -> i32 {
        self.counter += 1;
        self.int(1);
        self.int(self.counter);
        self.int(kind);
        self.counter
    }

    /// Writes a reference if `pointer` was written before, and records it otherwise.
    fn seen(&mut self, pointer: usize) -> bool {
        if let Some(&reference) = self.refs.get(&pointer) {
            self.int(0);
            self.int(reference);
            return true;
        }
        self.refs.insert(pointer, self.counter + 1);
        false
    }

    fn object(&mut self, value: &Value) -> mlua::Result<()> {
        match value {
            Value::Boolean(value) => {
                self.first_time(TYPE_BOOLEAN);
                self.int(i32::from(*value));
            }
            Value::Integer(value) => self.number(*value as f64),
            Value::Number(value) => self.number(*value),
            // Every string is written whole. The PC engine refers back to a
            // repeated string, but a reader takes either form.
            Value::String(value) => {
                self.first_time(TYPE_STRING);
                let bytes = value.as_bytes();
                let encoded = match std::str::from_utf8(&bytes) {
                    Ok(text) => self.encoding.encode(text).0.into_owned(),
                    Err(_) => bytes.to_vec(),
                };
                self.int(encoded.len() as i32);
                self.out.extend_from_slice(&encoded);
            }
            Value::Table(table) => {
                if !self.seen(table.to_pointer() as usize) {
                    self.first_time(TYPE_TABLE);
                    self.table(table)?;
                }
            }
            // Nil, and anything Pluto data from a save can't hold.
            _ => {
                self.int(0);
                self.int(0);
            }
        }
        Ok(())
    }

    fn number(&mut self, value: f64) {
        self.first_time(TYPE_NUMBER);
        self.out.extend_from_slice(&value.to_le_bytes());
    }

    fn table(&mut self, table: &Table) -> mlua::Result<()> {
        self.int(0); // not special
        match table.metatable() {
            Some(metatable) => self.object(&Value::Table(metatable))?,
            None => self.object(&Value::Nil)?,
        }
        for pair in table.clone().pairs::<Value, Value>() {
            let (key, value) = pair?;
            if !persistable(&key) || !persistable(&value) {
                continue;
            }
            self.object(&key)?;
            self.object(&value)?;
        }
        self.object(&Value::Nil)
    }
}

fn persistable(value: &Value) -> bool {
    matches!(
        value,
        Value::Boolean(_)
            | Value::Integer(_)
            | Value::Number(_)
            | Value::String(_)
            | Value::Table(_)
    )
}

struct Reader<'a> {
    lua: &'a Lua,
    bytes: &'a [u8],
    at: usize,
    refs: HashMap<i32, Value>,
    counter: i32,
    encoding: &'static Encoding,
}

impl<'a> Reader<'a> {
    fn take(&mut self, len: usize) -> mlua::Result<&'a [u8]> {
        let all: &'a [u8] = self.bytes;
        let end = self.at.checked_add(len).filter(|end| *end <= all.len());
        let end = end.ok_or_else(|| error(format!("data ends at byte {}", all.len())))?;
        let bytes = &all[self.at..end];
        self.at = end;
        Ok(bytes)
    }

    fn int(&mut self) -> mlua::Result<i32> {
        Ok(i32::from_le_bytes(self.take(4)?.try_into().unwrap()))
    }

    fn object(&mut self) -> mlua::Result<Value> {
        let first_time = self.int()?;
        let reference = self.int()?;
        if first_time == 0 {
            if reference == 0 {
                return Ok(Value::Nil);
            }
            return self
                .refs
                .get(&reference)
                .cloned()
                .ok_or_else(|| error(format!("reference {reference} before its object")));
        }
        self.counter += 1;
        if reference != self.counter {
            return Err(error(format!(
                "ref {reference} where {} was due",
                self.counter
            )));
        }
        let value = match self.int()? {
            TYPE_BOOLEAN => Value::Boolean(self.int()? != 0),
            TYPE_NUMBER => Value::Number(f64::from_le_bytes(self.take(8)?.try_into().unwrap())),
            TYPE_STRING => {
                let len =
                    usize::try_from(self.int()?).map_err(|_| error("negative length".into()))?;
                let bytes = self.take(len)?;
                let text = self.encoding.decode_without_bom_handling(bytes).0;
                Value::String(self.lua.create_string(text.as_bytes())?)
            }
            TYPE_TABLE => {
                if self.int()? != 0 {
                    return Err(error("a table persisted through __persist".into()));
                }
                let table = self.lua.create_table()?;
                // Register before the contents, which may refer back to it.
                self.refs.insert(reference, Value::Table(table.clone()));
                if let Value::Table(metatable) = self.object()? {
                    table.set_metatable(Some(metatable));
                }
                loop {
                    let key = self.object()?;
                    if key.is_nil() {
                        break;
                    }
                    let value = self.object()?;
                    table.raw_set(key, value)?;
                }
                Value::Table(table)
            }
            kind => return Err(error(format!("unsupported object type {kind}"))),
        };
        self.refs.insert(reference, value.clone());
        Ok(value)
    }
}

const BASE64: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

fn base64_encode(bytes: &[u8], out: &mut String) {
    for chunk in bytes.chunks(3) {
        let b = [
            chunk[0],
            *chunk.get(1).unwrap_or(&0),
            *chunk.get(2).unwrap_or(&0),
        ];
        let n = u32::from(b[0]) << 16 | u32::from(b[1]) << 8 | u32::from(b[2]);
        for i in 0..4 {
            if i <= chunk.len() {
                out.push(BASE64[(n >> (18 - 6 * i) & 63) as usize] as char);
            } else {
                out.push('=');
            }
        }
    }
}

fn base64_decode(text: &str) -> Option<Vec<u8>> {
    let mut out = Vec::with_capacity(text.len() * 3 / 4);
    let mut n = 0u32;
    let mut bits = 0;
    for c in text.bytes().filter(|&c| c != b'=') {
        n = n << 6 | BASE64.iter().position(|&b| b == c)? as u32;
        bits += 6;
        if bits >= 8 {
            bits -= 8;
            out.push((n >> bits) as u8);
        }
    }
    Some(out)
}

#[cfg(test)]
mod tests {
    use super::*;
    use encoding_rs::{SHIFT_JIS, UTF_8};

    /// Pluto bytes as the PC engine writes them: `{ "adv" = { flag = true },
    /// [1] = 2.5, name = "行頭", again = <the adv table> }` minus key order.
    fn int(value: i32) -> Vec<u8> {
        value.to_le_bytes().to_vec()
    }

    #[test]
    fn reads_and_writes_what_the_pc_engine_writes() {
        let (name, _, _) = SHIFT_JIS.encode("行頭");
        let mut bytes = Vec::new();
        bytes.extend([int(1), int(1), int(5), int(0), int(0), int(0)].concat()); // root table
        bytes.extend([int(1), int(2), int(4), int(3)].concat()); // "adv"
        bytes.extend(b"adv");
        bytes.extend([int(1), int(3), int(5), int(0), int(0), int(0)].concat()); // adv table
        bytes.extend([int(1), int(4), int(4), int(4)].concat()); // "flag"
        bytes.extend(b"flag");
        bytes.extend([int(1), int(5), int(1), int(1)].concat()); // true
        bytes.extend([int(0), int(0)].concat()); // end of adv
        bytes.extend([int(1), int(6), int(4), int(4)].concat()); // "name"
        bytes.extend(b"name");
        bytes.extend([int(1), int(7), int(4), int(name.len() as i32)].concat());
        bytes.extend(name.iter());
        bytes.extend([int(1), int(8), int(4), int(5)].concat()); // "again"
        bytes.extend(b"again");
        bytes.extend([int(0), int(3)].concat()); // reference to adv
        bytes.extend([int(0), int(0)].concat()); // end of root

        let lua = Lua::new();
        let root = unpersist(&lua, &bytes, SHIFT_JIS).unwrap();
        let Value::Table(root) = root else { panic!() };
        let adv: Table = root.get("adv").unwrap();
        assert!(adv.get::<bool>("flag").unwrap());
        assert_eq!(root.get::<String>("name").unwrap(), "行頭");
        assert_eq!(
            root.get::<Table>("again").unwrap().to_pointer(),
            adv.to_pointer()
        );

        // Writing it back gives an object the reader takes again, with the
        // shared table still shared and the string back in Shift_JIS.
        let written = persist(&Value::Table(root), SHIFT_JIS).unwrap();
        assert!(written.windows(name.len()).any(|w| w == &name[..]));
        let again = unpersist(&lua, &written, SHIFT_JIS).unwrap();
        let Value::Table(again) = again else { panic!() };
        assert_eq!(
            again.get::<Table>("again").unwrap().to_pointer(),
            again.get::<Table>("adv").unwrap().to_pointer()
        );
        assert_eq!(again.get::<String>("name").unwrap(), "行頭");
    }

    #[test]
    fn numbers_keys_and_nil_take_the_pc_reference_numbers() {
        let lua = Lua::new();
        let table = lua.create_table().unwrap();
        table.raw_set(1, 2.5).unwrap();
        let written = persist(&Value::Table(table), UTF_8).unwrap();
        let expected = [
            int(1),
            int(1),
            int(5),
            int(0),
            int(0),
            int(0), // table, no metatable
            int(1),
            int(2),
            int(3),
            1.0f64.to_le_bytes().to_vec(),
            int(1),
            int(3),
            int(3),
            2.5f64.to_le_bytes().to_vec(),
            int(0),
            int(0),
        ]
        .concat();
        assert_eq!(written, expected);
    }

    #[test]
    fn variable_form_round_trips_any_bytes() {
        for bytes in [
            vec![],
            vec![0],
            vec![0, 255],
            (0..=255).collect::<Vec<u8>>(),
        ] {
            let text = to_variable(&bytes);
            assert!(text.is_ascii() || text.starts_with(VARIABLE_PREFIX));
            assert_eq!(from_variable(&text), Some(bytes));
        }
        assert_eq!(from_variable("plain"), None);
    }

    #[test]
    fn rejects_truncated_data() {
        let lua = Lua::new();
        assert!(unpersist(&lua, &[1, 0, 0, 0, 1, 0], UTF_8).is_err());
    }
}
