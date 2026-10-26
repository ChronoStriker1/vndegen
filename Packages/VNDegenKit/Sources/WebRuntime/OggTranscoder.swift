import AudioToolbox
import CVorbis
import Foundation
import OSLog

/// Serves Ogg Vorbis audio as AAC. WebKit on iOS has no Vorbis decoder: `<audio>` fails with "not supported" and
/// `decodeAudioData` rejects the bytes, even though `canPlayType("audio/ogg")` says "probably". iOS's AudioToolbox cannot
/// read Ogg either, so stb_vorbis decodes it and AudioToolbox's AAC encoder writes an .m4a, once per file, in `directory`.
/// The game's file is read through the handle `GameFolder` opened and checked.
actor OggTranscodeCache {
    let directory: URL
    private let log: Logger
    private var running: [String: Task<URL, any Error>] = [:]

    init(directory: URL, log: Logger) {
        self.directory = directory
        self.log = log
    }

    static func handles(_ path: String) -> Bool {
        ["ogg", "oga"].contains((path as NSString).pathExtension.lowercased())
    }

    func serve(_ folder: GameFolder, path: String, method: String, range: String?) async -> FileResponse {
        guard let (source, base, size, _) = folder.open(servedPath: path) else { return .empty(404) }
        do {
            let url = try await transcoded(source, base: base, size: size)
            let handle = try FileHandle(forReadingFrom: url)
            let length = Int64(try handle.seekToEnd())
            return GameFolder.response(handle, base: 0, size: length, contentType: "audio/mp4", method: method, range: range)
        } catch {
            log.error("transcoding \(path, privacy: .public) failed: \(error.localizedDescription, privacy: .public)")
            // The original bytes are no worse than nothing, and a format stb_vorbis cannot read, such as Opus, may still play.
            return GameFolder.response(source, base: base, size: size, contentType: "audio/ogg", method: method, range: range)
        }
    }

    /// The cached .m4a for this version of the file, transcoding it first if needed. Requests for a file that is still
    /// being transcoded wait for that transcode.
    private func transcoded(_ source: FileHandle, base: UInt64, size: Int64) async throws -> URL {
        var info = stat()
        guard fstat(source.fileDescriptor, &info) == 0 else { throw CocoaError(.fileReadUnknown) }
        let name = "\(info.st_dev)-\(info.st_ino)-\(base)-\(size)-\(info.st_mtimespec.tv_sec)-\(info.st_mtimespec.tv_nsec).m4a"
        let url = directory.appendingPathComponent(name)
        if FileManager.default.fileExists(atPath: url.path) { return url }
        if let task = running[name] { return try await task.value }
        let (directory, log) = (directory, log)
        let task = Task.detached(priority: .userInitiated) {
            try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
            let staging = directory.appendingPathComponent(".\(UUID().uuidString).m4a")
            do {
                let start = ContinuousClock.now
                try AudioTranscoder.transcodeToAAC(source, base: base, size: size, to: staging)
                try FileManager.default.moveItem(at: staging, to: url)
                log.notice("transcoded \(size) bytes of Ogg to AAC in \(String(describing: ContinuousClock.now - start), privacy: .public)")
            } catch {
                try? FileManager.default.removeItem(at: staging)
                throw error
            }
            return url
        }
        running[name] = task
        defer { running[name] = nil }
        return try await task.value
    }
}

nonisolated enum AudioTranscoder {
    /// Decodes the Ogg Vorbis file stored in `handle` at `base`, `size` bytes long, and writes it as AAC in an MPEG-4 file.
    static func transcodeToAAC(_ handle: FileHandle, base: UInt64, size: Int64, to output: URL) throws {
        // A FILE of its own on a duplicate descriptor, so closing it leaves `handle` open.
        guard size > 0, size <= Int64(UInt32.max), let file = fdopen(dup(handle.fileDescriptor), "rb") else { throw unreadable }
        guard fseeko(file, off_t(base), SEEK_SET) == 0 else {
            fclose(file)
            throw unreadable
        }
        var error: Int32 = 0
        guard let vorbis = stb_vorbis_open_file_section(file, 1, &error, nil, UInt32(size)) else {
            fclose(file)
            throw unreadable
        }
        defer { stb_vorbis_close(vorbis) }
        let info = stb_vorbis_get_info(vorbis)
        let channels = UInt32(info.channels)
        guard channels > 0, info.sample_rate > 0 else { throw unreadable }

        var pcm = AudioStreamBasicDescription(
            mSampleRate: Double(info.sample_rate), mFormatID: kAudioFormatLinearPCM,
            mFormatFlags: kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked, mBytesPerPacket: 4 * channels,
            mFramesPerPacket: 1, mBytesPerFrame: 4 * channels, mChannelsPerFrame: channels, mBitsPerChannel: 32, mReserved: 0)
        var aac = AudioStreamBasicDescription()
        aac.mFormatID = kAudioFormatMPEG4AAC
        aac.mSampleRate = pcm.mSampleRate
        aac.mChannelsPerFrame = channels
        var writerRef: ExtAudioFileRef?
        try check(ExtAudioFileCreateWithURL(output as CFURL, kAudioFileM4AType, &aac, nil, AudioFileFlags.eraseFile.rawValue, &writerRef))
        let writer = writerRef!
        defer { ExtAudioFileDispose(writer) }
        try check(ExtAudioFileSetProperty(writer, kExtAudioFileProperty_ClientDataFormat,
                                          UInt32(MemoryLayout<AudioStreamBasicDescription>.size), &pcm))

        let frames = 8192
        let samples = UnsafeMutablePointer<Float>.allocate(capacity: frames * Int(channels))
        defer { samples.deallocate() }
        while true {
            let count = stb_vorbis_get_samples_float_interleaved(vorbis, Int32(channels), samples, Int32(frames * Int(channels)))
            if count <= 0 { break }
            var list = AudioBufferList(mNumberBuffers: 1, mBuffers: AudioBuffer(
                mNumberChannels: channels, mDataByteSize: UInt32(Int(count) * Int(pcm.mBytesPerFrame)), mData: samples))
            try check(ExtAudioFileWrite(writer, UInt32(count), &list))
        }
    }

    private static let unreadable = CocoaError(.fileReadCorruptFile)

    private static func check(_ status: OSStatus) throws {
        guard status == noErr else { throw NSError(domain: NSOSStatusErrorDomain, code: Int(status)) }
    }
}
