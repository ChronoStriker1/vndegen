//! VNDegen's C interface to siglus_rs: the table `vnsiglus.h` declares, over siglus_scene_vm's `SiglusHost` for
//! SiglusEngine games and the `reallive` crate's engine for RealLive games. The host calls every function on one
//! thread.

use std::ffi::{CStr, CString, c_char, c_void};
use std::fs::{File, OpenOptions};
use std::io::Write;
use std::path::{Path, PathBuf};
use std::ptr::{NonNull, null, null_mut};
use std::sync::Mutex;

use anyhow::{Context, Result, anyhow, bail};
use reallive::engine::{Engine, EngineOptions};
use reallive::input::{Button, InputEvent, Key};
use siglus_scene_vm::host::{SiglusHost, SiglusHostConfig};
use siglus_scene_vm::render::Renderer;
use siglus_scene_vm::runtime::input::{VmKey, VmMouseButton};

mod keys;

pub type MessageBoxFn =
    unsafe extern "C" fn(context: *mut c_void, request: u64, kind: i32, title: *const c_char, message: *const c_char);

pub const VIEW_UIKIT: i32 = 1;
pub const VIEW_APPKIT: i32 = 2;

pub const RUNNING: i32 = 0;
pub const ENDED: i32 = 1;
pub const FAILED: i32 = 2;

/// `VNSiglusApiV1`, field for field.
#[repr(C)]
pub struct ApiV1 {
    pub struct_size: u32,
    pub string_free: unsafe extern "C" fn(*mut c_char),
    pub set_log_file: unsafe extern "C" fn(*const c_char),
    pub set_default_font: unsafe extern "C" fn(*const c_char) -> i32,
    pub siglus_create:
        unsafe extern "C" fn(i32, *mut c_void, u32, u32, f64, *const c_char, *mut *mut c_char) -> *mut SiglusGame,
    pub siglus_step: unsafe extern "C" fn(*mut SiglusGame, u32) -> i32,
    pub siglus_resize: unsafe extern "C" fn(*mut SiglusGame, u32, u32, f64),
    pub siglus_screen_size: unsafe extern "C" fn(*const SiglusGame, *mut u32, *mut u32),
    pub siglus_pointer: unsafe extern "C" fn(*mut SiglusGame, f64, f64),
    pub siglus_button: unsafe extern "C" fn(*mut SiglusGame, i32, i32),
    pub siglus_wheel: unsafe extern "C" fn(*mut SiglusGame, i32),
    pub siglus_key: unsafe extern "C" fn(*mut SiglusGame, i32, i32, i32),
    pub siglus_text: unsafe extern "C" fn(*mut SiglusGame, *const c_char),
    pub siglus_wants_text: unsafe extern "C" fn(*const SiglusGame) -> i32,
    pub siglus_set_paused: unsafe extern "C" fn(*mut SiglusGame, i32),
    pub siglus_set_message_box: unsafe extern "C" fn(*mut SiglusGame, Option<MessageBoxFn>, *mut c_void),
    pub siglus_answer: unsafe extern "C" fn(*mut SiglusGame, u64, i64),
    pub siglus_destroy: unsafe extern "C" fn(*mut SiglusGame),
    pub reallive_open: unsafe extern "C" fn(*const c_char, *const c_char, *mut *mut c_char) -> *mut RealLiveGame,
    pub reallive_step: unsafe extern "C" fn(*mut RealLiveGame, u32) -> i32,
    pub reallive_frame: unsafe extern "C" fn(*const RealLiveGame, *mut u32, *mut u32) -> *const u8,
    pub reallive_pointer: unsafe extern "C" fn(*mut RealLiveGame, i32, i32),
    pub reallive_button: unsafe extern "C" fn(*mut RealLiveGame, i32, i32),
    pub reallive_wheel: unsafe extern "C" fn(*mut RealLiveGame, i32),
    pub reallive_key: unsafe extern "C" fn(*mut RealLiveGame, i32, i32),
    pub reallive_text: unsafe extern "C" fn(*mut RealLiveGame, *const c_char),
    pub reallive_cursor_visible: unsafe extern "C" fn(*const RealLiveGame) -> i32,
    pub reallive_set_paused: unsafe extern "C" fn(*mut RealLiveGame, i32),
    pub reallive_close: unsafe extern "C" fn(*mut RealLiveGame),
}

