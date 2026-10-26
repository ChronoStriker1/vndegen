import AudioToolbox
import Foundation
import OSLog
import Testing
@testable import WebRuntime

private let fixtures = URL(fileURLWithPath: #filePath).deletingLastPathComponent().appendingPathComponent("Fixtures")

/// Frames and channels of an audio file, read back with AudioToolbox.
private func audioInfo(_ url: URL) throws -> (frames: Int64, channels: UInt32, format: AudioFormatID) {
    var file: ExtAudioFileRef?
    guard ExtAudioFileOpenURL(url as CFURL, &file) == noErr, let file else { throw CocoaError(.fileReadCorruptFile) }
    defer { ExtAudioFileDispose(file) }
    var frames: Int64 = 0, format = AudioStreamBasicDescription()
    var size = UInt32(MemoryLayout<Int64>.size)
    ExtAudioFileGetProperty(file, kExtAudioFileProperty_FileLengthFrames, &size, &frames)
    size = UInt32(MemoryLayout<AudioStreamBasicDescription>.size)
    ExtAudioFileGetProperty(file, kExtAudioFileProperty_FileDataFormat, &size, &format)
    return (frames, format.mChannelsPerFrame, format.mFormatID)
}

@Test func oggVorbisIsServedAsAAC() async throws {
    let fm = FileManager.default
    let base = fm.temporaryDirectory.appendingPathComponent("vndegen-ogg-\(UUID().uuidString)", isDirectory: true)
    defer { try? fm.removeItem(at: base) }
    let game = base.appendingPathComponent("game/data/bgm", isDirectory: true)
    try fm.createDirectory(at: game, withIntermediateDirectories: true)
    try fm.copyItem(at: fixtures.appendingPathComponent("tone.ogg"), to: game.appendingPathComponent("tone.ogg"))
    try Data("not vorbis".utf8).write(to: game.appendingPathComponent("broken.ogg"))
    let folder = GameFolder(root: base.appendingPathComponent("game"), webRoot: "")
    let cache = OggTranscodeCache(directory: base.appendingPathComponent("cache"), log: Logger())

    #expect(OggTranscodeCache.handles("/data/bgm/Tone.OGG") && !OggTranscodeCache.handles("/data/bgm/tone.m4a"))
    let response = await cache.serve(folder, path: "/data/bgm/tone.ogg", method: "GET", range: nil)
    #expect(response.status == 200 && response.headers["Content-Type"] == "audio/mp4")
    let cached = try fm.contentsOfDirectory(at: base.appendingPathComponent("cache"), includingPropertiesForKeys: nil)
    #expect(cached.count == 1)
    let info = try audioInfo(try #require(cached.first))
    #expect(info.format == kAudioFormatMPEG4AAC && info.channels == 2)
    #expect(abs(info.frames - 44100) < 4096, "frames \(info.frames)")

    let part = await cache.serve(folder, path: "/data/bgm/tone.ogg", method: "GET", range: "bytes=0-99")
    #expect(part.status == 206 && part.headers["Content-Length"] == "100")
    #expect(try fm.contentsOfDirectory(atPath: base.appendingPathComponent("cache").path).count == 1)

    let broken = await cache.serve(folder, path: "/data/bgm/broken.ogg", method: "GET", range: nil)
    #expect(broken.status == 200 && broken.headers["Content-Type"] == "audio/ogg")
    #expect(await cache.serve(folder, path: "/data/bgm/missing.ogg", method: "GET", range: nil).status == 404)
}
