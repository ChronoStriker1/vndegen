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
/// `index` is the PC engine's instruction index, which differs from the
/// core's (see `Script::pc_index`).
#[derive(Debug, Clone, PartialEq)]
pub struct Frame {
    pub script: String,
    pub index: usize,
    /// The wait the frame was in when it called on: 0 none, 1 a click
    /// wait, 3 a stop. A load resumes it when the call returns.
    pub wait: u32,
    pub queued: Vec<Tag>,
}

/// The parts of a numbered save the core restores.
#[derive(Debug, Clone, PartialEq)]
pub struct SlotSave {
    /// Where the game continues after a load: in the save routine the game
    /// called, right after the call that saved.
    pub position: Frame,
    /// Outermost first. A frame's queued tags run when the routine returns
    /// to it, after a click: the save was made at that click wait.
    pub call_stack: Vec<Frame>,
    pub locals: Variables,
    /// Tags that recreate the layers, handlers, and music, in replay order.
    pub replay: Vec<Tag>,
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
        self.field(number)
            .ok_or_else(|| format!("no field {number}"))
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
        wait: object.field(1).map(int).transpose()?.unwrap_or(0) as u32,
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

/// A message layer's font as the PC engine keeps it (field 25).
#[derive(Debug, Clone, PartialEq)]
pub struct FontLayout {
    pub face: String,
    pub ruby_face: String,
    pub left: i32,
    pub top: i32,
    pub width: i32,
    pub height: i32,
    pub size: u8,
    pub ruby_size: u8,
    pub space_top: i32,
    pub space_middle: i32,
    pub space_bottom: i32,
    /// `0xRRGGBB`.
    pub color: u32,
    pub shadow_color: u32,
    pub outline_color: u32,
    /// 0 left, 1 center, 2 right.
    pub align: i32,
    /// Bits: 1 bold, 2 outline, 4 shadow.
    pub style: i32,
    pub hung: bool,
    pub kerning: i32,
}

impl Default for FontLayout {
    /// The engine's font for a message layer no `[font]` has set.
    fn default() -> Self {
        Self {
            face: String::new(),
            ruby_face: String::new(),
            left: 350,
            top: 250,
            width: 700,
            height: 500,
            size: 24,
            ruby_size: 14,
            space_top: -4,
            space_middle: -6,
            space_bottom: 0,
            color: 0xFF_FFFF,
            shadow_color: 0,
            outline_color: 0,
            align: 0,
            style: 6,
            hung: true,
            kerning: 0,
        }
    }
}

/// Everything a numbered save the PC engine loads holds, from the core.
#[derive(Debug, Clone, Default)]
pub struct SlotWrite {
    /// Where the save was made, inside the game's save routine.
    pub position: WriteFrame,
    /// Outermost first. The innermost frame is where the story continues.
    pub call_stack: Vec<WriteFrame>,
    pub locals: Variables,
    /// Tags that recreate each layer, by layer id.
    pub layers: Vec<(String, Vec<Tag>)>,
    /// Each message layer's tags this page, its text among them.
    pub message_layers: Vec<(String, Vec<Tag>)>,
    pub fonts: Vec<(String, FontLayout)>,
    pub push_handlers: Vec<Tag>,
    pub dirchg: Option<Tag>,
    pub bgm: Option<Tag>,
    pub current_message_layer: String,
    pub macro_files: Vec<String>,
    pub key_click_start: String,
    pub key_click_end: String,
    pub menu_script: String,
    pub backlog_layer: String,
    pub on_save: String,
    pub on_load: String,
}

#[derive(Debug, Clone, Default)]
pub struct WriteFrame {
    pub script: String,
    pub index: usize,
    /// As in [`Frame`].
    pub wait: u32,
    pub queued: Vec<Tag>,
    /// The instruction at `index`, with its source line.
    pub at: Option<Tag>,
}

/// The root fields in the order of the PC engine's table.
const TABLE_ORDER: [u32; 68] = [
    31, 64, 30, 29, 28, 53, 26, 76, 75, 67, 66, 25, 24, 70, 69, 50, 38, 34, 33, 49, 48, 47, 46, 45,
    44, 43, 42, 41, 40, 37, 17, 52, 16, 65, 12, 11, 5, 58, 4, 57, 15, 68, 39, 62, 9, 61, 8, 36, 60,
    7, 51, 56, 3, 13, 18, 71, 35, 59, 6, 74, 63, 20, 73, 2, 72, 19, 14, 1,
];

struct Writer {
    encoding: &'static Encoding,
}

impl Writer {
    fn int(&self, value: i32) -> Vec<u8> {
        value.to_le_bytes().to_vec()
    }