static API: ApiV1 = ApiV1 {
    struct_size: size_of::<ApiV1>() as u32,
    string_free,
    set_log_file,
    set_default_font,
    siglus_create,
    siglus_step,
    siglus_resize,
    siglus_screen_size,
    siglus_pointer,
    siglus_button,
    siglus_wheel,
    siglus_key,
    siglus_text,
    siglus_wants_text,
    siglus_set_paused,
    siglus_set_message_box,
    siglus_answer,
    siglus_destroy,
    reallive_open,
    reallive_step,
    reallive_frame,
    reallive_pointer,
    reallive_button,
    reallive_wheel,
    reallive_key,
    reallive_text,
    reallive_cursor_visible,
    reallive_set_paused,
    reallive_close,
};

#[unsafe(no_mangle)]
pub extern "C" fn vnsiglus_get_api_v1() -> *const ApiV1 {
    &API
}

// MARK: Shared

unsafe extern "C" fn string_free(string: *mut c_char) {
    if !string.is_null() {
        drop(unsafe { CString::from_raw(string) });
    }
}

/// A C string as a Rust one. Null and invalid UTF-8 read as missing.
unsafe fn text(string: *const c_char) -> Option<String> {
    if string.is_null() {
        return None;
    }
    unsafe { CStr::from_ptr(string) }.to_str().ok().map(str::to_owned)
}

fn report(error: *mut *mut c_char, failure: &anyhow::Error) {
    let message = format!("{failure:#}");
    log::error!("{message}");
    if let Some(slot) = unsafe { error.as_mut() } {
        *slot = CString::new(message.replace('\0', " ")).map_or(null_mut(), CString::into_raw);
    }
}

/// Writes the engines' `log` lines to the file the host names.
struct FileLog {
    file: Mutex<Option<File>>,
}

impl log::Log for FileLog {
    fn enabled(&self, metadata: &log::Metadata<'_>) -> bool {
        metadata.level() <= log::Level::Info
    }

    fn log(&self, record: &log::Record<'_>) {
        if !self.enabled(record.metadata()) {
            return;
        }
        if let Ok(mut file) = self.file.lock()
            && let Some(file) = file.as_mut()
        {
            let _ = writeln!(file, "[{}] {}: {}", record.level(), record.target(), record.args());
        }
    }

    fn flush(&self) {
        if let Ok(mut file) = self.file.lock()
            && let Some(file) = file.as_mut()
        {
            let _ = file.flush();
        }
    }
}

static LOG: FileLog = FileLog { file: Mutex::new(None) };

unsafe extern "C" fn set_log_file(path: *const c_char) {
    let file = unsafe { text(path) }.and_then(|path| OpenOptions::new().create(true).append(true).open(path).ok());
    if let Ok(mut current) = LOG.file.lock() {
        *current = file;
    }
    // Only the first call installs the logger. Later calls change the file it writes to.
    if log::set_logger(&LOG).is_ok() {
        log::set_max_level(log::LevelFilter::Info);
    }
}

unsafe extern "C" fn set_default_font(path: *const c_char) -> i32 {
    let Some(path) = (unsafe { text(path) }) else { return 0 };
    let Ok(bytes) = std::fs::read(&path) else { return 0 };
    // RealLive finds fonts the host registers.
    game_fs::add_host_font(bytes.clone());
    i32::from(siglus_scene_vm::text_render::set_default_font(bytes, path))
}

// MARK: SiglusEngine

pub struct SiglusGame {
    host: SiglusHost,
    /// The view's size in pixels and its pixels per point.
    surface: (u32, u32, f64),
}

unsafe extern "C" fn siglus_create(
    view_kind: i32,
    view: *mut c_void,
    width: u32,
    height: u32,
    scale: f64,
    root: *const c_char,
    error: *mut *mut c_char,
) -> *mut SiglusGame {
    let created = unsafe { text(root) }
        .ok_or_else(|| anyhow!("no game folder"))
        .and_then(|root| unsafe { create_siglus(view_kind, view, width, height, scale, Path::new(&root)) });
    match created {
        Ok(game) => Box::into_raw(Box::new(game)),
        Err(failure) => {
            report(error, &failure);
            null_mut()
        }
    }
}

