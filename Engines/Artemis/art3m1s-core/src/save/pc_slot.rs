//! Numbered and quick saves of the PC Artemis engine, magic `BOWS` (VNDegen).
//!
//! `docs/artemis-save-format.md` in VNDegen describes the format. A save is
//! a root object of numbered fields, in the format [`super::bow`] reads, in
//! version 1000 or 1002. The engine keeps most of its state as the tags that
//! recreate it, such as an `lyc` and an `lyprop` for each layer, so a load
//! replays those tags through the interpreter.

use super::bow::{self, Cursor, Document, Node, Object, ObjectNode, Version};
use super::pc::Variables;
use encoding_rs::Encoding;
use std::ops::Range;

pub const MAGIC: &[u8; 4] = b"BOWS";

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
    pub version: Version,
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

fn int(bytes: &[u8]) -> Result<i32, String> {
    Ok(i32::from_le_bytes(
        bytes
            .get(..4)
            .ok_or("an int field is short")?
            .try_into()
            .unwrap(),
    ))
}

fn text(
    document: &Document,
    field: Range<usize>,
    encoding: &'static Encoding,
) -> Result<String, String> {
    Ok(encoding
        .decode(document.cursor(field).string()?)
        .0
        .into_owned())
}

/// Where a tag object ends: after its parameter list, its last field.
fn tag_end(document: &Document, object: &Object) -> Result<usize, String> {
    let mut params = document.cursor(object.require(4)?);
    for _ in 0..params.u32()? {
        params.string()?;
        params.string()?;
    }
    Ok(params.position())
}

/// Where a frame object ends: after its tag, its last field.
fn frame_end(document: &Document, object: &Object) -> Result<usize, String> {
    tag_end(document, &document.object(object.require(8)?)?)
}

/// A tag object. An empty tag has no name.
fn tag(document: &Document, object: &Object, encoding: &'static Encoding) -> Result<Tag, String> {
    let mut reader = document.cursor(object.require(4)?);
    let mut params = Vec::new();
    for _ in 0..reader.u32()? {
        let key = encoding.decode(reader.string()?).0.into_owned();
        let value = encoding.decode(reader.string()?).0.into_owned();
        params.push((key, value));
    }
    Ok(Tag {
        name: text(document, object.require(3)?, encoding)?,
        params,
        line: int(document.bytes(object.require(2)?))?,
    })
}

/// The next `count` tags of a list, without the empty ones.
fn tags(cursor: &mut Cursor, count: u32, encoding: &'static Encoding) -> Result<Vec<Tag>, String> {
    let document = cursor.document();
    let mut out = Vec::new();
    for _ in 0..count {
        let tag = tag(document, &cursor.object(tag_end)?, encoding)?;
        if !tag.name.is_empty() {
            out.push(tag);
        }
    }
    Ok(out)
}

/// The tag a field holds.
fn field_tag(
    document: &Document,
    field: Range<usize>,
    encoding: &'static Encoding,
) -> Result<Tag, String> {
    tag(document, &document.object(field)?, encoding)
}

fn frame(
    document: &Document,
    object: &Object,
    encoding: &'static Encoding,
) -> Result<Frame, String> {
    let queued = match object.field(6) {
        Some(field) => {
            let mut list = document.cursor(field);
            let count = list.u32()?;
            tags(&mut list, count, encoding)?
        }
        None => Vec::new(),
    };
    Ok(Frame {
        script: text(document, object.require(3)?, encoding)?,
        index: usize::try_from(int(document.bytes(object.require(4)?))?)
            .map_err(|_| "a negative index")?,
        wait: object
            .field(1)
            .map(|field| int(document.bytes(field)))
            .transpose()?
            .unwrap_or(0) as u32,
        queued,
    })
}

/// A tag list: a count, then for each a key, a count, and that many tags.
fn keyed_tags(
    document: &Document,
    field: Range<usize>,
    encoding: &'static Encoding,
) -> Result<Vec<(String, Vec<Tag>)>, String> {
    let mut cursor = document.cursor(field);
    let mut out = Vec::new();
    for _ in 0..cursor.u32()? {
        let key = encoding.decode(cursor.string()?).0.into_owned();
        let count = cursor.u32()?;
        out.push((key, tags(&mut cursor, count, encoding)?));
    }
    Ok(out)
}

