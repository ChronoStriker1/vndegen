use std::collections::HashMap;
use std::ffi::{c_char, c_int, CStr, CString};
use std::ptr;
use std::slice;

use eluna::sdk::{EmoteLoadOptions, EmoteRuntime};
use eluna::{EmoteDrawFrameInfo, EmoteDrawPass, EmoteStaticSprite, PsbDecryptionKey, PsbNormalizeOptions, TimelinePlayMode};

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Vertex {
    x: f32,
    y: f32,
    u: f32,
    v: f32,
    r: f32,
    g: f32,
    b: f32,
    a: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Draw {
    texture: u32,
    blend: u32,
    pass: u32,
    first: u32,
    count: u32,
    clip: [f32; 4],
    stencil_first: u32,
    stencil_count: u32,
    stencil_initial: u32,
    stencil_final: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct StencilSource {
    phase: u32,
    texture: u32,
    first: u32,
    count: u32,
}

struct Texture {
    width: i32,
    height: i32,
    rgba: Vec<u8>,
}

pub struct Model {
    runtime: EmoteRuntime,
    textures: HashMap<u32, Option<Texture>>,
    vertices: Vec<Vertex>,
    draws: Vec<Draw>,
    stencils: Vec<StencilSource>,
    names: Vec<CString>,
}

#[repr(C)]
pub struct ApiV1 {
    version: u32,
    open: unsafe extern "C" fn(*const u8, usize, u32, *mut c_char, usize) -> *mut Model,
    close: unsafe extern "C" fn(*mut Model),
    set_variable: unsafe extern "C" fn(*mut Model, *const c_char, f32, f32, f32),
    play_timeline: unsafe extern "C" fn(*mut Model, *const c_char, u32, c_int) -> c_int,
    stop_timeline: unsafe extern "C" fn(*mut Model, *const c_char),
    timeline_playing: unsafe extern "C" fn(*mut Model, *const c_char) -> c_int,
    progress: unsafe extern "C" fn(*mut Model, f32),
    build: unsafe extern "C" fn(*mut Model) -> c_int,
    draws: unsafe extern "C" fn(*mut Model, *mut c_int) -> *const Draw,
    vertices: unsafe extern "C" fn(*mut Model, *mut c_int) -> *const Vertex,
    texture: unsafe extern "C" fn(*mut Model, u32, *mut c_int, *mut c_int) -> *const u8,
    bounds: unsafe extern "C" fn(*mut Model, *mut f32) -> c_int,
    variable_count: unsafe extern "C" fn(*mut Model) -> c_int,
    variable_name: unsafe extern "C" fn(*mut Model, c_int) -> *const c_char,
    stencils: unsafe extern "C" fn(*mut Model, *mut c_int) -> *const StencilSource,
}

static API: ApiV1 = ApiV1 {
    version: 1,
    open,
    close,
    set_variable,
    play_timeline,
    stop_timeline,
    timeline_playing,
    progress,
    build,
    draws,
    vertices,
    texture,
    bounds,
    variable_count,
    variable_name,
    stencils,
};

#[no_mangle]
pub extern "C" fn vnemote_get_api_v1() -> *const ApiV1 {
    &API
}

unsafe fn write_error(out: *mut c_char, size: usize, message: &str) {
    if out.is_null() || size == 0 {
        return;
    }
    let bytes = message.as_bytes();
    let n = bytes.len().min(size - 1);
    ptr::copy_nonoverlapping(bytes.as_ptr(), out as *mut u8, n);
    *out.add(n) = 0;
}

unsafe fn text<'a>(s: *const c_char) -> Option<&'a str> {
    if s.is_null() {
        return None;
    }
    CStr::from_ptr(s).to_str().ok()
}

unsafe extern "C" fn open(data: *const u8, size: usize, key: u32, error: *mut c_char, error_size: usize) -> *mut Model {
    if data.is_null() {
        write_error(error, error_size, "no data");
        return ptr::null_mut();
    }
    let bytes = slice::from_raw_parts(data, size);
    let options = EmoteLoadOptions {
        normalize: PsbNormalizeOptions {
            decrypt_key: Some(PsbDecryptionKey::emote_key(key)),
            ..PsbNormalizeOptions::default()
        },
        motion: None,
        timeline: None,
        autoplay_timeline: false,
    };
    match EmoteRuntime::from_bytes(bytes, options) {
        Ok(runtime) => {
            let names = runtime
                .variable_names()
                .iter()
                .filter_map(|name| CString::new(*name).ok())
                .collect();
            Box::into_raw(Box::new(Model {
                runtime,
                textures: HashMap::new(),
                vertices: Vec::new(),
                draws: Vec::new(),
                stencils: Vec::new(),
                names,
            }))
        }
        Err(err) => {
            write_error(error, error_size, &err.to_string());
            ptr::null_mut()
        }
    }
}

unsafe extern "C" fn close(model: *mut Model) {
    if !model.is_null() {
        drop(Box::from_raw(model));
    }
}

unsafe extern "C" fn set_variable(model: *mut Model, name: *const c_char, value: f32, ticks: f32, easing: f32) {
    let (Some(model), Some(name)) = (model.as_mut(), text(name)) else {
        return;
    };
    if ticks > 0.0 {
        model.runtime.set_variable_timed(name, value, ticks, easing);
    } else {
        model.runtime.set_variable(name, value);
    }
}

unsafe extern "C" fn play_timeline(model: *mut Model, name: *const c_char, flags: u32, looping: c_int) -> c_int {
    let (Some(model), Some(name)) = (model.as_mut(), text(name)) else {
        return 0;
    };
    let mode = TimelinePlayMode::from_flags(flags).with_looping(looping != 0);
    model.runtime.play_timeline(name, mode).is_ok() as c_int
}

unsafe extern "C" fn stop_timeline(model: *mut Model, name: *const c_char) {
    if let (Some(model), Some(name)) = (model.as_mut(), text(name)) {
        let _ = model.runtime.stop_timeline(name);
    }
}

unsafe extern "C" fn timeline_playing(model: *mut Model, name: *const c_char) -> c_int {
    match (model.as_ref(), text(name)) {
        (Some(model), Some(name)) => model.runtime.is_timeline_playing(name) as c_int,
        _ => 0,
    }
}

unsafe extern "C" fn progress(model: *mut Model, ticks: f32) {
    if let Some(model) = model.as_mut() {
        let _ = model.runtime.progress_ticks(ticks);
    }
}

fn corner(sprite: &EmoteStaticSprite, packed: u32) -> [f32; 4] {
    [
        ((packed >> 24) & 0xff) as f32 / 255.0,
        ((packed >> 16) & 0xff) as f32 / 255.0,
        ((packed >> 8) & 0xff) as f32 / 255.0,
        ((packed & 0xff) as f32 / 255.0 * sprite.opacity).clamp(0.0, 1.0),
    ]
}

fn bilerp(c: &[[f32; 4]; 4], u: f32, v: f32) -> [f32; 4] {
    let mut out = [0.0; 4];
    for i in 0..4 {
        let top = c[0][i] + (c[1][i] - c[0][i]) * u;
        let bottom = c[2][i] + (c[3][i] - c[2][i]) * u;
        out[i] = top + (bottom - top) * v;
    }
    out
}

fn transform(sprite: &EmoteStaticSprite, p: [f32; 2]) -> [f32; 2] {
    let sx = if sprite.scale_x.is_finite() { sprite.scale_x } else { 1.0 };
    let sy = if sprite.scale_y.is_finite() { sprite.scale_y } else { 1.0 };
    let angle = if sprite.rotation_degrees.is_finite() { sprite.rotation_degrees.to_radians() } else { 0.0 };
    let (sin, cos) = angle.sin_cos();
    let dx = (p[0] - sprite.center_x) * sx;
    let dy = (p[1] - sprite.center_y) * sy;
    let local = [sprite.center_x + dx * cos - dy * sin, sprite.center_y + dx * sin + dy * cos];
    let m = sprite.world_transform;
    [m[0] * local[0] + m[1] * local[1] + m[4], m[2] * local[0] + m[3] * local[1] + m[5]]
}

fn push_sprite(out: &mut Vec<Vertex>, sprite: &EmoteStaticSprite) {
    let colors = [
        corner(sprite, sprite.corner_colors[0]),
        corner(sprite, sprite.corner_colors[1]),
        corner(sprite, sprite.corner_colors[2]),
        corner(sprite, sprite.corner_colors[3]),
    ];
    let (dx, dy) = match &sprite.mesh {
        Some(mesh) => (mesh.division_x.max(1) as usize, mesh.division_y.max(1) as usize),
        None => (1, 1),
    };
    let left = sprite.center_x - sprite.width * 0.5;
    let top = sprite.center_y - sprite.height * 0.5;
    let at = |ix: usize, iy: usize| -> Vertex {
        let u = ix as f32 / dx as f32;
        let v = iy as f32 / dy as f32;
        let p = match &sprite.mesh {
            Some(mesh) => mesh.sample(u, v),
            None => [u, v],
        };
        let pos = transform(sprite, [left + p[0] * sprite.width, top + p[1] * sprite.height]);
        let c = bilerp(&colors, u, v);
        Vertex {
            x: pos[0],
            y: pos[1],
            u: sprite.uv_left + (sprite.uv_right - sprite.uv_left) * u,
            v: sprite.uv_top + (sprite.uv_bottom - sprite.uv_top) * v,
            r: c[0],
            g: c[1],
            b: c[2],
            a: c[3],
        }
    };
    for y in 0..dy {
        for x in 0..dx {
            let (tl, bl, tr, br) = (at(x, y), at(x, y + 1), at(x + 1, y), at(x + 1, y + 1));
            out.extend_from_slice(&[tl, bl, tr, tr, bl, br]);
        }
    }
}

fn pass_code(pass: &EmoteDrawPass) -> u32 {
    match pass {
        EmoteDrawPass::Normal => 0,
        EmoteDrawPass::MaskGeneration => 1,
        EmoteDrawPass::StencilCompositeMask => 2,
        EmoteDrawPass::Filtered => 3,
    }
}

unsafe extern "C" fn build(model: *mut Model) -> c_int {
    let Some(model) = model.as_mut() else {
        return 0;
    };
    let Model { runtime, vertices, draws, stencils, .. } = model;
    vertices.clear();
    draws.clear();
    stencils.clear();
    let scene = runtime.scene();
    let visible: Vec<&EmoteStaticSprite> = scene
        .sprites
        .iter()
        .filter(|s| s.visible && s.opacity > 0.0 && !s.feedback_history)
        .collect();
    let mut by_key: HashMap<&[u64], &EmoteStaticSprite> = HashMap::new();
    for sprite in &visible {
        by_key.insert(sprite.draw_frame_info.native_draw_key.as_slice(), *sprite);
    }
    let infos: HashMap<&[u64], &EmoteDrawFrameInfo> = scene
        .layer_states
        .iter()
        .map(|state| (state.draw_frame_info.native_draw_key.as_slice(), &state.draw_frame_info))
        .collect();
    for sprite in &visible {
        if matches!(sprite.draw_frame_info.pass, EmoteDrawPass::MaskGeneration | EmoteDrawPass::StencilCompositeMask) {
            continue;
        }
        let mut chain: Vec<&EmoteDrawFrameInfo> = Vec::new();
        let mut cursor = sprite.draw_frame_info.stencil_parent_native_key.as_deref();
        let mut seen = std::collections::HashSet::new();
        while let Some(key) = cursor {
            if !seen.insert(key) {
                break;
            }
            let Some(info) = infos.get(key).copied() else {
                break;
            };
            if matches!(info.stencil_phase, 1 | 2) {
                chain.push(info);
            }
            cursor = info.stencil_parent_native_key.as_deref();
        }
        let initial: u32 = if chain.iter().any(|i| i.stencil_phase == 2) { 1 } else { 0 };
        let mut final_ref = initial;
        let stencil_first = stencils.len() as u32;
        for phase in [1i64, 2i64] {
            for info in chain.iter().copied().filter(|i| i.stencil_phase == phase) {
                let keys: Vec<&[u64]> = if (info.stencil_type & 4) != 0 {
                    scene
                        .composite_mask_sources_by_key
                        .get(&info.native_draw_key)
                        .map(|list| list.iter().map(|k| k.as_slice()).collect())
                        .unwrap_or_default()
                } else {
                    vec![info.native_draw_key.as_slice()]
                };
                for key in keys {
                    if let Some(source) = by_key.get(key).copied() {
                        let first = vertices.len() as u32;
                        push_sprite(vertices, source);
                        stencils.push(StencilSource {
                            phase: phase as u32,
                            texture: source.texture_resource_index,
                            first,
                            count: vertices.len() as u32 - first,
                        });
                    }
                }
                if phase == 1 {
                    stencils.push(StencilSource { phase: 9, texture: 0, first: 0, count: 0 });
                    final_ref = (final_ref + 1).min(255);
                }
            }
        }
        let first = vertices.len() as u32;
        push_sprite(vertices, sprite);
        draws.push(Draw {
            texture: sprite.texture_resource_index,
            blend: sprite.blend_mode,
            pass: pass_code(&sprite.draw_frame_info.pass),
            first,
            count: vertices.len() as u32 - first,
            clip: sprite.draw_frame_info.clip_rect.unwrap_or([-1.0e30, -1.0e30, 1.0e30, 1.0e30]),
            stencil_first,
            stencil_count: stencils.len() as u32 - stencil_first,
            stencil_initial: initial,
            stencil_final: final_ref,
        });
    }
    draws.len() as c_int
}

unsafe extern "C" fn stencils(model: *mut Model, count: *mut c_int) -> *const StencilSource {
    let Some(model) = model.as_ref() else {
        return ptr::null();
    };
    if !count.is_null() {
        *count = model.stencils.len() as c_int;
    }
    model.stencils.as_ptr()
}

unsafe extern "C" fn draws(model: *mut Model, count: *mut c_int) -> *const Draw {
    let Some(model) = model.as_ref() else {
        return ptr::null();
    };
    if !count.is_null() {
        *count = model.draws.len() as c_int;
    }
    model.draws.as_ptr()
}

unsafe extern "C" fn vertices(model: *mut Model, count: *mut c_int) -> *const Vertex {
    let Some(model) = model.as_ref() else {
        return ptr::null();
    };
    if !count.is_null() {
        *count = model.vertices.len() as c_int;
    }
    model.vertices.as_ptr()
}

fn decompress_rl(bytes: &[u8], align: usize, expected: usize) -> Option<Vec<u8>> {
    let mut out = Vec::with_capacity(expected);
    let mut pos = 0;
    while pos < bytes.len() {
        let cmd = bytes[pos];
        pos += 1;
        if cmd & 0x80 != 0 {
            let count = (cmd ^ 0x80) as usize + 3;
            let pattern = bytes.get(pos..pos + align)?;
            for _ in 0..count {
                out.extend_from_slice(pattern);
            }
            pos += align;
        } else {
            let count = (cmd as usize + 1) * align;
            out.extend_from_slice(bytes.get(pos..pos + count)?);
            pos += count;
        }
    }
    Some(out)
}

fn big_endian_rgba(format: Option<&str>) -> bool {
    let Some(format) = format else {
        return false;
    };
    let normalized: String = format.chars().filter(|c| c.is_ascii_alphanumeric()).map(|c| c.to_ascii_uppercase()).collect();
    normalized == "BERGBA8"
}

fn decode_texture(model: &Model, index: u32) -> Option<Texture> {
    let source = model.runtime.texture_sources().values().find(|t| t.resource_index == index)?;
    let bytes = model.runtime.texture_bytes(index)?;
    let bpp = source.bit_count.unwrap_or(32) as usize;
    if bpp != 32 {
        return None;
    }
    let expected = source.width as usize * source.height as usize * 4;
    let payload = if source.compress.as_deref().is_some_and(|c| c.eq_ignore_ascii_case("RL")) {
        decompress_rl(bytes, 4, expected)?
    } else {
        bytes.to_vec()
    };
    if payload.len() != expected {
        return None;
    }
    let mut rgba = Vec::with_capacity(expected);
    if big_endian_rgba(source.format.as_deref()) {
        rgba.extend_from_slice(&payload);
    } else {
        for px in payload.chunks_exact(4) {
            rgba.extend_from_slice(&[px[2], px[1], px[0], px[3]]);
        }
    }
    Some(Texture { width: source.width as i32, height: source.height as i32, rgba })
}

unsafe extern "C" fn texture(model: *mut Model, index: u32, width: *mut c_int, height: *mut c_int) -> *const u8 {
    let Some(model) = model.as_mut() else {
        return ptr::null();
    };
    if !model.textures.contains_key(&index) {
        let decoded = decode_texture(model, index);
        model.textures.insert(index, decoded);
    }
    match model.textures.get(&index) {
        Some(Some(t)) => {
            if !width.is_null() {
                *width = t.width;
            }
            if !height.is_null() {
                *height = t.height;
            }
            t.rgba.as_ptr()
        }
        _ => ptr::null(),
    }
}

unsafe extern "C" fn bounds(model: *mut Model, out: *mut f32) -> c_int {
    let (Some(model), false) = (model.as_ref(), out.is_null()) else {
        return 0;
    };
    match model.runtime.bounds() {
        Some(b) => {
            *out = b.min_x;
            *out.add(1) = b.min_y;
            *out.add(2) = b.max_x;
            *out.add(3) = b.max_y;
            1
        }
        None => 0,
    }
}

unsafe extern "C" fn variable_count(model: *mut Model) -> c_int {
    model.as_ref().map_or(0, |m| m.names.len() as c_int)
}

unsafe extern "C" fn variable_name(model: *mut Model, index: c_int) -> *const c_char {
    match model.as_ref().and_then(|m| m.names.get(index as usize)) {
        Some(name) => name.as_ptr(),
        None => ptr::null(),
    }
}
