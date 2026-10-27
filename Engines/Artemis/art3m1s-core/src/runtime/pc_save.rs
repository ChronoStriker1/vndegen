//! Builds a PC Artemis engine save from the runtime's state (VNDegen).
//!
//! The PC engine keeps its state as the tags that recreate it, so each part
//! of the scene becomes tags: a layer an `lyc`, an `lyprop` with the
//! properties set, and an `lyevent` for each handler; a message layer the
//! text of its page; the music its `splay`.

use super::CoreRuntime;
use crate::save::pc_slot::{FontLayout, SlotWrite, Tag, WriteFrame};
use std::collections::HashMap;

fn tag(name: &str, params: impl IntoIterator<Item = (String, String)>) -> Tag {
    let mut params: Vec<(String, String)> = params.into_iter().collect();
    params.sort();
    Tag {
        name: name.to_string(),
        params,
        line: -1,
    }
}

fn number(value: f32) -> String {
    if value.fract() == 0.0 {
        format!("{}", value as i64)
    } else {
        format!("{value}")
    }
}

/// `RRGGBB`, `0xRRGGBB`, or `#RRGGBB` as a number.
fn color(text: Option<&str>, default: u32) -> u32 {
    text.map(|text| text.trim_start_matches("0x").trim_start_matches('#'))
        .and_then(|hex| u32::from_str_radix(hex, 16).ok())
        .map_or(default, |value| value & 0xFF_FFFF)
}

fn layer_tags(layer: &crate::compositor::Layer) -> Vec<Tag> {
    let id = ("id".to_string(), layer.id.clone());
    let mut tags = Vec::new();
    let props = &layer.props;
    if let Some(file) = &layer.file {
        tags.push(tag("lyc", [id.clone(), ("file".into(), file.clone())]));
    } else if let Some([r, g, b, a]) = layer.solid_color {
        let mut create = vec![
            id.clone(),
            ("color".into(), format!("{a:02X}{r:02X}{g:02X}{b:02X}")),
        ];
        if let Some(width) = props.width {
            create.push(("width".into(), number(width)));
        }
        if let Some(height) = props.height {
            create.push(("height".into(), number(height)));
        }
        tags.push(tag("lyc", create));
    }
    let mut set = vec![id.clone()];
    let mut put = |name: &str, value: Option<String>| {
        if let Some(value) = value {
            set.push((name.to_string(), value));
        }
    };
    put("left", props.left.map(number));
    put("top", props.top.map(number));
    if layer.solid_color.is_none() {
        put("width", props.width.map(number));
        put("height", props.height.map(number));
    }
    put("xscale", props.x_scale.map(number));
    put("yscale", props.y_scale.map(number));
    put("rotate", props.rotate.map(number));
    put("alpha", props.alpha.map(|alpha| alpha.to_string()));
    put("visible", props.visible.map(|visible| u8::from(visible).to_string()));
    put(
        "clip",
        props
            .clip
            .map(|clip| clip.iter().map(|v| number(*v)).collect::<Vec<_>>().join(",")),
    );
    put("layermode", props.layer_mode.clone());
    for (key, value) in &props.custom {
        set.push((key.clone(), value.clone()));
    }
    if set.len() > 1 {
        tags.push(tag("lyprop", set));
    }
    let mut events: Vec<_> = layer.event_handlers.iter().collect();
    events.sort_by(|a, b| a.0.cmp(b.0));
    for (kind, handler) in events {
        let mut params: HashMap<String, String> = handler.filter_params.clone();
        params.insert("id".into(), layer.id.clone());
        params.insert("type".into(), kind.clone());
        if !handler.enabled {
            params.insert("mode".into(), "disable".into());
        }
        tags.push(tag("lyevent", params));
    }
    tags
}

/// A message layer's page as the tags that print it.
fn page_tag(page_tag: &crate::text::BacklogTag) -> Tag {
    use crate::text::BacklogTag;
    match page_tag {
        BacklogTag::Text(text) => tag("print", [("data".to_string(), text.clone())]),
        BacklogTag::LineBreak => tag("rt", []),
        BacklogTag::Font(params) => tag("font", params.clone()),
        BacklogTag::RubyStart(text) => tag("ruby", [("text".to_string(), text.clone())]),
        BacklogTag::RubyEnd => tag("/ruby", []),
    }
}

fn font_layout(layer: &crate::text::render::MessageLayer) -> FontLayout {
    let font = &layer.font;
    let defaults = FontLayout::default();
    let style = font.style.as_deref().unwrap_or("outline,shadow");
    let mut style_bits = 0;
    for (name, bit) in [("bold", 1), ("outline", 2), ("shadow", 4)] {
        if style.split(',').any(|part| part.trim() == name) {
            style_bits |= bit;
        }
    }
    FontLayout {
        face: font.face.clone().unwrap_or_default(),
        ruby_face: font.ruby_face.clone().or_else(|| font.face.clone()).unwrap_or_default(),
        left: layer.left as i32,
        top: layer.top as i32,
        width: layer.width as i32,
        height: layer.height as i32,
        size: font.size.map_or(defaults.size, |size| size as u8),
        ruby_size: font.ruby_size.map_or(defaults.ruby_size, |size| size as u8),
        space_top: font.spacetop.map_or(defaults.space_top, |v| v as i32),
        space_middle: font.spacemiddle.map_or(defaults.space_middle, |v| v as i32),
        space_bottom: font.spacebottom.map_or(defaults.space_bottom, |v| v as i32),
        color: color(font.color.as_deref(), defaults.color),
        shadow_color: color(font.shadow_color.as_deref(), 0),
        outline_color: color(font.outline_color.as_deref(), 0),
        align: match font.align.as_deref() {
            Some("center") => 1,
            Some("right") => 2,
            _ => 0,
        },
        style: style_bits,
        hung: font.hung.unwrap_or(true),
        kerning: font.kerning.map_or(0, |v| v as i32),
    }
}

