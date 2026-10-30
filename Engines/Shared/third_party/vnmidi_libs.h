// The libraries vnmidi.c compiles into itself: TinySoundFont, TinyMidiLoader, and stb_vorbis, which TinySoundFont uses
// to decode SF3 samples. Their functions get internal linkage, or a vnmidi_ prefix, so an engine that compiles its own
// copies links beside vnmidi without duplicate symbols. The pragma keeps the compiler's warnings about upstream code,
// which stays unmodified, out of builds that turn warnings into errors.
#pragma GCC system_header
// GCC reports this one after inlining, where the system header no longer hides it.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#define stb_vorbis_close vnmidi_stb_vorbis_close
#define stb_vorbis_decode_memory vnmidi_stb_vorbis_decode_memory
#define stb_vorbis_get_comment vnmidi_stb_vorbis_get_comment
#define stb_vorbis_get_error vnmidi_stb_vorbis_get_error
#define stb_vorbis_get_file_offset vnmidi_stb_vorbis_get_file_offset
#define stb_vorbis_get_frame_float vnmidi_stb_vorbis_get_frame_float
#define stb_vorbis_get_frame_short vnmidi_stb_vorbis_get_frame_short
#define stb_vorbis_get_frame_short_interleaved vnmidi_stb_vorbis_get_frame_short_interleaved
#define stb_vorbis_get_info vnmidi_stb_vorbis_get_info
#define stb_vorbis_get_sample_offset vnmidi_stb_vorbis_get_sample_offset
#define stb_vorbis_get_samples_float vnmidi_stb_vorbis_get_samples_float
#define stb_vorbis_get_samples_float_interleaved vnmidi_stb_vorbis_get_samples_float_interleaved
#define stb_vorbis_get_samples_short vnmidi_stb_vorbis_get_samples_short
#define stb_vorbis_get_samples_short_interleaved vnmidi_stb_vorbis_get_samples_short_interleaved
#define stb_vorbis_open_memory vnmidi_stb_vorbis_open_memory
#define stb_vorbis_seek vnmidi_stb_vorbis_seek
#define stb_vorbis_seek_frame vnmidi_stb_vorbis_seek_frame
#define stb_vorbis_seek_start vnmidi_stb_vorbis_seek_start
#define stb_vorbis_stream_length_in_samples vnmidi_stb_vorbis_stream_length_in_samples
#define stb_vorbis_stream_length_in_seconds vnmidi_stb_vorbis_stream_length_in_seconds
#define TSF_STATIC
#define TML_STATIC
#include "stb_vorbis/stb_vorbis.c"
#define TSF_IMPLEMENTATION
#include "tsf/tsf.h"
#define TML_IMPLEMENTATION
#include "tsf/tml.h"
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