unsafe fn create_siglus(
    view_kind: i32,
    view: *mut c_void,
    width: u32,
    height: u32,
    scale: f64,
    root: &Path,
) -> Result<SiglusGame> {
    use raw_window_handle::{
        AppKitDisplayHandle, AppKitWindowHandle, RawDisplayHandle, RawWindowHandle, UiKitDisplayHandle,
        UiKitWindowHandle,
    };
    let view = NonNull::new(view).ok_or_else(|| anyhow!("no view"))?;
    let (display, window) = match view_kind {
        VIEW_UIKIT => (
            RawDisplayHandle::UiKit(UiKitDisplayHandle::new()),
            RawWindowHandle::UiKit(UiKitWindowHandle::new(view)),
        ),
        VIEW_APPKIT => (
            RawDisplayHandle::AppKit(AppKitDisplayHandle::new()),
            RawWindowHandle::AppKit(AppKitWindowHandle::new(view)),
        ),
        other => bail!("unknown view kind {other}"),
    };
    let scale = if scale.is_finite() && scale > 0.0 { scale } else { 1.0 };
    let (width, height) = (width.max(1), height.max(1));
    let renderer =
        pollster::block_on(unsafe { Renderer::new_from_raw_handles(display, window, width, height, scale as f32) })
            .context("the renderer could not start")?;
    let host = SiglusHost::new_with_renderer_sync(SiglusHostConfig::new(root.to_path_buf()), renderer)
        .context("the game could not start")?;
    let mut game = SiglusGame { host, surface: (width, height, scale) };
    game.fit();
    Ok(game)
}

impl SiglusGame {
    /// Draws the game's screen at its own aspect ratio in the middle of the surface.
    fn fit(&mut self) {
        let (width, height, scale) = self.surface;
        let (screen_width, screen_height) = self.host.logical_size();
        let (x, y, fit_width, fit_height) = fit(width, height, screen_width, screen_height);
        self.host.resize_with_logical_viewport(
            width,
            height,
            scale as f32,
            screen_width,
            screen_height,
            x,
            y,
            fit_width,
            fit_height,
        );
    }

    fn save_global(&mut self) {
        siglus_scene_vm::runtime::forms::syscom::write_global_save(&self.host.vm_mut().ctx);
    }
}

/// The largest rectangle with the screen's aspect ratio that fits the surface, centered: x, y, width, height.
fn fit(width: u32, height: u32, screen_width: u32, screen_height: u32) -> (u32, u32, u32, u32) {
    let (width, height) = (width.max(1), height.max(1));
    let (screen_width, screen_height) = (screen_width.max(1), screen_height.max(1));
    let scale = (f64::from(width) / f64::from(screen_width)).min(f64::from(height) / f64::from(screen_height));
    let fit_width = ((f64::from(screen_width) * scale).round() as u32).clamp(1, width);
    let fit_height = ((f64::from(screen_height) * scale).round() as u32).clamp(1, height);
    ((width - fit_width) / 2, (height - fit_height) / 2, fit_width, fit_height)
}

unsafe fn siglus<'a>(game: *mut SiglusGame) -> Option<&'a mut SiglusGame> {
    unsafe { game.as_mut() }
}

unsafe extern "C" fn siglus_step(game: *mut SiglusGame, elapsed_ms: u32) -> i32 {
    let Some(game) = (unsafe { siglus(game) }) else { return FAILED };
    match game.host.step(elapsed_ms.max(1)) {
        Ok(false) => RUNNING,
        Ok(true) => ENDED,
        Err(failure) => {
            log::error!("SiglusEngine stopped: {failure:#}");
            FAILED
        }
    }
}

unsafe extern "C" fn siglus_resize(game: *mut SiglusGame, width: u32, height: u32, scale: f64) {
    let Some(game) = (unsafe { siglus(game) }) else { return };
    let scale = if scale.is_finite() && scale > 0.0 { scale } else { game.surface.2 };
    game.surface = (width.max(1), height.max(1), scale);
    game.fit();
}

unsafe extern "C" fn siglus_screen_size(game: *const SiglusGame, width: *mut u32, height: *mut u32) {
    let Some(game) = (unsafe { game.as_ref() }) else { return };
    let (screen_width, screen_height) = game.host.logical_size();
    if let Some(width) = unsafe { width.as_mut() } {
        *width = screen_width;
    }
    if let Some(height) = unsafe { height.as_mut() } {
        *height = screen_height;
    }
}

unsafe extern "C" fn siglus_pointer(game: *mut SiglusGame, x: f64, y: f64) {
    if let Some(game) = unsafe { siglus(game) } {
        game.host.mouse_move(x, y);
    }
}

unsafe extern "C" fn siglus_button(game: *mut SiglusGame, button: i32, down: i32) {
    let Some(game) = (unsafe { siglus(game) }) else { return };
    let button = match button {
        0 => VmMouseButton::Left,
        1 => VmMouseButton::Right,
        2 => VmMouseButton::Middle,
        _ => return,
    };
    if down != 0 { game.host.mouse_down(button) } else { game.host.mouse_up(button) }
}

