use std::ffi::{c_char, c_int, c_void};
use std::fs::File;
use std::io::{Read, Seek, SeekFrom};
use std::path::Path;
use std::sync::{Arc, Mutex, OnceLock};
use std::time::{Duration, Instant};

use anyhow::{Context, Result, bail};
use kira::Frame;
use kira::sound::streaming::Decoder as KiraStreamingDecoder;

use crate::assets::RgbaImage;

pub type ReadFn = unsafe extern "C" fn(*mut c_void, u64, *mut c_void, usize) -> i64;

#[repr(C)]
#[derive(Debug, Default, Clone, Copy)]
pub struct HostMovieInfo {
    pub width: c_int,
    pub height: c_int,
    pub fps: f64,
    pub duration: f64,
    pub audio_rate: c_int,
    pub audio_channels: c_int,
}

#[derive(Clone, Copy)]
pub struct HostMovieApi {
    pub open: unsafe extern "C" fn(ReadFn, *mut c_void, u64, c_int, *mut HostMovieInfo, *mut c_char, usize) -> *mut c_void,
    pub frame: unsafe extern "C" fn(*mut c_void, f64, *mut c_int, *mut c_int) -> *const u8,
    pub audio: unsafe extern "C" fn(*mut c_void, *mut f32, usize) -> usize,
    pub seek: unsafe extern "C" fn(*mut c_void, f64),
    pub close: unsafe extern "C" fn(*mut c_void),
}

static API: OnceLock<HostMovieApi> = OnceLock::new();

pub fn set_host_movie_api(api: HostMovieApi) {
    let _ = API.set(api);
}

pub(crate) fn available() -> bool {
    API.get().is_some()
}

struct Movie {
    api: HostMovieApi,
    handle: *mut c_void,
    _file: Box<Mutex<File>>,
}

unsafe impl Send for Movie {}

impl Drop for Movie {
    fn drop(&mut self) {
        unsafe { (self.api.close)(self.handle) };
    }
}

pub(crate) struct HostMovie {
    movie: Mutex<Movie>,
    pub info: HostMovieInfo,
}

impl std::fmt::Debug for HostMovie {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("HostMovie").field("info", &self.info).finish()
    }
}

unsafe extern "C" fn read_file(user: *mut c_void, offset: u64, buffer: *mut c_void, length: usize) -> i64 {
    let file = unsafe { &*(user as *const Mutex<File>) };
    let out = unsafe { std::slice::from_raw_parts_mut(buffer as *mut u8, length) };
    let mut file = file.lock().unwrap_or_else(|poisoned| poisoned.into_inner());
    match file.seek(SeekFrom::Start(offset)).and_then(|_| file.read(out)) {
        Ok(read) => read as i64,
        Err(_) => -1,
    }
}

impl HostMovie {
    pub(crate) fn open(path: &Path, looping: bool) -> Result<Arc<Self>> {
        let Some(api) = API.get().copied() else { bail!("no movie decoder for {}", path.display()) };
        let opened = File::open(path).with_context(|| format!("open movie {}", path.display()))?;
        let size = opened.metadata()?.len();
        let file = Box::new(Mutex::new(opened));
        let mut info = HostMovieInfo::default();
        let mut error = [0 as c_char; 256];
        let user = &*file as *const Mutex<File> as *mut c_void;
        let handle = unsafe {
            (api.open)(read_file, user, size, c_int::from(looping), &mut info, error.as_mut_ptr(), error.len())
        };
        if handle.is_null() {
            let reason = unsafe { std::ffi::CStr::from_ptr(error.as_ptr()) }.to_string_lossy().into_owned();
            bail!("movie decoder can't play {}: {reason}", path.display());
        }
        Ok(Arc::new(Self { movie: Mutex::new(Movie { api, handle, _file: file }), info }))
    }

    pub(crate) fn duration_ms(&self) -> Option<u64> {
        (self.info.duration > 0.0).then(|| (self.info.duration * 1000.0).round() as u64)
    }

