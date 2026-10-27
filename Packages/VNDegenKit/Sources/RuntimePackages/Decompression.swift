import CLzmaDec
import Foundation

/// A streaming decoder for one compressed stream.
protocol Decompressor: AnyObject {
    /// Decodes some of `input` into `output`. `ended` is true once the stream's end marker has been decoded; streams
    /// without one, such as Inno Setup's LZMA1, never set it and end when their input does.
    func decode(_ input: UnsafeRawBufferPointer, into output: UnsafeMutableRawBufferPointer) throws
        -> (consumed: Int, produced: Int, ended: Bool)
}

/// Copies bytes through unchanged.
final class StoredDecoder: Decompressor {
    func decode(_ input: UnsafeRawBufferPointer, into output: UnsafeMutableRawBufferPointer) throws
        -> (consumed: Int, produced: Int, ended: Bool) {
        let count = min(input.count, output.count)
        if count > 0 { output.baseAddress!.copyMemory(from: input.baseAddress!, byteCount: count) }
        return (count, count, false)
    }
}

/// Inno Setup's LZMA1 streams: a 5-byte properties header, then raw LZMA data with no size field.
final class LZMA1Decoder: Decompressor {
    // Zeroed, as LzmaDec_Construct (a macro Swift can't call) would leave it.
    private var state = CLzmaDec()
    private var header: [UInt8] = []
    private var started = false
    private let allocator = ISzAlloc(Alloc: { _, size in malloc(size) }, Free: { _, address in free(address) })

    deinit {
        withUnsafeMutablePointer(to: &state) { state in
            withUnsafePointer(to: allocator) { LzmaDec_Free(state, $0) }
        }
    }

    func decode(_ input: UnsafeRawBufferPointer, into output: UnsafeMutableRawBufferPointer) throws
        -> (consumed: Int, produced: Int, ended: Bool) {
        var consumed = 0
        if !started {
            while header.count < Int(LZMA_PROPS_SIZE), consumed < input.count {
                header.append(input[consumed])
                consumed += 1
            }
            guard header.count == Int(LZMA_PROPS_SIZE) else { return (consumed, 0, false) }
            guard header[0] < 9 * 5 * 5 else { throw RuntimeInstallError.corrupt("bad LZMA properties") }
            let result = withUnsafeMutablePointer(to: &state) { state in
                withUnsafePointer(to: allocator) { LzmaDec_Allocate(state, header, UInt32(LZMA_PROPS_SIZE), $0) }
            }
            guard result == SZ_OK else { throw RuntimeInstallError.corrupt("LZMA dictionary allocation failed") }
            withUnsafeMutablePointer(to: &state) { LzmaDec_Init($0) }
            started = true
        }
        var inSize = input.count - consumed
        var outSize = output.count
        var status = LZMA_STATUS_NOT_SPECIFIED
        let result = withUnsafeMutablePointer(to: &state) { state in
            LzmaDec_DecodeToBuf(state, output.baseAddress?.assumingMemoryBound(to: UInt8.self), &outSize,
                                input.baseAddress.map { ($0 + consumed).assumingMemoryBound(to: UInt8.self) }, &inSize,
                                LZMA_FINISH_ANY, &status)
        }
        guard result == SZ_OK else { throw RuntimeInstallError.corrupt("LZMA data error \(result)") }
        return (consumed + inSize, outSize, status == LZMA_STATUS_FINISHED_WITH_MARK)
    }
}

/// zlib streams with their zlib header, as Inno Setup writes them.
final class ZlibDecoder: Decompressor {
    private var stream = z_stream()

    init() throws {
        guard inflateInit_(&stream, ZLIB_VERSION, Int32(MemoryLayout<z_stream>.size)) == Z_OK else {
            throw RuntimeInstallError.corrupt("zlib initialization failed")
        }
    }

    deinit { inflateEnd(&stream) }

    func decode(_ input: UnsafeRawBufferPointer, into output: UnsafeMutableRawBufferPointer) throws
        -> (consumed: Int, produced: Int, ended: Bool) {
        stream.next_in = UnsafeMutablePointer(mutating: input.baseAddress?.assumingMemoryBound(to: Bytef.self))
        stream.avail_in = uInt(input.count)
        stream.next_out = output.baseAddress?.assumingMemoryBound(to: Bytef.self)
        stream.avail_out = uInt(output.count)
        let result = inflate(&stream, Z_NO_FLUSH)
        guard result == Z_OK || result == Z_STREAM_END || result == Z_BUF_ERROR else {
            throw RuntimeInstallError.corrupt("zlib data error \(result)")
        }
        return (input.count - Int(stream.avail_in), output.count - Int(stream.avail_out), result == Z_STREAM_END)
    }
}

/// bzip2 streams.
final class BZip2Decoder: Decompressor {
    private var stream = bz_stream()

    init() throws {
        guard BZ2_bzDecompressInit(&stream, 0, 0) == BZ_OK else {
            throw RuntimeInstallError.corrupt("bzip2 initialization failed")
        }
    }

    deinit { BZ2_bzDecompressEnd(&stream) }

    func decode(_ input: UnsafeRawBufferPointer, into output: UnsafeMutableRawBufferPointer) throws
        -> (consumed: Int, produced: Int, ended: Bool) {
        stream.next_in = UnsafeMutablePointer(mutating: input.baseAddress?.assumingMemoryBound(to: CChar.self))
        stream.avail_in = UInt32(input.count)
        stream.next_out = output.baseAddress?.assumingMemoryBound(to: CChar.self)
        stream.avail_out = UInt32(output.count)
        let result = BZ2_bzDecompress(&stream)
        guard result == BZ_OK || result == BZ_STREAM_END else {
            throw RuntimeInstallError.corrupt("bzip2 data error \(result)")
        }
        return (input.count - Int(stream.avail_in), output.count - Int(stream.avail_out), result == BZ_STREAM_END)
    }
}