/// Windows reports 120 a notch.
unsafe extern "C" fn siglus_wheel(game: *mut SiglusGame, notches: i32) {
    if let Some(game) = unsafe { siglus(game) } {
        game.host.mouse_wheel(notches.saturating_mul(120));
    }
}

unsafe extern "C" fn siglus_key(game: *mut SiglusGame, virtual_key: i32, down: i32, repeat: i32) {
    let Some(game) = (unsafe { siglus(game) }) else { return };
    let Some(key) = keys::siglus(virtual_key) else { return };
    match (down != 0, repeat != 0) {
        // A held Return, Space, or Escape doesn't repeat, as in upstream's hosts, so it can't run through menus.
        (true, true) if matches!(key, VmKey::Enter | VmKey::Space | VmKey::Escape) => {}
        (true, _) => game.host.key_down(key),
        (false, _) => game.host.key_up(key),
    }
}

unsafe extern "C" fn siglus_text(game: *mut SiglusGame, typed: *const c_char) {
    if let (Some(game), Some(typed)) = (unsafe { siglus(game) }, unsafe { text(typed) }) {
        game.host.text_input(&typed);
    }
}

unsafe extern "C" fn siglus_wants_text(game: *const SiglusGame) -> i32 {
    unsafe { game.as_ref() }.map_or(0, |game| i32::from(game.host.focused_editbox_ime_area().is_some()))
}

unsafe extern "C" fn siglus_set_paused(game: *mut SiglusGame, paused: i32) {
    let Some(game) = (unsafe { siglus(game) }) else { return };
    // iOS may end a paused app without warning, so its global data is saved now.
    if paused != 0 {
        game.save_global();
    }
    game.host.vm_mut().ctx.audio.set_muted(paused != 0);
}

unsafe extern "C" fn siglus_set_message_box(game: *mut SiglusGame, show: Option<MessageBoxFn>, context: *mut c_void) {
    if let Some(game) = unsafe { siglus(game) } {
        game.host.set_native_messagebox_callback(show, context);
    }
}

unsafe extern "C" fn siglus_answer(game: *mut SiglusGame, request: u64, button: i64) {
    if let Some(game) = unsafe { siglus(game) } {
        game.host.submit_native_messagebox_result(request, button);
    }
}

unsafe extern "C" fn siglus_destroy(game: *mut SiglusGame) {
    if game.is_null() {
        return;
    }
    let mut game = unsafe { Box::from_raw(game) };
    game.save_global();
}

// MARK: RealLive

pub struct RealLiveGame {
    engine: Engine,
    frame: Vec<u8>,
    size: (u32, u32),
    ended: bool,
}

unsafe extern "C" fn reallive_open(root: *const c_char, saves: *const c_char, error: *mut *mut c_char) -> *mut RealLiveGame {
    let opened = match (unsafe { text(root) }, unsafe { text(saves) }) {
        (Some(root), Some(saves)) => open_reallive(Path::new(&root), PathBuf::from(saves)),
        _ => Err(anyhow!("no game or save folder")),
    };
    match opened {
        Ok(game) => Box::into_raw(Box::new(game)),
        Err(failure) => {
            report(error, &failure);
            null_mut()
        }
    }
}

fn open_reallive(root: &Path, saves: PathBuf) -> Result<RealLiveGame> {
    let mut options = EngineOptions::new(root);
    options.save_dir = Some(saves);
    let engine = Engine::open(options).context("the game could not start")?;
    let size = (engine.machine.sys.gfx.width.max(1) as u32, engine.machine.sys.gfx.height.max(1) as u32);
    Ok(RealLiveGame { engine, frame: vec![0; size.0 as usize * size.1 as usize * 4], size, ended: false })
}

impl RealLiveGame {
    fn end(&mut self) {
        if !self.ended {
            self.save_global();
            self.ended = true;
        }
    }

    fn save_global(&mut self) {
        if let Err(failure) = reallive::save::save_global(&self.engine.machine) {
            log::error!("RealLive's global data wasn't saved: {failure:#}");
        }
    }
}

unsafe fn reallive<'a>(game: *mut RealLiveGame) -> Option<&'a mut RealLiveGame> {
    unsafe { game.as_mut() }
}

unsafe extern "C" fn reallive_step(game: *mut RealLiveGame, elapsed_ms: u32) -> i32 {
    let Some(game) = (unsafe { reallive(game) }) else { return FAILED };
    if game.ended {
        return ENDED;
    }
    game.engine.tick(u64::from(elapsed_ms));
    for failure in game.engine.machine.diagnostics.errors.drain(..) {
        log::warn!("RealLive: {failure}");
    }
    if game.engine.finished() {
        game.end();
        return ENDED;
    }
    let surface = game.engine.render();
    game.size = (surface.width.max(1) as u32, surface.height.max(1) as u32);
    game.frame = surface.rgba;
    RUNNING
}