    pub(crate) fn frame(&self, time_ms: u64) -> (Option<RgbaImage>, bool) {
        let movie = self.movie.lock().unwrap_or_else(|poisoned| poisoned.into_inner());
        let (mut changed, mut ended) = (0, 0);
        let pixels = unsafe { (movie.api.frame)(movie.handle, time_ms as f64 / 1000.0, &mut changed, &mut ended) };
        if pixels.is_null() || changed == 0 {
            return (None, ended != 0);
        }
        let (width, height) = (self.info.width.max(1) as u32, self.info.height.max(1) as u32);
        let length = width as usize * height as usize * 4;
        let rgba = unsafe { std::slice::from_raw_parts(pixels, length) }.to_vec();
        (Some(RgbaImage { width, height, center_x: 0, center_y: 0, rgba }), ended != 0)
    }

    pub(crate) fn first_frame(&self) -> Result<RgbaImage> {
        let started = Instant::now();
        loop {
            if let (Some(frame), _) = self.frame(0) {
                return Ok(frame);
            }
            if started.elapsed() > Duration::from_secs(5) {
                bail!("movie decoder gave no first frame");
            }
            std::thread::sleep(Duration::from_millis(5));
        }
    }

    fn audio(&self, out: &mut [f32], frames: usize) -> usize {
        let movie = self.movie.lock().unwrap_or_else(|poisoned| poisoned.into_inner());
        unsafe { (movie.api.audio)(movie.handle, out.as_mut_ptr(), frames) }
    }

    pub(crate) fn seek(&self, time_ms: u64) {
        let movie = self.movie.lock().unwrap_or_else(|poisoned| poisoned.into_inner());
        unsafe { (movie.api.seek)(movie.handle, time_ms as f64 / 1000.0) };
    }
}

const DECODE_FRAMES: usize = 1024;

pub(crate) struct HostMovieAudioDecoder {
    movie: Arc<HostMovie>,
    frames: usize,
    looping: bool,
    produced: usize,
    buffer: Vec<f32>,
}

impl HostMovieAudioDecoder {
    pub(crate) fn new(movie: Arc<HostMovie>, looping: bool) -> Self {
        let rate = movie.info.audio_rate.max(1) as usize;
        let frames = if looping {
            usize::MAX / 2
        } else {
            movie.duration_ms().map_or(usize::MAX / 2, |ms| (ms as usize).saturating_mul(rate) / 1000)
        };
        Self { movie, frames, looping, produced: 0, buffer: Vec::new() }
    }
}

impl KiraStreamingDecoder for HostMovieAudioDecoder {
    type Error = anyhow::Error;

    fn sample_rate(&self) -> u32 {
        self.movie.info.audio_rate.max(1) as u32
    }

    fn num_frames(&self) -> usize {
        self.frames
    }

    fn decode(&mut self) -> Result<Vec<Frame>, Self::Error> {
        let wanted = self.frames.saturating_sub(self.produced).min(DECODE_FRAMES);
        if wanted == 0 {
            return Ok(Vec::new());
        }
        let channels = self.movie.info.audio_channels.max(1) as usize;
        self.buffer.clear();
        self.buffer.resize(wanted * channels, 0.0);
        let got = self.movie.audio(&mut self.buffer, wanted);
        self.buffer[got.min(wanted) * channels..].fill(0.0);
        self.produced += wanted;
        Ok(self
            .buffer
            .chunks_exact(channels)
            .map(|sample| match sample {
                [mono] => Frame::from_mono(*mono),
                [left, right, ..] => Frame::new(*left, *right),
                [] => Frame::ZERO,
            })
            .collect())
    }

    fn seek(&mut self, index: usize) -> Result<usize, Self::Error> {
        if !self.looping || index != 0 {
            let rate = self.movie.info.audio_rate.max(1) as u64;
            self.movie.seek(index as u64 * 1000 / rate);
        }
        self.produced = index;
        Ok(index)
    }
}
