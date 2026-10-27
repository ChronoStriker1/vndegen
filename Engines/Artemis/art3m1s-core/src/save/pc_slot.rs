//! Numbered and quick saves of the PC Artemis engine, magic `BOWS` (VNDegen).
//!
//! `docs/artemis-save-format.md` in VNDegen describes the format. A save is
//! a root object of numbered fields. The engine keeps most of its state as
//! the tags that recreate it, such as an `lyc` and an `lyprop` for each
//! layer, so a load replays those tags through the interpreter.

use super::pc::{Reader, Variables};
use encoding_rs::Encoding;

pub const MAGIC: &[u8; 4] = b"BOWS";
const VERSION: u32 = 1000;

/// A tag as the engine stores it, with its parameters in order.
#[derive(Debug, Clone, PartialEq)]
pub struct Tag {
    pub name: String,
    pub params: Vec<(String, String)>,
    pub line: i32,
}

/// A script position: where execution continues, and the tags queued there.
#[derive(Debug, Clone, PartialEq)]
pub struct Frame {
    pub script: String,
    pub index: usize,
    pub queued: Vec<Tag>,
}

/// The parts of a numbered save the core restores.
#[derive(Debug, Clone, PartialEq)]
pub struct SlotSave {
    pub position: Frame,
    /// Outermost first.
    pub call_stack: Vec<Frame>,
    pub locals: Variables,
    /// Tags that recreate the layers, handlers, and music, in replay order.
    pub replay: Vec<Tag>,
}

impl SlotSave {
    /// Where the game continues after a load. The stored position is inside
    /// the save routine the game called. The PC engine continues in the
    /// innermost frame of the call stack, where the story was, and runs the
    /// tags queued there.
    pub fn resume(&self) -> &Frame {
        self.call_stack.last().unwrap_or(&self.position)
    }
}

/// An object's fields, by number. `body` starts at the field count.
struct Object<'a> {
    fields: Vec<(u32, &'a [u8])>,
}

impl<'a> Object<'a> {
    fn parse(body: &'a [u8]) -> Result<Self, String> {
        let mut reader = Reader::new(body);
        let count = reader.u32()? as usize;
        let mut table = Vec::with_capacity(count.min(128));
        for _ in 0..count {
            let field = reader.u32()?;
            let offset = reader.u32()? as usize;
            if offset > body.len() {
                return Err(format!("field {field} past the end of its object"));
            }
            table.push((field, offset));
        }
        let mut starts: Vec<usize> = table.iter().map(|(_, offset)| *offset).collect();
        starts.sort_unstable();
        let fields = table
            .into_iter()
            .map(|(field, start)| {
                let end = starts
                    .iter()
                    .copied()
                    .find(|&next| next > start)
                    .unwrap_or(body.len());
                (field, &body[start..end])
            })
            .collect();
        Ok(Self { fields })
    }

    /// An object nested with its length in front.
    fn sized(bytes: &'a [u8]) -> Result<Self, String> {
        let mut reader = Reader::new(bytes);
        let len = reader.u32()? as usize;
        Self::parse(reader.take(len)?)
    }

    fn field(&self, number: u32) -> Option<&'a [u8]> {
        self.fields
            .iter()
            .find(|(candidate, _)| *candidate == number)
            .map(|(_, data)| *data)
    }

    fn require(&self, number: u32) -> Result<&'a [u8], String> {
        self.field(number).ok_or_else(|| format!("no field {number}"))
    }
}

fn int(bytes: &[u8]) -> Result<i32, String> {
    Ok(i32::from_le_bytes(
        bytes
            .get(..4)
            .ok_or("an int field is short")?
            .try_into()
            .unwrap(),
    ))
}

fn text(bytes: &[u8], encoding: &'static Encoding) -> Result<String, String> {
    let mut reader = Reader::new(bytes);
    Ok(encoding.decode(reader.string()?).0.into_owned())
}

/// A tag object. An empty tag has no name.
fn tag(object: &Object, encoding: &'static Encoding) -> Result<Tag, String> {
    let name = text(object.require(3)?, encoding)?;
    let mut reader = Reader::new(object.require(4)?);
    let count = reader.u32()?;
    let mut params = Vec::new();
    for _ in 0..count {
        let key = encoding.decode(reader.string()?).0.into_owned();
        let value = encoding.decode(reader.string()?).0.into_owned();
        params.push((key, value));
    }
    Ok(Tag {
        name,
        params,
        line: int(object.require(2)?)?,
    })
}