    fn text(&self, text: &str) -> Vec<u8> {
        let bytes = self.encoding.encode(text).0;
        [self.int(bytes.len() as i32), bytes.into_owned()].concat()
    }

    fn texts(&self, texts: &[String]) -> Vec<u8> {
        let mut out = self.int(texts.len() as i32);
        for text in texts {
            out.extend(self.text(text));
        }
        out
    }

    /// An object body: the field count, the table, and the fields.
    fn object(&self, fields: Vec<(u32, Vec<u8>)>) -> Vec<u8> {
        let mut table = self.int(fields.len() as i32);
        let start = 4 + 8 * fields.len();
        let mut data = Vec::new();
        for (number, bytes) in fields {
            table.extend(number.to_le_bytes());
            table.extend(((start + data.len()) as u32).to_le_bytes());
            data.extend(bytes);
        }
        [table, data].concat()
    }

    fn sized(&self, body: Vec<u8>) -> Vec<u8> {
        [self.int(body.len() as i32), body].concat()
    }

    fn tag(&self, tag: Option<&Tag>) -> Vec<u8> {
        let empty = Tag {
            name: String::new(),
            params: Vec::new(),
            line: 0,
        };
        let tag = tag.unwrap_or(&empty);
        let mut params = self.int(tag.params.len() as i32);
        for (key, value) in &tag.params {
            params.extend(self.text(key));
            params.extend(self.text(value));
        }
        let body = self.object(vec![
            (2, self.int(tag.line)),
            (3, self.text(&tag.name)),
            (4, params),
        ]);
        self.sized(body)
    }

    fn tags(&self, tags: &[Tag]) -> Vec<u8> {
        let mut out = self.int(tags.len() as i32);
        for tag in tags {
            out.extend(self.tag(Some(tag)));
        }
        out
    }

    fn keyed_tags(&self, entries: &[(String, Vec<Tag>)]) -> Vec<u8> {
        let mut out = self.int(entries.len() as i32);
        for (key, tags) in entries {
            out.extend(self.text(key));
            out.extend(self.tags(tags));
        }
        out
    }

    fn frame(&self, frame: &WriteFrame) -> Vec<u8> {
        let body = self.object(vec![
            (1, self.int(frame.wait as i32)),
            (2, self.int(0)),
            (3, self.text(&frame.script)),
            (4, self.int(frame.index as i32)),
            (6, self.tags(&frame.queued)),
            (7, vec![0]),
            (8, self.tag(frame.at.as_ref())),
        ]);
        self.sized(body)
    }

