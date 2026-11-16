use std::ffi::c_char;
use std::path::{Path, PathBuf};
use std::ptr::{null, null_mut};

use anyhow::{Context, Result, anyhow};
use reallive::engine::{Engine, EngineOptions};
use reallive::input::{Button, InputEvent, Key};

use super::{ENDED, FAILED, RUNNING, keys, report, text};

pub struct RealLiveGame {
    engine: Engine,
    frame: Vec<u8>,
    size: (u32, u32),
    ended: bool,
}

pub(crate) unsafe extern "C" fn reallive_open(root: *const c_char, saves: *const c_char, error: *mut *mut c_char) -> *mut RealLiveGame {
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

pub(crate) unsafe extern "C" fn reallive_step(game: *mut RealLiveGame, elapsed_ms: u32) -> i32 {
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

pub(crate) unsafe extern "C" fn reallive_frame(game: *const RealLiveGame, width: *mut u32, height: *mut u32) -> *const u8 {
    let Some(game) = (unsafe { game.as_ref() }) else { return null() };
    if let Some(width) = unsafe { width.as_mut() } {
        *width = game.size.0;
    }
    if let Some(height) = unsafe { height.as_mut() } {
        *height = game.size.1;
    }
    game.frame.as_ptr()
}

pub(crate) unsafe extern "C" fn reallive_pointer(game: *mut RealLiveGame, x: i32, y: i32) {
    if let Some(game) = unsafe { reallive(game) } {
        game.engine.machine.sys.input.mouse = (x, y);
    }
}

pub(crate) unsafe extern "C" fn reallive_button(game: *mut RealLiveGame, button: i32, down: i32) {
    let Some(game) = (unsafe { reallive(game) }) else { return };
    let button = match button {
        0 => Button::Left,
        1 => Button::Right,
        _ => return,
    };
    game.engine.input(if down != 0 { InputEvent::Press(button) } else { InputEvent::Release(button) });
}

pub(crate) unsafe extern "C" fn reallive_wheel(game: *mut RealLiveGame, notches: i32) {
    let Some(game) = (unsafe { reallive(game) }) else { return };
    let button = if notches > 0 { Button::WheelUp } else { Button::WheelDown };
    for _ in 0..notches.unsigned_abs().min(16) {
        game.engine.input(InputEvent::Press(button));
    }
}

pub(crate) unsafe extern "C" fn reallive_key(game: *mut RealLiveGame, virtual_key: i32, down: i32) {
    let Some(game) = (unsafe { reallive(game) }) else { return };
    let Some(key) = keys::reallive(virtual_key) else { return };
    game.engine.input(if down != 0 { InputEvent::KeyDown(key) } else { InputEvent::KeyUp(key) });
}

pub(crate) unsafe extern "C" fn reallive_text(game: *mut RealLiveGame, typed: *const c_char) {
    if let (Some(game), Some(typed)) = (unsafe { reallive(game) }, unsafe { text(typed) }) {
        game.engine.input(InputEvent::Text(typed));
    }
}

pub(crate) unsafe extern "C" fn reallive_cursor_visible(game: *const RealLiveGame) -> i32 {
    unsafe { game.as_ref() }.map_or(1, |game| i32::from(game.engine.machine.sys.cursor_visible))
}

pub(crate) unsafe extern "C" fn reallive_set_paused(game: *mut RealLiveGame, paused: i32) {
    let Some(game) = (unsafe { reallive(game) }) else { return };
    if paused != 0 {
        game.save_global();
    }
    game.engine.machine.sys.sound.set_muted(paused != 0);
}

pub(crate) unsafe extern "C" fn reallive_close(game: *mut RealLiveGame) {
    if !game.is_null() {
        unsafe { Box::from_raw(game) }.end();
    }
}