unsafe extern "C" fn reallive_frame(game: *const RealLiveGame, width: *mut u32, height: *mut u32) -> *const u8 {
    let Some(game) = (unsafe { game.as_ref() }) else { return null() };
    if let Some(width) = unsafe { width.as_mut() } {
        *width = game.size.0;
    }
    if let Some(height) = unsafe { height.as_mut() } {
        *height = game.size.1;
    }
    game.frame.as_ptr()
}

unsafe extern "C" fn reallive_pointer(game: *mut RealLiveGame, x: i32, y: i32) {
    if let Some(game) = unsafe { reallive(game) } {
        game.engine.machine.sys.input.mouse = (x, y);
    }
}

unsafe extern "C" fn reallive_button(game: *mut RealLiveGame, button: i32, down: i32) {
    let Some(game) = (unsafe { reallive(game) }) else { return };
    let button = match button {
        0 => Button::Left,
        1 => Button::Right,
        _ => return,
    };
    game.engine.input(if down != 0 { InputEvent::Press(button) } else { InputEvent::Release(button) });
}

unsafe extern "C" fn reallive_wheel(game: *mut RealLiveGame, notches: i32) {
    let Some(game) = (unsafe { reallive(game) }) else { return };
    let button = if notches > 0 { Button::WheelUp } else { Button::WheelDown };
    for _ in 0..notches.unsigned_abs().min(16) {
        game.engine.input(InputEvent::Press(button));
    }
}

unsafe extern "C" fn reallive_key(game: *mut RealLiveGame, virtual_key: i32, down: i32) {
    let Some(game) = (unsafe { reallive(game) }) else { return };
    let Some(key) = keys::reallive(virtual_key) else { return };
    game.engine.input(if down != 0 { InputEvent::KeyDown(key) } else { InputEvent::KeyUp(key) });
}

unsafe extern "C" fn reallive_text(game: *mut RealLiveGame, typed: *const c_char) {
    if let (Some(game), Some(typed)) = (unsafe { reallive(game) }, unsafe { text(typed) }) {
        game.engine.input(InputEvent::Text(typed));
    }
}

unsafe extern "C" fn reallive_cursor_visible(game: *const RealLiveGame) -> i32 {
    unsafe { game.as_ref() }.map_or(1, |game| i32::from(game.engine.machine.sys.cursor_visible))
}

unsafe extern "C" fn reallive_set_paused(game: *mut RealLiveGame, paused: i32) {
    let Some(game) = (unsafe { reallive(game) }) else { return };
    if paused != 0 {
        game.save_global();
    }
    game.engine.machine.sys.sound.set_muted(paused != 0);
}

unsafe extern "C" fn reallive_close(game: *mut RealLiveGame) {
    if !game.is_null() {
        unsafe { Box::from_raw(game) }.end();
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fit_centers_the_screen() {
        assert_eq!(fit(1920, 1080, 1280, 720), (0, 0, 1920, 1080));
        assert_eq!(fit(2048, 1536, 1280, 720), (0, 192, 2048, 1152));
        assert_eq!(fit(1000, 1000, 800, 600), (0, 125, 1000, 750));
        assert_eq!(fit(2532, 1170, 1280, 720), (226, 0, 2080, 1170));
    }

    #[test]
    fn table_has_every_function() {
        let api = unsafe { &*vnsiglus_get_api_v1() };
        assert_eq!(api.struct_size as usize, size_of::<ApiV1>());
        // 28 functions after the size, each one pointer wide.
        assert_eq!(size_of::<ApiV1>(), 8 + 28 * size_of::<usize>());
    }

    #[test]
    fn missing_games_report_errors() {
        let mut error = null_mut();
        let root = CString::new("/nonexistent/vnsiglus").unwrap();
        let game = unsafe { reallive_open(root.as_ptr(), root.as_ptr(), &mut error) };
        assert!(game.is_null());
        assert!(!error.is_null());
        unsafe { string_free(error) };
        let mut error = null_mut();
        let game = unsafe { siglus_create(VIEW_APPKIT, null_mut(), 1, 1, 1.0, root.as_ptr(), &mut error) };
        assert!(game.is_null());
        let message = unsafe { CStr::from_ptr(error) }.to_str().unwrap().to_owned();
        assert_eq!(message, "no view");
        unsafe { string_free(error) };
    }
}
