import Foundation
import OSLog
import Testing
import WebKit
@testable import WebRuntime

private let fixtures = URL(fileURLWithPath: #filePath).deletingLastPathComponent().appendingPathComponent("Fixtures")

/// A game folder with tone.ogg (one second of 440 Hz in stereo), a copy under another name, and a file that starts like
/// Ogg but is not, served over vndegen:// to a page running the shim and the Ogg decoder.
@MainActor private func oggPage(_ limits: WebGameSession.OggLimits = .init()) async throws -> (PageHarness, URL) {
    let fm = FileManager.default
    let root = fm.temporaryDirectory.appendingPathComponent("vndegen-ogg-\(UUID().uuidString)", isDirectory: true)
    let bgm = root.appendingPathComponent("data/bgm", isDirectory: true)
    try fm.createDirectory(at: bgm, withIntermediateDirectories: true)
    try Data("<html><body></body></html>".utf8).write(to: root.appendingPathComponent("index.html"))
    try fm.copyItem(at: fixtures.appendingPathComponent("tone.ogg"), to: bgm.appendingPathComponent("tone.ogg"))
    try fm.copyItem(at: fixtures.appendingPathComponent("tone.ogg"), to: bgm.appendingPathComponent("again.OGG"))
    try Data("OggS and then nothing a decoder can use".utf8).write(to: bgm.appendingPathComponent("broken.ogg"))
    let page = try PageHarness(storage: false, scripts: [try WebGameSession.oggScript(limits: limits)],
                               scheme: GameSchemeHandler(folder: GameFolder(root: root, webRoot: ""), log: Logger())) { _ in .value(.null) }
    try await page.load(URLRequest(url: GameFolder(root: root, webRoot: "").indexURL))
    return (page, root)
}

// Media elements load nothing in a web view under `swift test`, not even a plain WAV file, so their path is checked in
// the simulator. Web Audio works here. An OfflineAudioContext at 22.05 kHz tells the decoders apart: WebKit's own
// resamples to the context's rate, and this one keeps the file's 44.1 kHz.

@MainActor @Test func webAudioDecodesOggVorbisInThePage() async throws {
    let (page, root) = try await oggPage()
    defer { try? FileManager.default.removeItem(at: root) }
    let result = try await page.run("""
        const context = new OfflineAudioContext(2, 1, 22050);
        const buffer = await context.decodeAudioData(await (await fetch('data/bgm/tone.ogg')).arrayBuffer());
        let peak = 0;
        for (const s of buffer.getChannelData(0)) peak = Math.max(peak, Math.abs(s));
        const callback = await new Promise((done) => context.decodeAudioData(
            new Uint8Array([0x4f, 0x67, 0x67, 0x53, 1, 2, 3]).buffer, () => done('decoded'), () => done('failed')));
        let broken;
        try {
            await context.decodeAudioData(await (await fetch('data/bgm/broken.ogg')).arrayBuffer());
            broken = 'decoded';
        } catch (e) {
            broken = 'rejected';
        }
        return [buffer.numberOfChannels, buffer.sampleRate, buffer.length, Math.round(peak * 1000), callback, broken];
        """) as? [Any]
    #expect(result?[0] as? Int == 2 && result?[1] as? Int == 44100 && result?[2] as? Int == 44100)
    // The fixture's sine peaks at 0.092 after Vorbis, as ffmpeg's decoder also reads it.
    #expect((88...96).contains(result?[3] as? Int ?? 0))
    #expect(result?[4] as? String == "failed" && result?[5] as? String == "rejected")
}

@MainActor @Test func audioOverTheSampleLimitIsNotDecodedHere() async throws {
    let (page, root) = try await oggPage(.init(maxSamples: 1000))
    defer { try? FileManager.default.removeItem(at: root) }
    // Refused here, the file goes to WebKit's decoder, which on the Mac reads Vorbis and resamples it.
    let result = try await page.run("""
        const buffer = await new OfflineAudioContext(2, 1, 22050).decodeAudioData(await (await fetch('data/bgm/tone.ogg')).arrayBuffer());
        return buffer.sampleRate;
        """) as? Int
    #expect(result == 22050)
}

@MainActor @Test func manyDecodesAtOnceAllFinish() async throws {
    let (page, root) = try await oggPage(.init(workers: 2))
    defer { try? FileManager.default.removeItem(at: root) }
    let lengths = try await page.run("""
        const bytes = await (await fetch('data/bgm/tone.ogg')).arrayBuffer();
        const context = new OfflineAudioContext(2, 1, 22050);
        const buffers = await Promise.all(Array.from({ length: 12 }, () => context.decodeAudioData(bytes.slice(0))));
        return buffers.map((b) => b.length);
        """) as? [Int]
    #expect(lengths == Array(repeating: 44100, count: 12))
}
