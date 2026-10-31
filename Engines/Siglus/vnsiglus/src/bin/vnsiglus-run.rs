//! Linux test host for VNDegen's C interface: runs a RealLive game headless through the same table the app calls,
//! for a number of frames, pressing Return now and then, and writes the last frame as a PNG.
//!
//!   vnsiglus-run <game folder> <save folder> <frames> <out.png> [font]
//!
//! SiglusEngine games need a view, so on Linux they run in upstream's own player, `siglus_engine`, which drives the
//! same VM.

use std::ffi::{CStr, CString};
use std::ptr::null_mut;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 5 {
        eprintln!("usage: vnsiglus-run <game folder> <save folder> <frames> <out.png> [font]");
        std::process::exit(2);
    }
    let api = unsafe { &*vnsiglus::vnsiglus_get_api_v1() };
    let text = |s: &str| CString::new(s).unwrap();
    unsafe { (api.set_log_file)(text("/dev/stderr").as_ptr()) };
    if let Some(font) = args.get(5) {
        println!("font {}", unsafe { (api.set_default_font)(text(font).as_ptr()) });
    }
    let frames: u32 = args[3].parse().expect("frames");
    let mut error = null_mut();
    let game = unsafe { (api.reallive_open)(text(&args[1]).as_ptr(), text(&args[2]).as_ptr(), &mut error) };
    if game.is_null() {
        eprintln!("open failed: {}", unsafe { CStr::from_ptr(error) }.to_string_lossy());
        unsafe { (api.string_free)(error) };
        std::process::exit(1);
    }
    let mut status = vnsiglus::RUNNING;
    let mut ran = 0;
    for frame in 0..frames {
        status = unsafe { (api.reallive_step)(game, 16) };
        ran = frame + 1;
        if status != vnsiglus::RUNNING {
            break;
        }
        // Return every two seconds, to read on.
        if frame % 120 == 119 {
            unsafe { (api.reallive_key)(game, 0x0D, 1) };
            unsafe { (api.reallive_key)(game, 0x0D, 0) };
        }
    }
    let (mut width, mut height) = (0, 0);
    let pixels = unsafe { (api.reallive_frame)(game, &mut width, &mut height) };
    let bytes = unsafe { std::slice::from_raw_parts(pixels, width as usize * height as usize * 4) };
    image::save_buffer(&args[4], bytes, width, height, image::ColorType::Rgba8).expect("write png");
    println!("frames {ran} status {status} size {width}x{height}");
    unsafe { (api.reallive_close)(game) };
}