impl CoreRuntime {
    /// A script position with the PC engine's instruction index. A call
    /// frame also holds the tag it returns to.
    fn write_frame(&self, script: &str, index: usize, wait: u32, queued: Vec<Tag>, with_tag: bool) -> WriteFrame {
        let at = self
            .interpreter
            .get_script(script)
            .and_then(|script| script.instructions.get(index))
            .filter(|_| with_tag)
            .map(|instruction| {
                let mut tagged = tag(
                    &instruction.tag,
                    instruction
                        .params
                        .iter()
                        .filter(|(key, _)| !key.starts_with('\u{b}'))
                        .map(|(k, v)| (k.clone(), v.clone())),
                );
                tagged.line = instruction.line as i32;
                tagged
            });
        WriteFrame {
            script: script.to_string(),
            index: self.interpreter.pc_index(script, index),
            wait,
            queued,
            at,
        }
    }

    /// The runtime's state as a PC engine save.
    pub(super) fn pc_slot_save(
        &self,
        scene: &crate::compositor::Scene,
        text: Option<&crate::save::MessageTextSnapshot>,
    ) -> SlotWrite {
        let interpreter = &self.interpreter;
        let position = self.write_frame(
            interpreter.current_script().unwrap_or_default(),
            interpreter.current_line(),
            0,
            Vec::new(),
            false,
        );
        // The frame an input handler returns to was in a wait, which a load
        // resumes, then runs the tags queued behind a click wait. Without a
        // handler, queued tags mean the innermost frame was at a click wait.
        let frames = interpreter.call_stack();
        let queued: Vec<Tag> = interpreter
            .queued_wait_tags()
            .into_iter()
            .map(|(name, params)| tag(&name, params))
            .collect();
        let (waiting, wait) = match &self.active_inline_event_frame {
            Some(event) => (event.stack.len(), self.inline_event_wait),
            None => (frames.len().saturating_sub(1), u32::from(!queued.is_empty())),
        };
        let call_stack = frames
            .iter()
            .enumerate()
            .map(|(i, frame)| {
                let (wait, queued) = match (i == waiting, wait) {
                    (true, 1) => (1, queued.clone()),
                    (true, wait) => (wait, Vec::new()),
                    (false, _) => (0, Vec::new()),
                };
                self.write_frame(&frame.script, frame.return_line, wait, queued, true)
            })
            .collect();

        let variables = interpreter.variables();
        let mut locals: Vec<_> = variables
            .iter_local()
            .map(|(name, value)| (name.clone(), value.clone()))
            .collect();
        locals.sort_by(|a, b| a.0.cmp(&b.0));

        let mut layers: Vec<_> = scene
            .all_layers()
            .map(|layer| (layer.id.clone(), layer_tags(layer)))
            .filter(|(_, tags)| !tags.is_empty())
            .collect();
        layers.sort_by(|a, b| a.0.cmp(&b.0));

        let message_layers = text
            .map(|text| {
                text.layers
                    .iter()
                    .map(|layer| (layer.id.clone(), layer.tags.iter().map(page_tag).collect()))
                    .collect()
            })
            .unwrap_or_default();
        let mut fonts = Vec::new();
        let mut current_message_layer = String::new();
        if let Some(renderer) = self.text_renderer.as_ref() {
            let state = renderer.font_state();
            current_message_layer = state.active_layer.clone().unwrap_or_default();
            let mut ids: Vec<_> = state.layers.keys().cloned().collect();
            ids.sort();
            for id in ids {
                fonts.push((id.clone(), font_layout(&state.layers[&id])));
            }
        }

        let mut push_handlers = Vec::new();
        let mut dirchg = None;
        let mut handlers: Vec<_> = self.compositor.input_handlers().collect();
        handlers.sort_by(|a, b| a.0.cmp(b.0));
        for ((event, key), handler) in handlers {
            let mut params = handler.filter_params.clone();
            if !key.is_empty() {
                params.insert("key".into(), key.clone());
            }
            match event.as_str() {
                "push" => push_handlers.push(tag("setonpush", params)),
                "dirchg" => dirchg = Some(tag("setondirchg", params)),
                _ => {}
            }
        }

        let bgm = self.audio.audio_state().bgm_channel.as_ref().map(|channel| {
            tag(
                "splay",
                [
                    ("file".to_string(), channel.file.clone()),
                    ("gain".to_string(), channel.raw_gain.to_string()),
                    ("loop".to_string(), u8::from(channel.loop_play).to_string()),
                    ("pan".to_string(), channel.raw_pan.to_string()),
                    ("time".to_string(), "0".to_string()),
                ],
            )
        });

        let handlers = interpreter.engine_context().lock().unwrap().event_handlers.clone();
        let handler = |name: &str| handlers.get(name).cloned().unwrap_or_default();
        SlotWrite {
            position,
            call_stack,
            locals,
            layers,
            message_layers,
            fonts,
            push_handlers,
            dirchg,
            bgm,
            current_message_layer,
            macro_files: interpreter.loaded_macro_files(),
            key_click_start: handler("onClickWaitIn"),
            key_click_end: handler("onClickWaitOut"),
            menu_script: self.control.rclick_file().unwrap_or_default(),
            backlog_layer: "backlog".into(),
            on_save: handler("onSave"),
            on_load: handler("onLoad"),
        }
    }
}
