//! The types of siglus_rs's WMV and WMA decoder that siglus_scene_vm uses, with no decoder behind them. Opening any
//! file fails, so the engine treats every `.wmv` movie as one it can't play.

use std::io::{Read, Seek};
use std::marker::PhantomData;

#[derive(Debug)]
pub struct DecoderError;

impl std::fmt::Display for DecoderError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("WMV and WMA files are not supported in this build")
    }
}

impl std::error::Error for DecoderError {}

pub type Result<T> = std::result::Result<T, DecoderError>;

#[derive(Clone, Debug)]
pub struct YuvFrame {
    pub width: u32,
    pub height: u32,
    pub y: Vec<u8>,
    pub cb: Vec<u8>,
    pub cr: Vec<u8>,
}

#[derive(Clone, Debug)]
pub struct DecodedFrame {
    pub pts_ms: u32,
    pub is_key_frame: bool,
    pub frame: YuvFrame,
}

#[derive(Clone, Debug)]
pub struct PcmFrameF32 {
    pub pts_ms: u32,
    pub sample_rate: u32,
    pub channels: u16,
    pub samples: Vec<f32>,
}

#[derive(Clone, Debug)]
pub struct DecodedAudioFrame {
    pub pts_ms: u32,
    pub frame: PcmFrameF32,
}

pub fn yuv420p_to_rgba_scaled(_frame: &YuvFrame, output_width: u32, output_height: u32) -> Vec<u8> {
    vec![0; output_width as usize * output_height as usize * 4]
}

pub mod asf {
    use super::Result;
    use std::io::{Read, Seek};

    #[derive(Clone, Debug)]
    pub struct VideoStreamInfo {
        pub stream_number: u8,
        pub width: u32,
        pub height: u32,
        pub codec_four_cc: [u8; 4],
        pub extra_data: Vec<u8>,
    }

    #[derive(Clone, Debug)]
    pub struct AudioStreamInfo {
        pub stream_number: u8,
        pub format_tag: u16,
        pub channels: u16,
        pub sample_rate: u32,
        pub bit_rate: u32,
        pub block_align: u16,
        pub bits_per_sample: u16,
        pub extra_data: Vec<u8>,
    }

    #[derive(Clone, Debug)]
    pub struct AsfFile {
        pub video_streams: Vec<VideoStreamInfo>,
        pub audio_streams: Vec<AudioStreamInfo>,
        pub play_duration_ms: Option<u64>,
    }

    impl AsfFile {
        pub fn open<R: Read + Seek>(_reader: &mut R) -> Result<Self> {
            Err(super::DecoderError)
        }
    }
}

pub struct AsfWmv2Decoder<R: Read + Seek> {
    info: asf::VideoStreamInfo,
    reader: PhantomData<R>,
}

impl<R: Read + Seek> AsfWmv2Decoder<R> {
    pub fn open(_reader: R) -> Result<Self> {
        Err(DecoderError)
    }

    pub fn video_stream_info(&self) -> &asf::VideoStreamInfo {
        &self.info
    }

    pub fn duration_ms(&self) -> Option<u64> {
        None
    }

    pub fn next_frame(&mut self) -> Result<Option<DecodedFrame>> {
        Err(DecoderError)
    }
}

pub struct AsfWmaDecoder<R: Read + Seek> {
    reader: PhantomData<R>,
}

impl<R: Read + Seek> AsfWmaDecoder<R> {
    pub fn open(_reader: R) -> Result<Self> {
        Err(DecoderError)
    }

    pub fn sample_rate(&self) -> u32 {
        0
    }

    pub fn channels(&self) -> u16 {
        0
    }

    pub fn duration_ms(&self) -> Option<u64> {
        None
    }

    pub fn next_frame(&mut self) -> Result<Option<DecodedAudioFrame>> {
        Err(DecoderError)
    }
}
