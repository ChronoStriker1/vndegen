#![cfg(feature = "ffmpeg")]
use art3m1s_media::{MediaSource, ffmpeg::FfmpegVideoDecoder};
use std::sync::Arc;
struct Bytes(Vec<u8>);
impl MediaSource for Bytes {
    fn len(&self) -> Result<u64, String> {
        Ok(self.0.len() as u64)
    }
    fn read_at(&self, offset: u64, out: &mut [u8]) -> Result<usize, String> {
        let start = (offset as usize).min(self.0.len());
        let n = out.len().min(self.0.len() - start);
        out[..n].copy_from_slice(&self.0[start..start + n]);
        Ok(n)
    }
}
#[cfg(windows)]
fn enable_heap_corruption_detection() {
    #[link(name = "kernel32")]
    unsafe extern "system" {
        fn HeapSetInformation(
            heap: *mut core::ffi::c_void,
            class: u32,
            info: *const core::ffi::c_void,
            len: usize,
        ) -> i32;
    }
    assert_ne!(
        unsafe { HeapSetInformation(core::ptr::null_mut(), 1, core::ptr::null(), 0) },
        0
    );
}
#[test]
fn invalid_media_releases_custom_avio_without_heap_corruption() {
    #[cfg(windows)]
    enable_heap_corruption_detection();
    for _ in 0..3 {
        assert!(FfmpegVideoDecoder::open(Arc::new(Bytes(vec![b'A'; 1024]))).is_err());
    }
}