/// Tags written with their lengths, one after another, `count` of them.
fn tags(reader: &mut Reader, count: u32, encoding: &'static Encoding) -> Result<Vec<Tag>, String> {
    let mut out = Vec::new();
    for _ in 0..count {
        let len = reader.u32()? as usize;
        let tag = tag(&Object::parse(reader.take(len)?)?, encoding)?;
        if !tag.name.is_empty() {
            out.push(tag);
        }
    }
    Ok(out)
}

fn frame(object: &Object, encoding: &'static Encoding) -> Result<Frame, String> {
    let queued = match object.field(6) {
        Some(data) if data.len() > 4 => {
            let mut reader = Reader::new(data);
            let count = reader.u32()?;
            tags(&mut reader, count, encoding)?
        }
        _ => Vec::new(),
    };
    Ok(Frame {
        script: text(object.require(3)?, encoding)?,
        index: usize::try_from(int(object.require(4)?)?).map_err(|_| "a negative index")?,
        queued,
    })
}

/// A tag list: a count, then for each a key, a count, and that many tags.
fn keyed_tags(data: &[u8], encoding: &'static Encoding) -> Result<Vec<(String, Vec<Tag>)>, String> {
    let mut reader = Reader::new(data);
    let count = reader.u32()?;
    let mut out = Vec::new();
    for _ in 0..count {
        let key = encoding.decode(reader.string()?).0.into_owned();
        let tag_count = reader.u32()?;
        out.push((key, tags(&mut reader, tag_count, encoding)?));
    }
    Ok(out)
}