/// Reads a numbered save.
pub fn read(bytes: &[u8], encoding: &'static Encoding) -> Result<SlotSave, String> {
    let document = Document::open(bytes, MAGIC)?;
    let root = document.root()?;

    let position = frame(&document, &document.object(root.require(3)?)?, encoding)?;
    let mut call_stack = Vec::new();
    let mut stack = document.cursor(root.require(4)?);
    for _ in 0..stack.u32()? {
        let object = stack.object(frame_end)?;
        call_stack.push(frame(&document, &object, encoding)?);
    }

    let variables = document.object(root.require(30)?)?;
    let mut locals = Vec::new();
    let mut entries = document.cursor(variables.require(2)?);
    for _ in 0..entries.u32()? {
        let name = encoding.decode(entries.string()?).0.into_owned();
        let value = super::pc::variable_value(entries.string()?, encoding);
        locals.push((name, value));
    }

    let mut replay = Vec::new();
    for (_, layer) in keyed_tags(&document, root.require(26)?, encoding)? {
        replay.extend(layer);
    }
    // Each message layer's tags since it was last cleared, its visible text
    // among them, run inside that layer.
    for (id, tags) in keyed_tags(&document, root.require(24)?, encoding)? {
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
    if let Some(field) = root.field(56) {
        // Push handlers: a count, then an int key and a tag for each.
        let mut cursor = document.cursor(field);
        for _ in 0..cursor.u32()? {
            cursor.u32()?;
            replay.extend(tags(&mut cursor, 1, encoding)?);
        }
    }
    for number in [33, 29] {
        if let Some(field) = root.field(number) {
            let handler = field_tag(&document, field, encoding)?;
            if !handler.name.is_empty() {
                replay.push(handler);
            }
        }
    }
    // The message layer that was current, which text goes to after the load,
    // whatever was current before it (VNDegen).
    if let Some(field) = root.field(11) {
        let id = text(&document, field, encoding)?;
        if !id.is_empty() {
            replay.push(Tag {
                name: "chgmsg".into(),
                params: vec![("id".into(), id), ("stack".into(), "0".into())],
                line: -1,
            });
        }
    }

    Ok(SlotSave {
        version: document.version,
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
    pub menu_script: String,
    pub backlog_layer: String,
    /// The Lua functions `e:setEventHandler` set.
    pub key_click_start: String,
    pub key_click_end: String,
    pub on_save: String,
    pub on_load: String,
    pub on_enter_frame: String,
    pub on_debug_skip_out: String,
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

/// Version 1000's root fields in the order of the PC engine's table. Its
/// data goes in ascending order.
const TABLE_ORDER_1000: [u32; 68] = [
    31, 64, 30, 29, 28, 53, 26, 76, 75, 67, 66, 25, 24, 70, 69, 50, 38, 34, 33, 49, 48, 47, 46, 45,
    44, 43, 42, 41, 40, 37, 17, 52, 16, 65, 12, 11, 5, 58, 4, 57, 15, 68, 39, 62, 9, 61, 8, 36, 60,
    7, 51, 56, 3, 13, 18, 71, 35, 59, 6, 74, 63, 20, 73, 2, 72, 19, 14, 1,
];

/// Version 1002's root fields in the order of the PC engine's data, then
/// of its table.
const DATA_ORDER_1002: [u32; 74] = [
    1, 72, 19, 2, 73, 74, 36, 71, 3, 4, 6, 51, 7, 8, 5, 9, 39, 57, 58, 35, 68, 81, 82, 11, 12, 63,
    13, 14, 59, 60, 15, 16, 62, 52, 61, 17, 37, 20, 18, 79, 56, 78, 40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 77, 33, 34, 38, 50, 69, 70, 80, 24, 25, 66, 67, 75, 76, 26, 53, 28, 29, 65, 30, 64, 31,
];
const TABLE_ORDER_1002: [u32; 74] = [
    14, 19, 72, 73, 20, 63, 39, 68, 15, 74, 6, 59, 35, 71, 18, 13, 56, 51, 7, 60, 36, 8, 61, 9, 62,
    57, 4, 58, 5, 2, 81, 3, 82, 11, 12, 16, 52, 17, 37, 79, 78, 40, 41, 42, 43, 44, 45, 46, 47, 48,
    49, 77, 33, 34, 38, 50, 69, 70, 1, 80, 24, 25, 66, 67, 75, 76, 26, 53, 28, 29, 65, 30, 64, 31,
];

/// Version 1002's font object fields in the order of its data, then of its
/// table.
const FONT_DATA_ORDER_1002: [u32; 34] = [
    35, 3, 4, 5, 6, 10, 7, 8, 9, 11, 12, 13, 14, 15, 16, 17, 18, 19, 1, 2, 20, 21, 22, 23, 24, 33,
    34, 25, 26, 27, 28, 29, 30, 31,
];
const FONT_TABLE_ORDER_1002: [u32; 34] = [
    18, 3, 4, 33, 5, 34, 6, 35, 10, 7, 8, 9, 11, 12, 13, 14, 15, 16, 17, 19, 1, 2, 20, 21, 22, 23,
    24, 25, 26, 27, 28, 29, 30, 31,
];

struct Writer {
    encoding: &'static Encoding,
    version: Version,
}

impl Writer {
    fn int(&self, value: i32) -> Node {
        Node::Bytes(value.to_le_bytes().to_vec())
    }

    fn byte(&self, value: u8) -> Node {
        Node::Bytes(vec![value])
    }

    fn float(&self, value: f32) -> Node {
        Node::Bytes(value.to_le_bytes().to_vec())
    }

    fn text(&self, text: &str) -> Node {
        let bytes = self.encoding.encode(text).0;
        Node::Bytes(
            [
                (bytes.len() as u32).to_le_bytes().to_vec(),
                bytes.into_owned(),
            ]
            .concat(),
        )
    }

    fn texts(&self, texts: &[String]) -> Vec<Node> {
        let mut out = vec![self.int(texts.len() as i32)];
        out.extend(texts.iter().map(|text| self.text(text)));
        out
    }

    fn tag(&self, tag: Option<&Tag>) -> Node {
        let empty = Tag {
            name: String::new(),
            params: Vec::new(),
            line: 0,
        };
        let tag = tag.unwrap_or(&empty);
        let mut params = vec![self.int(tag.params.len() as i32)];
        for (key, value) in &tag.params {
            params.push(self.text(key));
            params.push(self.text(value));
        }
        Node::Object(ObjectNode::new(vec![
            (2, vec![self.int(tag.line)]),
            (3, vec![self.text(&tag.name)]),
            (4, params),
        ]))
    }

    fn tags(&self, tags: &[Tag]) -> Vec<Node> {
        let mut out = vec![self.int(tags.len() as i32)];
        out.extend(tags.iter().map(|tag| self.tag(Some(tag))));
        out
    }

    fn keyed_tags(&self, entries: &[(String, Vec<Tag>)]) -> Vec<Node> {
        let mut out = vec![self.int(entries.len() as i32)];
        for (key, tags) in entries {
            out.push(self.text(key));
            out.extend(self.tags(tags));
        }
        out
    }

    fn frame(&self, frame: &WriteFrame) -> Node {
        let mut fields = vec![
            (1, vec![self.int(frame.wait as i32)]),
            (2, vec![self.int(0)]),
            (3, vec![self.text(&frame.script)]),
            (4, vec![self.int(frame.index as i32)]),
            (6, self.tags(&frame.queued)),
            (7, vec![self.byte(0)]),
            (8, vec![self.tag(frame.at.as_ref())]),
        ];
        if self.version == Version::V1002 {
            fields.retain(|(number, _)| *number != 7);
        }
        Node::Object(ObjectNode::new(fields))
    }

    /// A font. Version 1000 keeps positions and spacing as ints and scales
    /// as 65536 or 255 for 1; version 1002 keeps them as floats, and adds
    /// field 35.
    fn font(&self, font: &FontLayout) -> Node {
        let v1002 = self.version == Version::V1002;
        let number = |value: i32| {
            if v1002 {
                self.float(value as f32)
            } else {
                self.int(value)
            }
        };
        let one = |fixed: i32| {
            if v1002 {
                self.float(1.0)
            } else {
                self.int(fixed)
            }
        };
        let mut fields = vec![
            (1, vec![self.text(&font.face)]),
            (2, vec![self.text(&font.ruby_face)]),
            (3, vec![number(font.left)]),
            (4, vec![number(font.top)]),
            (5, vec![number(font.width)]),
            (6, vec![number(font.height)]),
            (7, vec![self.int(0)]),
            (8, vec![self.int(0)]),
            (9, vec![self.byte(1)]),
            (10, vec![one(255)]),
            (11, vec![one(65536)]),
            (12, vec![one(65536)]),
            (13, vec![self.int(0)]),
            (14, vec![self.int(0)]),
            (15, vec![self.byte(1)]),
            (16, vec![one(255)]),
            (17, vec![one(65536)]),
            (18, vec![one(65536)]),
            (19, vec![self.int(0)]),
            (20, vec![self.byte(font.size)]),
            (21, vec![self.byte(font.ruby_size)]),
            (22, vec![number(font.space_top)]),
            (23, vec![number(font.space_middle)]),
            (24, vec![number(font.space_bottom)]),
            (25, vec![self.int(font.color as i32)]),
            (26, vec![self.int(font.shadow_color as i32)]),
            (27, vec![self.int(font.outline_color as i32)]),
            (28, vec![self.int(font.align)]),
            (29, vec![self.int(font.style)]),
            (30, vec![self.byte(0)]),
            (31, vec![self.byte(u8::from(font.hung))]),
            (33, vec![self.int(font.kerning)]),
            (34, vec![self.int(0)]),
        ];
        if !v1002 {
            return Node::Object(ObjectNode::new(fields));
        }
        fields.push((35, vec![self.int(1)]));
        let mut object = ObjectNode::new(
            FONT_DATA_ORDER_1002
                .iter()
                .map(|number| {
                    let index = fields
                        .iter()
                        .position(|(n, _)| n == number)
                        .expect("font field");
                    fields.swap_remove(index)
                })
                .collect(),
        );
        object.table_order = FONT_TABLE_ORDER_1002.to_vec();
        Node::Object(object)
    }

    /// A field that is an object of int fields, all 0. Version 1000 writes
    /// it empty, as the engine reads missing fields as 0.
    fn zero_object(&self, numbers: &[u32]) -> Node {
        match self.version {
            Version::V1000 => self.int(0),
            Version::V1002 => Node::Object(ObjectNode::new(
                numbers
                    .iter()
                    .map(|number| (*number, vec![self.int(0)]))
                    .collect(),
            )),
        }
    }

    fn variables(&self, locals: &Variables) -> Node {
        let mut list = (locals.len() as u32).to_le_bytes().to_vec();
        for (name, value) in locals {
            for bytes in [
                self.encoding.encode(name).0.into_owned(),
                super::pc::variable_bytes(value, self.encoding),
            ] {
                list.extend((bytes.len() as u32).to_le_bytes());
                list.extend(bytes);
            }
        }
        let list = Node::Bytes(list);
        let macro_scope = Node::Object(ObjectNode::new(vec![
            (1, vec![self.int(0)]),
            (2, vec![self.int(0)]),
            (3, vec![self.int(0)]),
        ]));
        Node::Object(ObjectNode::new(vec![
            (1, vec![self.text("'")]),
            (2, vec![list]),
            (3, vec![self.int(1), macro_scope]),
        ]))
    }
}

/// Writes a numbered save the PC engine loads, in the version its engine
/// writes.
pub fn write(save: &SlotWrite, version: Version, encoding: &'static Encoding) -> Vec<u8> {
    let w = Writer { encoding, version };
    let v1002 = version == Version::V1002;
    let mut stack = vec![w.int(save.call_stack.len() as i32)];
    stack.extend(save.call_stack.iter().map(|frame| w.frame(frame)));
    let mut fonts = vec![w.int(save.fonts.len() as i32)];
    for (id, font) in &save.fonts {
        fonts.push(w.text(id));
        fonts.push(w.int(1));
        fonts.push(w.font(font));
    }
    let mut push = vec![w.int(save.push_handlers.len() as i32)];
    for tag in &save.push_handlers {
        push.push(w.int(0));
        push.push(w.tag(Some(tag)));
    }

    let field = |number: u32| -> Vec<Node> {
        match number {
            // Flags, as the engine writes them in a fresh save.
            1 | 2 | 28 | 57 | 58 | 61 | 71 => vec![w.byte(1)],
            6 => vec![w.byte(u8::from(!v1002))],
            7 | 15 | 17 | 52 | 68 | 72 | 73 | 74 => vec![w.byte(0)],
            5 => vec![w.int(1)],
            79 => vec![w.int(3000)],
            12 => vec![w.int(1), w.int(0)],
            3 => vec![w.frame(&save.position)],
            4 => stack.clone(),
            11 => vec![w.text(&save.current_message_layer)],
            24 => w.keyed_tags(&save.message_layers),
            25 => fonts.clone(),
            26 => w.keyed_tags(&save.layers),
            29 => vec![w.tag(save.bgm.as_ref())],
            30 => vec![w.variables(&save.locals)],
            31 => vec![w.zero_object(&[7, 1, 2, 3, 4, 5, 6])],
            33 => vec![w.tag(save.dirchg.as_ref())],
            34 => vec![w.text(&save.on_enter_frame)],
            36 => w.texts(&save.macro_files),
            38 => vec![w.text(&save.key_click_start)],
            50 => vec![w.text(&save.key_click_end)],
            51 => vec![w.text(&save.menu_script)],
            40..=49 | 77 => vec![w.tag(None)],
            56 => push.clone(),
            62 => vec![w.text(&save.backlog_layer)],
            // The default font, which 1000 reads fine empty.
            63 if v1002 => vec![w.font(&FontLayout::default())],
            65 if v1002 => vec![Node::Object(ObjectNode::new(vec![(1, vec![w.int(0)])]))],
            69 => vec![w.text(&save.on_save)],
            70 => vec![w.text(&save.on_load)],
            80 => vec![w.text(&save.on_debug_skip_out)],
            // Message window settings the engine rebuilds from the game's
            // scripts, which load fine empty, and fields that are zero in a
            // fresh save.
            _ => vec![w.int(0)],
        }
    };
    let (data_order, table_order) = if v1002 {
        (DATA_ORDER_1002.to_vec(), TABLE_ORDER_1002.to_vec())
    } else {
        let mut ascending = TABLE_ORDER_1000.to_vec();
        ascending.sort_unstable();
        (ascending, TABLE_ORDER_1000.to_vec())
    };
    let mut root = ObjectNode::new(data_order.into_iter().map(|n| (n, field(n))).collect());
    root.table_order = table_order;
    bow::encode(MAGIC, version, &root)
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
        for version in [Version::V1000, Version::V1002] {
            let read_back = read(&write(&save, version, SHIFT_JIS), SHIFT_JIS).unwrap();
            assert_eq!(read_back.version, version);
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
            assert_eq!(
                names,
                ["lyc", "chgmsg", "print", "/chgmsg", "splay", "chgmsg"]
            );
            let current = read_back.replay.last().unwrap();
            assert_eq!(
                current.params,
                [
                    ("id".to_string(), "adv".to_string()),
                    ("stack".to_string(), "0".to_string())
                ],
                "the current message layer is current again after the load"
            );
        }
    }

    #[test]
    fn rejects_other_files() {
        assert!(read(b"{}", SHIFT_JIS).is_err());
        assert!(read(b"BOWS\xe8\x03\x00\x00", SHIFT_JIS).is_err());
    }
}
