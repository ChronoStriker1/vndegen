#import <AVFoundation/AVFoundation.h>
#import <Accelerate/Accelerate.h>
#import <VideoToolbox/VideoToolbox.h>
#include "vnalpha.h"
#include "third_party/libwebm/mkvparser/mkvreader.h"
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"
#include "third_party/opus/include/opus.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/resource.h>
#include <thread>
#include <vector>

struct vnalpha_control { std::atomic<bool> cancelled{false}; };
vnalpha_control *vnalpha_control_create() { return new vnalpha_control; }
void vnalpha_control_cancel(vnalpha_control *control) { control->cancelled.store(true); }
void vnalpha_control_destroy(vnalpha_control *control) { delete control; }
namespace {
struct Element { long long id, start, end; };
struct Track {
    long long number = 0, type = 0, width = 0, height = 0, alpha = 0, delay = 0;
    std::string codec;
    std::vector<uint8_t> header;
};
struct Decoder {
    vpx_codec_ctx_t context{};
    Decoder() {
        vpx_codec_dec_cfg_t config{};
        config.threads = 4;
        if (vpx_codec_dec_init(&context, vpx_codec_vp9_dx(), &config, 0))
            throw std::runtime_error("VP9 decoder initialization failed");
        vpx_codec_control(&context, VP9D_SET_ROW_MT, 1);
    }
    ~Decoder() { vpx_codec_destroy(&context); }
    vpx_image_t *decode(const std::vector<uint8_t>& packet) {
        if (vpx_codec_decode(&context, packet.data(), static_cast<unsigned>(packet.size()), nullptr, 0))
            throw std::runtime_error(vpx_codec_error(&context));
        vpx_codec_iter_t iterator = nullptr;
        auto image = vpx_codec_get_frame(&context, &iterator);
        if (!image || image->fmt != VPX_IMG_FMT_I420 || vpx_codec_get_frame(&context, &iterator))
            throw std::runtime_error("Expected one 8-bit I420 frame per VP9 packet");
        return image;
    }
};
class Conversion {
    mkvparser::MkvReader reader;
    Track track;
    long long video_track = 0, scale = 1000000, cluster_time = 0;
    double duration_ticks = 0;
    Decoder color, alpha;
    OpusDecoder *opus = nullptr;
    long long audio_track = 0, audio_delay = 0;
    int channels = 0, preskip = 0;
    bool probing = true;
    bool headers_only = false;
    vnalpha_control *control;
    int video_packets = 0, audio_packets = 0;
    CMAudioFormatDescriptionRef audio_format = nullptr;
    AVAssetWriter *writer = nil;
    AVAssetWriterInput *input = nil;
    AVAssetWriterInput *audio_input = nil;
    AVAssetWriterInputPixelBufferAdaptor *adaptor = nil;
    vnalpha_result& result;
    std::string destination;
    void checkCancelled() {
        if (control && control->cancelled.load()) throw std::runtime_error("Conversion cancelled");
    }
    Element next(long long& position, long long end) {
        long long id, size;
        if (mkvparser::ParseElementHeader(&reader, position, end, id, size))
            throw std::runtime_error("Invalid WebM element");
        // Unknown segment lengths are common in WebM. Nested elements must have a bounded size.
        if (size > end - position && id == 0x18538067) size = end - position;
        if (size < 0 || size > end - position) throw std::runtime_error("Truncated WebM element");
        Element element{id, position, position + size};
        position += size;
        return element;
    }
    long long number(Element element) {
        auto value = mkvparser::UnserializeUInt(&reader, element.start, element.end - element.start);
        if (value < 0) throw std::runtime_error("Invalid WebM integer");
        return value;
    }
    std::vector<uint8_t> bytes(Element element) {
        auto length = element.end - element.start;
        if (length > 64 * 1024 * 1024) throw std::runtime_error("WebM packet exceeds 64 MiB");
        std::vector<uint8_t> data(static_cast<size_t>(length));
        if (reader.Read(element.start, static_cast<long>(length), data.data()))
            throw std::runtime_error("Cannot read WebM packet");
        return data;
    }
    void check(bool success) {
        if (!success) throw std::runtime_error(writer.error.localizedDescription.UTF8String ?: "HEVC writer failed");
    }
    void openWriter() {
        if (!video_track || result.width < 1 || result.height < 1 || result.width > 8192 || result.height > 8192)
            throw std::runtime_error("Missing or unsupported VP9-alpha video track");
        NSError *error = nil;
        writer = [[AVAssetWriter alloc] initWithURL:[NSURL fileURLWithPath:@(destination.c_str())]
                                          fileType:AVFileTypeQuickTimeMovie error:&error];
        if (!writer) throw std::runtime_error(error.localizedDescription.UTF8String);
        NSDictionary *settings = @{AVVideoCodecKey: AVVideoCodecTypeHEVCWithAlpha,
            AVVideoWidthKey: @(result.width), AVVideoHeightKey: @(result.height),
            AVVideoCompressionPropertiesKey: @{AVVideoAllowFrameReorderingKey: @NO,
                AVVideoAverageBitRateKey: @(result.width * result.height * 6),
                (__bridge NSString *)kVTCompressionPropertyKey_TargetQualityForAlpha: @1.0}};
        input = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo outputSettings:settings];
        input.mediaTimeScale = 1000000000;
        NSDictionary *attributes = @{(id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
            (id)kCVPixelBufferWidthKey: @(result.width), (id)kCVPixelBufferHeightKey: @(result.height),
            (id)kCVPixelBufferIOSurfacePropertiesKey: @{}};
        adaptor = [AVAssetWriterInputPixelBufferAdaptor assetWriterInputPixelBufferAdaptorWithAssetWriterInput:input
                                                                                  sourcePixelBufferAttributes:attributes];
        check([writer canAddInput:input]);
        [writer addInput:input];
        if (opus) {
            audio_input = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeAudio
                outputSettings:@{AVFormatIDKey: @(kAudioFormatMPEG4AAC), AVSampleRateKey: @48000,
                                 AVNumberOfChannelsKey: @(channels), AVEncoderBitRateKey: @128000}
                sourceFormatHint:audio_format];
            check([writer canAddInput:audio_input]);
            [writer addInput:audio_input];
        }
        check([writer startWriting]);
        [writer startSessionAtSourceTime:kCMTimeZero];
    }
    void audio(const std::vector<uint8_t>& packet, int16_t relative) {
        std::vector<float> pcm(5760 * channels);
        int count = opus_decode_float(opus, packet.data(), static_cast<int>(packet.size()), pcm.data(), 5760, 0);
        if (count < 0) throw std::runtime_error("Invalid Opus packet");
        int skipped = std::min(count, preskip); preskip -= skipped; count -= skipped;
        if (!count) return;
        if (!writer) openWriter();
        while (!audio_input.readyForMoreMediaData && writer.status == AVAssetWriterStatusWriting) {
            checkCancelled();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        check(writer.status == AVAssetWriterStatusWriting);
        size_t length = count * channels * sizeof(float), sample_size = channels * sizeof(float);
        CMBlockBufferRef block = nullptr;
        CMSampleBufferRef sample = nullptr;
        auto time = CMTimeAdd(CMTimeMake((cluster_time + relative) * scale - audio_delay, 1000000000), CMTimeMake(skipped, 48000));
        CMSampleTimingInfo timing{CMTimeMake(1, 48000), time, kCMTimeInvalid};
        check(CMBlockBufferCreateWithMemoryBlock(nullptr, nullptr, length, nullptr, nullptr, 0, length, 0, &block) == noErr);
        auto copied = CMBlockBufferReplaceDataBytes(pcm.data() + skipped * channels, block, 0, length);
        auto created = CMSampleBufferCreateReady(nullptr, block, audio_format, count, 1, &timing, 1, &sample_size, &sample);
        bool accepted = copied == noErr && created == noErr && [audio_input appendSampleBuffer:sample];
        if (sample) CFRelease(sample);
        CFRelease(block);
        check(accepted);
    }
    void frame(Element block, Element additional) {
        long length = 0;
        auto track_number = mkvparser::ReadUInt(&reader, block.start, length);
        if (track_number != video_track && track_number != audio_track) return;
        if (probing) {
            if (track_number == video_track) ++video_packets; else ++audio_packets;
            return;
        }
        auto packet = bytes(block);
        if (length < 1 || packet.size() < static_cast<size_t>(length + 3) || packet[length + 2] & 6)
            throw std::runtime_error("Unsupported laced WebM video block");
        int16_t relative = static_cast<int16_t>((packet[length] << 8) | packet[length + 1]);
        packet.erase(packet.begin(), packet.begin() + length + 3);
        if (track_number == audio_track) {
            audio(packet, relative);
            if (--audio_packets == 0) [audio_input markAsFinished];
            return;
        }
        if (additional.end <= additional.start) throw std::runtime_error("Missing WebM alpha packet");
        auto yuv = color.decode(packet), mask = alpha.decode(bytes(additional));
        if (yuv->d_w != result.width || yuv->d_h != result.height || mask->d_w != yuv->d_w || mask->d_h != yuv->d_h)
            throw std::runtime_error("WebM frame dimensions changed");
        if (!writer) openWriter();
        while (!input.readyForMoreMediaData && writer.status == AVAssetWriterStatusWriting) {
            checkCancelled();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        check(writer.status == AVAssetWriterStatusWriting);
        CVPixelBufferRef buffer = nullptr;
        NSDictionary *limit = @{(__bridge NSString *)kCVPixelBufferPoolAllocationThresholdKey: @8};
        CVReturn allocated;
        while ((allocated = CVPixelBufferPoolCreatePixelBufferWithAuxAttributes(nullptr, adaptor.pixelBufferPool,
                (__bridge CFDictionaryRef)limit, &buffer)) == kCVReturnWouldExceedAllocationThreshold) {
            checkCancelled();
            check(writer.status == AVAssetWriterStatusWriting);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        check(allocated == kCVReturnSuccess);
        CVPixelBufferLockBaseAddress(buffer, 0);
        vImage_Buffer planes[3];
        for (int plane = 0; plane < 3; ++plane)
            planes[plane] = {yuv->planes[plane], static_cast<vImagePixelCount>((result.height + (plane ? 1 : 0)) >> (plane ? 1 : 0)),
                            static_cast<vImagePixelCount>((result.width + (plane ? 1 : 0)) >> (plane ? 1 : 0)),
                            static_cast<size_t>(yuv->stride[plane])};
        vImage_Buffer rgba{CVPixelBufferGetBaseAddress(buffer), static_cast<vImagePixelCount>(result.height),
                           static_cast<vImagePixelCount>(result.width), CVPixelBufferGetBytesPerRow(buffer)};
        vImage_YpCbCrPixelRange range = yuv->range == VPX_CR_FULL_RANGE
            ? vImage_YpCbCrPixelRange{0, 128, 255, 255, 255, 0, 255, 0}
            : vImage_YpCbCrPixelRange{16, 128, 235, 240, 255, 0, 255, 0};
        vImage_YpCbCrToARGB matrix;
        auto standard = yuv->cs == VPX_CS_BT_709 ? kvImage_YpCbCrToARGBMatrix_ITU_R_709_2 : kvImage_YpCbCrToARGBMatrix_ITU_R_601_4;
        vImageConvert_YpCbCrToARGB_GenerateConversion(standard, &range, &matrix, kvImage420Yp8_Cb8_Cr8, kvImageARGB8888, 0);
        const uint8_t permute[] = {3, 2, 1, 0};
        auto converted = vImageConvert_420Yp8_Cb8_Cr8ToARGB8888(&planes[0], &planes[1], &planes[2], &rgba, &matrix, permute, 255, 0);
        for (int y = 0; y < result.height; ++y) {
            auto pixels = static_cast<uint8_t *>(rgba.data) + y * rgba.rowBytes;
            for (int x = 0; x < result.width; ++x) pixels[x * 4 + 3] = mask->planes[0][y * mask->stride[0] + x];
        }
        vImagePremultiplyData_BGRA8888(&rgba, &rgba, 0);
        CVPixelBufferUnlockBaseAddress(buffer, 0);
        bool accepted = converted == kvImageNoError && [adaptor appendPixelBuffer:buffer
            withPresentationTime:CMTimeMake((cluster_time + relative) * scale, 1000000000)];
        CVPixelBufferRelease(buffer);
        check(accepted);
        ++result.frames;
        if (--video_packets == 0) [input markAsFinished];
    }
    void group(Element element) {
        Element block{}, additional{};
        for (auto position = element.start; position < element.end;) {
            auto child = next(position, element.end);
            if (child.id == 0xa1) block = child;
            if (child.id != 0x75a1) continue;
            for (auto a = child.start; a < child.end;) {
                auto more = next(a, child.end);
                if (more.id != 0xa6) continue;
                long long id = 1;
                Element payload{};
                for (auto b = more.start; b < more.end;) {
                    auto item = next(b, more.end);
                    if (item.id == 0xee) id = number(item);
                    if (item.id == 0xa5) payload = item;
                }
                if (id == 1) additional = payload;
            }
        }
        if (block.end > block.start) frame(block, additional);
    }
    void walk(long long start, long long end, int depth = 0) {
        if (depth > 6) throw std::runtime_error("Excessive WebM nesting");
        for (auto position = start; position < end;) {
            checkCancelled();
            auto element = next(position, end);
            switch (element.id) {
            case 0x1654ae6b:
                if (probing) walk(element.start, element.end, depth + 1);
                break;
            case 0x18538067: case 0x1549a966: case 0xe0:
                walk(element.start, element.end, depth + 1); break;
            case 0xae:
                track = {}; walk(element.start, element.end, depth + 1);
                if (track.type == 2 && !headers_only) {
                    if (track.codec != "A_OPUS" || track.header.size() < 19 || track.header[18] != 0)
                        throw std::runtime_error("Only mono/stereo Opus audio is supported");
                    channels = track.header[9];
                    preskip = track.header[10] | track.header[11] << 8;
                    int status = 0; opus = opus_decoder_create(48000, channels, &status);
                    if (!opus || status) throw std::runtime_error("Opus decoder initialization failed");
                    audio_track = track.number; audio_delay = track.delay;
                    AudioStreamBasicDescription format{48000, kAudioFormatLinearPCM,
                        kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked, static_cast<UInt32>(channels * 4), 1,
                        static_cast<UInt32>(channels * 4), static_cast<UInt32>(channels), 32, 0};
                    if (CMAudioFormatDescriptionCreate(nullptr, &format, 0, nullptr, 0, nullptr, nullptr, &audio_format))
                        throw std::runtime_error("Invalid Opus PCM format");
                }
                if (track.type == 1 && track.codec == "V_VP9" && track.alpha == 1) {
                    video_track = track.number; result.width = static_cast<int>(track.width); result.height = static_cast<int>(track.height);
                }
                break;
            case 0x1f43b675:
                if (headers_only) return;
                cluster_time = 0; walk(element.start, element.end, depth + 1); break;
            case 0x2ad7b1: scale = number(element); break;
            case 0x4489:
                if (mkvparser::UnserializeFloat(&reader, element.start, element.end - element.start, duration_ticks))
                    throw std::runtime_error("Invalid WebM duration");
                break;
            case 0xd7: track.number = number(element); break;
            case 0x83: track.type = number(element); break;
            case 0xb0: track.width = number(element); break;
            case 0xba: track.height = number(element); break;
            case 0x53c0: track.alpha = number(element); break;
            case 0x86: { auto data = bytes(element); track.codec.assign(data.begin(), data.end()); break; }
            case 0x63a2: track.header = bytes(element); break;
            case 0x56aa: track.delay = number(element); break;
            case 0xe7: cluster_time = number(element); break;
            case 0xa0: @autoreleasepool { group(element); } break;
            case 0xa3: @autoreleasepool { frame(element, {}); } break;
            }
        }
    }
public:
    Conversion(vnalpha_result& output, const char *path, vnalpha_control *cancel = nullptr)
        : control(cancel), result(output), destination(path) {}
    ~Conversion() {
        if (writer.status == AVAssetWriterStatusWriting) [writer cancelWriting];
        if (opus) opus_decoder_destroy(opus);
        if (audio_format) CFRelease(audio_format);
    }
    int inspect(const char *source) {
        headers_only = true;
        run(source);
        return video_track != 0;
    }
    void run(const char *source) {
        if (reader.Open(source)) throw std::runtime_error("Cannot open WebM");
        long long size, available;
        if (reader.Length(&size, &available)) throw std::runtime_error("Cannot read WebM length");
        // An input can end before the other. Tell AVAssetWriter at that packet, or its interleaver waits forever.
        walk(0, size);
        if (headers_only) return;
        result.duration = duration_ticks * scale / 1e9;
        probing = false;
        walk(0, size);
        result.duration = duration_ticks * scale / 1e9;
        if (!result.frames || result.duration <= 0) throw std::runtime_error("Missing frames or WebM duration");
        [writer endSessionAtSourceTime:CMTimeMakeWithSeconds(result.duration, 1000000000)];
        dispatch_semaphore_t done = dispatch_semaphore_create(0);
        [writer finishWritingWithCompletionHandler:^{ dispatch_semaphore_signal(done); }];
        while (dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 100 * NSEC_PER_MSEC))) checkCancelled();
        check(writer.status == AVAssetWriterStatusCompleted);
    }
};
}
int vnalpha_is_alpha(const char *source) {
    try { vnalpha_result result{}; return Conversion(result, "").inspect(source); }
    catch (...) { return 0; }
}
int vnalpha_convert(const char *source, const char *destination, vnalpha_control *control, vnalpha_result *result, char *error, int error_size) {
    @autoreleasepool {
        *result = {};
        auto start = std::chrono::steady_clock::now();
        int status = 0;
        try { Conversion conversion(*result, destination, control); conversion.run(source); }
        catch (const std::exception& failure) {
            snprintf(error, error_size, "%s", failure.what());
            [[NSFileManager defaultManager] removeItemAtPath:@(destination) error:nil];
            status = -1;
        }
        result->seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        struct rusage usage{}; getrusage(RUSAGE_SELF, &usage); result->peak_bytes = usage.ru_maxrss;
        return status;
    }
}
#ifdef VNALPHA_TOOL
int main(int argc, char **argv) {
    if (argc != 3) { fprintf(stderr, "usage: vnalpha source.webm destination.mov\n"); return 2; }
    vnalpha_result result{}; char error[512]{};
    int status = vnalpha_convert(argv[1], argv[2], nullptr, &result, error, sizeof(error));
    if (status) { fprintf(stderr, "%s\n", error); return 1; }
    printf("frames=%d width=%d height=%d duration=%.6f seconds=%.6f peak_bytes=%llu\n", result.frames,
           result.width, result.height, result.duration, result.seconds, static_cast<unsigned long long>(result.peak_bytes));
    return 0;
}
#endif