/// Reads a numbered save.
pub fn read(bytes: &[u8], encoding: &'static Encoding) -> Result<SlotSave, String> {
    let mut reader = Reader::new(bytes);
    if reader.take(4)? != MAGIC {
        return Err("not a BOWS save".into());
    }
    let version = reader.u32()?;
    if version != VERSION {
        return Err(format!("save version {version}, expected {VERSION}"));
    }
    let root = Object::parse(&bytes[8..])?;

    let position = frame(&Object::sized(root.require(3)?)?, encoding)?;
    let mut call_stack = Vec::new();
    let mut stack = Reader::new(root.require(4)?);
    for _ in 0..stack.u32()? {
        let len = stack.u32()? as usize;
        call_stack.push(frame(&Object::parse(stack.take(len)?)?, encoding)?);
    }

    let variables = Object::sized(root.require(30)?)?;
    let mut locals = Vec::new();
    let mut entries = Reader::new(variables.require(2)?);
    for _ in 0..entries.u32()? {
        let name = encoding.decode(entries.string()?).0.into_owned();
        let value = super::pc::variable_value(entries.string()?, encoding);
        locals.push((name, value));
    }

    let mut replay = Vec::new();
    for (_, layer) in keyed_tags(root.require(26)?, encoding)? {
        replay.extend(layer);
    }
    // Each message layer's tags since it was last cleared, its visible text
    // among them, run inside that layer.
    for (id, tags) in keyed_tags(root.require(24)?, encoding)? {
        if id.is_empty() || tags.is_empty() {
            continue;
        }
        replay.push(Tag {
            name: "chgmsg".into(),
            params: vec![("id".into(), id)],
            line: -1,
        });
        replay.extend(tags);
        replay.push(Tag {
            name: "/chgmsg".into(),
            params: Vec::new(),
            line: -1,
        });
    }
    if let Some(data) = root.field(56) {
        // Push handlers: a count, then an int key and a tag for each.
        let mut reader = Reader::new(data);
        for _ in 0..reader.u32()? {
            reader.u32()?;
            replay.extend(tags(&mut reader, 1, encoding)?);
        }
    }
    for field in [33, 29] {
        if let Some(data) = root.field(field) {
            let handler = tag(&Object::sized(data)?, encoding)?;
            if !handler.name.is_empty() {
                replay.push(handler);
            }
        }
    }

    Ok(SlotSave {
        position,
        call_stack,
        locals,
        replay,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use encoding_rs::SHIFT_JIS;

    fn int(value: u32) -> Vec<u8> {
        value.to_le_bytes().to_vec()
    }

    fn string(text: &str) -> Vec<u8> {
        let bytes = SHIFT_JIS.encode(text).0.into_owned();
        [int(bytes.len() as u32), bytes].concat()
    }

    /// An object body: the field count, the table, and the fields.
    fn object(fields: &[(u32, Vec<u8>)]) -> Vec<u8> {
        let mut table = int(fields.len() as u32);
        let mut data = Vec::new();
        let start = 4 + 8 * fields.len();
        for (number, bytes) in fields {
            table.extend(int(*number));
            table.extend(int((start + data.len()) as u32));
            data.extend(bytes);
        }
        [table, data].concat()
    }

    fn sized(body: Vec<u8>) -> Vec<u8> {
        [int(body.len() as u32), body].concat()
    }

    fn tag_object(name: &str, params: &[(&str, &str)], line: i32) -> Vec<u8> {
        let mut list = int(params.len() as u32);
        for (key, value) in params {
            list.extend(string(key));
            list.extend(string(value));
        }
        object(&[(2, line.to_le_bytes().to_vec()), (3, string(name)), (4, list)])
    }

    fn frame_object(script: &str, index: u32, queued: &[Vec<u8>]) -> Vec<u8> {
        let mut tags = int(queued.len() as u32);
        for tag in queued {
            tags.extend(sized(tag.clone()));
        }
        object(&[
            (3, string(script)),
            (4, int(index)),
            (6, tags),
            (8, sized(tag_object("", &[], 0))),
        ])
    }

    #[test]
    fn reads_position_stack_variables_and_replay_tags() {
        let blob = [1u8, 0, 0, 0, 1, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
        let mut variables_list = int(2);
        variables_list.extend(string("lt.day"));
        variables_list.extend(string("1"));
        variables_list.extend(string("lua.scr"));
        variables_list.extend([int(blob.len() as u32), blob.to_vec()].concat());
        let variables = object(&[(1, int(0)), (2, variables_list)]);

        let mut layers = int(1);
        layers.extend(string("1.0"));
        layers.extend(int(2));
        layers.extend(sized(tag_object("lyc", &[("id", "1.0"), ("file", "背景")], -1)));
        layers.extend(sized(tag_object("lyprop", &[("id", "1.0"), ("left", "-400")], -1)));

        let mut stack = int(1);
        stack.extend(sized(frame_object(
            "scenario/a.txt",
            17,
            &[tag_object("calllua", &[("function", "clear")], -1)],
        )));

        let mut text = int(2);
        text.extend(string(""));
        text.extend(int(0));
        text.extend(string("adv"));
        text.extend(int(1));
        text.extend(sized(tag_object("print", &[("data", "本文")], -1)));

        let root = object(&[
            (3, sized(frame_object("system/system.iet", 73, &[]))),
            (4, stack),
            (24, text),
            (26, layers),
            (29, sized(tag_object("splay", &[("file", "bgm/01")], -1))),
            (30, sized(variables)),
            (33, sized(tag_object("", &[], 0))),
        ]);
        let bytes = [b"BOWS".to_vec(), int(1000), root].concat();

        let save = read(&bytes, SHIFT_JIS).unwrap();
        assert_eq!(save.position.script, "system/system.iet");
        assert_eq!(save.position.index, 73);
        assert_eq!(save.call_stack.len(), 1);
        assert_eq!(save.call_stack[0].index, 17);
        assert_eq!(save.call_stack[0].queued[0].name, "calllua");
        assert_eq!(save.locals[0], ("lt.day".to_string(), asb_interpreter::Value::Int(1)));
        assert_eq!(
            asb_interpreter::pluto::from_variable(&save.locals[1].1.as_string()),
            Some(blob.to_vec())
        );
        let names: Vec<_> = save.replay.iter().map(|tag| tag.name.as_str()).collect();
        assert_eq!(names, ["lyc", "lyprop", "chgmsg", "print", "/chgmsg", "splay"]);
        assert_eq!(save.resume().script, "scenario/a.txt");
        assert_eq!(save.replay[0].params[1], ("file".to_string(), "背景".to_string()));
    }

    #[test]
    fn rejects_other_files() {
        assert!(read(b"{}", SHIFT_JIS).is_err());
        assert!(read(b"BOWS\xe8\x03\x00\x00", SHIFT_JIS).is_err());
    }
}