    fn font(&self, font: &FontLayout) -> Vec<u8> {
        let mut fields = vec![
            (1, self.text(&font.face)),
            (2, self.text(&font.ruby_face)),
            (3, self.int(font.left)),
            (4, self.int(font.top)),
            (5, self.int(font.width)),
            (6, self.int(font.height)),
            (7, self.int(0)),
            (8, self.int(0)),
            (9, vec![1]),
            (10, self.int(255)),
            (11, self.int(65536)),
            (12, self.int(65536)),
            (13, self.int(0)),
            (14, self.int(0)),
            (15, vec![1]),
            (16, self.int(255)),
            (17, self.int(65536)),
            (18, self.int(65536)),
            (19, self.int(0)),
            (20, vec![font.size]),
            (21, vec![font.ruby_size]),
            (22, self.int(font.space_top)),
            (23, self.int(font.space_middle)),
            (24, self.int(font.space_bottom)),
            (25, self.int(font.color as i32)),
            (26, self.int(font.shadow_color as i32)),
            (27, self.int(font.outline_color as i32)),
            (28, self.int(font.align)),
            (29, self.int(font.style)),
            (30, vec![0]),
            (31, vec![u8::from(font.hung)]),
            (33, self.int(font.kerning)),
            (34, self.int(0)),
        ];
        fields.sort_by_key(|(number, _)| *number);
        self.sized(self.object(fields))
    }
}

/// Writes a numbered save the PC engine loads.
pub fn write(save: &SlotWrite, encoding: &'static Encoding) -> Vec<u8> {
    let w = Writer { encoding };
    let empty_list = w.int(0);
    let mut variables = w.int(save.locals.len() as i32);
    for (name, value) in &save.locals {
        variables.extend(w.text(name));
        let bytes = super::pc::variable_bytes(value, encoding);
        variables.extend(w.int(bytes.len() as i32));
        variables.extend(bytes);
    }
    let macro_scope = w.sized(w.object(vec![(1, w.int(0)), (2, w.int(0)), (3, w.int(0))]));
    let variables = w.sized(w.object(vec![
        (1, w.text("'")),
        (2, variables),
        (3, [w.int(1), macro_scope].concat()),
    ]));
    let mut stack = w.int(save.call_stack.len() as i32);
    for frame in &save.call_stack {
        stack.extend(w.frame(frame));
    }
    let mut fonts = w.int(save.fonts.len() as i32);
    for (id, font) in &save.fonts {
        fonts.extend(w.text(id));
        fonts.extend(w.int(1));
        fonts.extend(w.font(font));
    }
    let mut push = w.int(save.push_handlers.len() as i32);
    for tag in &save.push_handlers {
        push.extend(w.int(0));
        push.extend(w.tag(Some(tag)));
    }

    let field = |number: u32| -> Vec<u8> {
        match number {
            1 | 2 | 6 | 28 | 57 | 58 | 61 | 71 => vec![1],
            7 | 15 | 17 | 52 | 68 | 72 | 73 | 74 => vec![0],
            5 => w.int(1),
            12 => [w.int(1), w.int(0)].concat(),
            3 => w.frame(&save.position),
            4 => stack.clone(),
            11 => w.text(&save.current_message_layer),
            24 => w.keyed_tags(&save.message_layers),
            25 => fonts.clone(),
            26 => w.keyed_tags(&save.layers),
            29 => w.tag(save.bgm.as_ref()),
            30 => variables.clone(),
            33 => w.tag(save.dirchg.as_ref()),
            36 => w.texts(&save.macro_files),
            38 => w.text(&save.key_click_start),
            50 => w.text(&save.key_click_end),
            51 => w.text(&save.menu_script),
            40..=49 => w.tag(None),
            53 => empty_list.clone(),
            56 => push.clone(),
            62 => w.text(&save.backlog_layer),
            69 => w.text(&save.on_save),
            70 => w.text(&save.on_load),
            // Message window settings the engine rebuilds from the game's
            // scripts, which load fine empty, and fields that are zero in a
            // fresh save.
            _ => empty_list.clone(),
        }
    };
    let mut sorted = TABLE_ORDER;
    sorted.sort_unstable();
    let mut starts = std::collections::HashMap::new();
    let mut data = Vec::new();
    let table_end = 12 + 8 * TABLE_ORDER.len();
    for number in sorted {
        starts.insert(number, table_end + data.len());
        data.extend(field(number));
    }
    let mut out = MAGIC.to_vec();
    out.extend(VERSION.to_le_bytes());
    out.extend((TABLE_ORDER.len() as u32).to_le_bytes());
    for number in TABLE_ORDER {
        out.extend(number.to_le_bytes());
        out.extend(((starts[&number] - 8) as u32).to_le_bytes());
    }
    out.extend(data);
    out
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
        object(&[
            (2, line.to_le_bytes().to_vec()),
            (3, string(name)),
            (4, list),
        ])
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
        let blob = [
            1u8, 0, 0, 0, 1, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        ];
        let mut variables_list = int(2);
        variables_list.extend(string("lt.day"));
        variables_list.extend(string("1"));
        variables_list.extend(string("lua.scr"));
        variables_list.extend([int(blob.len() as u32), blob.to_vec()].concat());
        let variables = object(&[(1, int(0)), (2, variables_list)]);

        let mut layers = int(1);
        layers.extend(string("1.0"));
        layers.extend(int(2));
        layers.extend(sized(tag_object(
            "lyc",
            &[("id", "1.0"), ("file", "背景")],
            -1,
        )));
        layers.extend(sized(tag_object(
            "lyprop",
            &[("id", "1.0"), ("left", "-400")],
            -1,
        )));

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
        assert_eq!(
            save.locals[0],
            ("lt.day".to_string(), asb_interpreter::Value::Int(1))
        );
        assert_eq!(
            asb_interpreter::pluto::from_variable(&save.locals[1].1.as_string()),
            Some(blob.to_vec())
        );
        let names: Vec<_> = save.replay.iter().map(|tag| tag.name.as_str()).collect();
        assert_eq!(
            names,
            ["lyc", "lyprop", "chgmsg", "print", "/chgmsg", "splay"]
        );
        assert_eq!(save.call_stack[0].script, "scenario/a.txt");
        assert_eq!(
            save.replay[0].params[1],
            ("file".to_string(), "背景".to_string())
        );
    }

    #[test]
    fn a_written_save_reads_back() {
        let tag = |name: &str, params: &[(&str, &str)]| Tag {
            name: name.into(),
            params: params
                .iter()
                .map(|(k, v)| (k.to_string(), v.to_string()))
                .collect(),
            line: -1,
        };
        let save = SlotWrite {
            position: WriteFrame {
                script: "system/system.iet".into(),
                index: 73,
                ..Default::default()
            },
            call_stack: vec![
                WriteFrame {
                    script: "scenario/main.txt".into(),
                    index: 20,
                    ..Default::default()
                },
                WriteFrame {
                    script: "scenario/a.txt".into(),
                    index: 17,
                    wait: 1,
                    queued: vec![tag("calllua", &[("function", "clear")])],
                    at: Some(Tag {
                        name: "行頭".into(),
                        params: Vec::new(),
                        line: 21,
                    }),
                },
            ],
            locals: vec![("lt.day".into(), asb_interpreter::Value::Int(1))],
            layers: vec![(
                "0".into(),
                vec![tag("lyc", &[("id", "0"), ("file", "背景")])],
            )],
            message_layers: vec![("adv".into(), vec![tag("print", &[("data", "本文")])])],
            fonts: vec![("adv".into(), FontLayout::default())],
            bgm: Some(tag("splay", &[("file", "bgm/01")])),
            current_message_layer: "adv".into(),
            ..Default::default()
        };
        let read_back = read(&write(&save, SHIFT_JIS), SHIFT_JIS).unwrap();
        assert_eq!(read_back.position.index, 73);
        assert_eq!(read_back.call_stack[1].script, "scenario/a.txt");
        assert_eq!(read_back.call_stack[1].queued[0].name, "calllua");
        assert_eq!(
            (read_back.call_stack[0].wait, read_back.call_stack[1].wait),
            (0, 1)
        );
        assert_eq!(read_back.locals, save.locals);
        let names: Vec<_> = read_back
            .replay
            .iter()
            .map(|tag| tag.name.as_str())
            .collect();
        assert_eq!(names, ["lyc", "chgmsg", "print", "/chgmsg", "splay"]);
    }

    #[test]
    fn rejects_other_files() {
        assert!(read(b"{}", SHIFT_JIS).is_err());
        assert!(read(b"BOWS\xe8\x03\x00\x00", SHIFT_JIS).is_err());
    }
}
