import Foundation
import OSLog
import Testing
import WebKit
@testable import WebRuntime

private let fixtures = URL(fileURLWithPath: #filePath).deletingLastPathComponent().appendingPathComponent("Fixtures")

/// A game folder with tone.ogg (one second of 440 Hz in stereo), mono.ogg (one second in mono at 22.05 kHz), a copy of
/// tone.ogg under another name, and a file that starts like Ogg but is not, served over vndegen:// to a page running the
/// shim and the Ogg decoder. `serve` can wrap the game's scheme handler.
@MainActor private func oggPage(_ limits: WebGameSession.OggLimits = .init(),
                                serve: (GameSchemeHandler) -> any WKURLSchemeHandler = { $0 }) async throws -> (PageHarness, URL) {
    let fm = FileManager.default
    let root = fm.temporaryDirectory.appendingPathComponent("vndegen-ogg-\(UUID().uuidString)", isDirectory: true)
    let bgm = root.appendingPathComponent("data/bgm", isDirectory: true)
    try fm.createDirectory(at: bgm, withIntermediateDirectories: true)
    try Data("<html><body></body></html>".utf8).write(to: root.appendingPathComponent("index.html"))
    try fm.copyItem(at: fixtures.appendingPathComponent("tone.ogg"), to: bgm.appendingPathComponent("tone.ogg"))
    try fm.copyItem(at: fixtures.appendingPathComponent("tone.ogg"), to: bgm.appendingPathComponent("again.OGG"))
    try fm.copyItem(at: fixtures.appendingPathComponent("mono.ogg"), to: bgm.appendingPathComponent("mono.ogg"))
    try Data("OggS and then nothing a decoder can use".utf8).write(to: bgm.appendingPathComponent("broken.ogg"))
    let folder = GameFolder(root: root, webRoot: "")
    let page = try PageHarness(storage: false, scripts: [try WebGameSession.oggScript(limits: limits)],
                               scheme: serve(GameSchemeHandler(folder: folder, log: Logger()))) { _ in .value(.null) }
    try await page.load(URLRequest(url: folder.indexURL))
    return (page, root)
}

/// Serves the game folder, but never answers a request for a file whose name starts with "stall".
@MainActor private final class StallingSchemeHandler: NSObject, WKURLSchemeHandler {
    let game: GameSchemeHandler
    var stopped = 0

    init(_ game: GameSchemeHandler) {
        self.game = game
    }

    private func stalls(_ task: any WKURLSchemeTask) -> Bool {
        task.request.url?.lastPathComponent.hasPrefix("stall") == true
    }

    func webView(_ webView: WKWebView, start task: any WKURLSchemeTask) {
        if !stalls(task) { game.webView(webView, start: task) }
    }

    func webView(_ webView: WKWebView, stop task: any WKURLSchemeTask) {
        if stalls(task) { stopped += 1 } else { game.webView(webView, stop: task) }
    }
}

/// Page helpers. `until` waits for a condition. `pagesOf` lists a file's Ogg pages as [offset, length]. `withSerial`
/// gives every page of a file another stream serial. `decodes` returns [sample rate, frames] or "rejected". It decodes a
/// copy, because decodeAudioData detaches the buffer it is given.
private let helpers = """
    const until = async (ready) => { while (!ready()) await new Promise((r) => setTimeout(r, 10)); };
    const pagesOf = (b) => {
        const pages = [];
        for (let at = 0; at + 27 <= b.length;) {
            let end = at + 27 + b[at + 26];
            for (let i = 0; i < b[at + 26]; i++) end += b[at + 27 + i];
            pages.push([at, end - at]);
            at = end;
        }
        return pages;
    };
    const withSerial = (b, serial) => {
        const copy = b.slice();
        for (const [at] of pagesOf(copy)) new DataView(copy.buffer).setUint32(at + 14, serial, true);
        return copy;
    };
    const concat = (...parts) => {
        const out = new Uint8Array(parts.reduce((n, p) => n + p.length, 0));
        let offset = 0;
        for (const p of parts) { out.set(p, offset); offset += p.length; }
        return out;
    };
    const decodes = async (context, bytes) => {
        try {
            const buffer = await context.decodeAudioData(bytes.slice().buffer);
            return [buffer.sampleRate, buffer.length];
        } catch (e) {
            return 'rejected';
        }
    };
    const file = async (path) => new Uint8Array(await (await fetch(path)).arrayBuffer());

    """

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

@MainActor @Test func chainedStreamsAllDecode() async throws {
    let (page, root) = try await oggPage()
    defer { try? FileManager.default.removeItem(at: root) }
    let result = try await page.run(helpers + """
        const tone = await file('data/bgm/tone.ogg'), mono = await file('data/bgm/mono.ogg');
        const context = new OfflineAudioContext(2, 1, 22050);
        return [
            await decodes(context, concat(tone, withSerial(tone, 7))),
            await decodes(context, concat(tone, tone)),
            await decodes(context, concat(tone, new TextEncoder().encode('ID3 tag and other trailing bytes'))),
            await decodes(context, concat(tone, withSerial(mono, 7))),
        ];
        """) as? [Any]
    // Two one-second links, with distinct serials as RFC 3533 asks or both the same as `cat` makes them.
    #expect(result?[0] as? [Int] == [44100, 88200])
    #expect(result?[1] as? [Int] == [44100, 88200])
    #expect(result?[2] as? [Int] == [44100, 44100])
    // A link in another format is refused here, so WebKit's decoder gets the file, and on the Mac resamples it.
    #expect((result?[3] as? [Int])?.first != 44100)
}

@MainActor @Test func aChainCountsAgainstOneSampleLimit() async throws {
    let (page, root) = try await oggPage(.init(maxSamples: 100_000))
    defer { try? FileManager.default.removeItem(at: root) }
    let result = try await page.run(helpers + """
        const tone = await file('data/bgm/tone.ogg');
        const context = new OfflineAudioContext(2, 1, 22050);
        return [await decodes(context, tone), await decodes(context, concat(tone, withSerial(tone, 7)))];
        """) as? [Any]
    #expect(result?[0] as? [Int] == [44100, 44100])
    #expect((result?[1] as? [Int])?.first != 44100)
}

@MainActor @Test func aPacketPastTheLimitIsRefusedWhileItIsStillArriving() async throws {
    let (page, root) = try await oggPage()
    defer { try? FileManager.default.removeItem(at: root) }
    // tone.ogg without its end-of-stream mark, then 17 full pages that continue one packet past 1 MiB and never end it.
    let result = try await page.run(helpers + """
        const tone = await file('data/bgm/tone.ogg');
        const [last] = pagesOf(tone).slice(-1);
        tone[last[0] + 5] &= ~4;
        const filler = new Uint8Array(27 + 255 + 255 * 255);
        filler.set(tone.subarray(last[0], last[0] + 27));
        filler[5] = 1;
        filler.fill(0xff, 6, 14);
        filler[26] = 255;
        filler.fill(255, 27, 27 + 255);
        return await decodes(new OfflineAudioContext(2, 1, 22050), concat(tone, ...Array(17).fill(filler)));
        """)
    #expect((result as? [Int])?.first != 44100)
}

@MainActor @Test func queuedDecodesHoldNoCopyOfTheirInput() async throws {
    let (page, root) = try await oggPage(.init(workers: 2))
    defer { try? FileManager.default.removeItem(at: root) }
    // Memory is not visible from the page, so this counts the input bytes copied while ten of the twelve requests wait.
    let result = try await page.run("""
        const bytes = await (await fetch('data/bgm/tone.ogg')).arrayBuffer();
        const context = new OfflineAudioContext(2, 1, 22050);
        const slice = Uint8Array.prototype.slice;
        let copied = 0;
        Uint8Array.prototype.slice = function () {
            const out = slice.apply(this, arguments);
            copied += out.length;
            return out;
        };
        const requests = Array.from({ length: 12 }, () => context.decodeAudioData(bytes.slice(0)));
        const whileQueued = copied;
        Uint8Array.prototype.slice = slice;
        const buffers = await Promise.all(requests);
        return [whileQueued / bytes.byteLength, buffers.every((b) => b.length === 44100)];
        """) as? [Any]
    #expect((result?[0] as? Int ?? 99) <= 2)
    #expect(result?[1] as? Bool == true)
}

@MainActor @Test func aQueuedDecodeKeepsTheBytesItWasGiven() async throws {
    let (page, root) = try await oggPage(.init(workers: 2))
    defer { try? FileManager.default.removeItem(at: root) }
    // The page zeroes each buffer right after handing it over, while most of the requests still wait in the queue.
    let result = try await page.run("""
        const bytes = await (await fetch('data/bgm/tone.ogg')).arrayBuffer();
        const context = new OfflineAudioContext(2, 1, 22050);
        const inputs = Array.from({ length: 6 }, () => bytes.slice(0));
        const requests = inputs.map((input) => context.decodeAudioData(input));
        for (const input of inputs) { try { new Uint8Array(input).fill(0); } catch (e) {} }
        const buffers = await Promise.all(requests.map((r) => r.catch(() => null)));
        return [buffers.every((b) => b && b.length === 44100), inputs.every((input) => input.byteLength === 0)];
        """) as? [Bool]
    #expect(result == [true, true])
}

@MainActor @Test func reassigningASourceAfterRemovingItSetsTheDecodedTrackOnce() async throws {
    let (page, root) = try await oggPage()
    defer { try? FileManager.default.removeItem(at: root) }
    // `keeper` keeps the decode of again.OGG alive, so `restarted` gets the same cache entry back. A handler left from
    // its first assignment would set the decoded track a second time, which aborts the new play().
    let result = try await page.run(helpers + """
        const keeper = new Audio('data/bgm/again.OGG');
        const restarted = new Audio('data/bgm/again.OGG');
        restarted.play().catch(() => {});
        restarted.removeAttribute('src');
        restarted.src = 'data/bgm/again.OGG';
        let sets = 0;
        new MutationObserver((records) => { sets += records.length; }).observe(restarted, { attributes: true, attributeFilter: ['src'] });
        step('decoding again.OGG');
        await until(() => keeper.getAttribute('src'));
        await new Promise((r) => setTimeout(r, 100));
        return [sets, (restarted.getAttribute('src') || '').slice(0, 5)];
        """) as? [Any]
    #expect(result?[0] as? Int == 1 && result?[1] as? String == "blob:")
}

@MainActor @Test func cancelledFetchesFreeTheirWorkers() async throws {
    var stalling: StallingSchemeHandler?
    let (page, root) = try await oggPage(.init(workers: 2)) { game in
        let handler = StallingSchemeHandler(game)
        stalling = handler
        return handler
    }
    defer { try? FileManager.default.removeItem(at: root) }
    let result = try await page.run("""
        const stalled = [new Audio('data/bgm/stall1.ogg'), new Audio('data/bgm/stall2.ogg')];
        await new Promise((r) => setTimeout(r, 200));
        stalled[0].src = 'data/bgm/none.wav';
        stalled[1].removeAttribute('src');
        step('decoding after the cancelled fetches');
        const buffer = await new OfflineAudioContext(2, 1, 22050).decodeAudioData(await (await fetch('data/bgm/tone.ogg')).arrayBuffer());
        return [buffer.sampleRate, buffer.length];
        """) as? [Int]
    #expect(result == [44100, 44100])
    for _ in 0..<100 where stalling?.stopped != 2 { try await Task.sleep(for: .milliseconds(20)) }
    #expect(stalling?.stopped == 2)
}

@MainActor @Test func requestingACancelledTrackAgainDecodesItAfresh() async throws {
    let (page, root) = try await oggPage(.init(workers: 2))
    defer { try? FileManager.default.removeItem(at: root) }
    // Two Web Audio decodes hold both workers, so the element's decode waits in the queue when it is cancelled.
    let result = try await page.run(helpers + """
        const bytes = await (await fetch('data/bgm/tone.ogg')).arrayBuffer();
        const context = new OfflineAudioContext(2, 1, 22050);
        const busy = [context.decodeAudioData(bytes.slice(0)), context.decodeAudioData(bytes.slice(0))];
        const audio = new Audio('data/bgm/tone.ogg');
        audio.src = 'data/bgm/none.wav';
        audio.src = 'data/bgm/tone.ogg';
        await Promise.all(busy);
        await until(() => audio.getAttribute('src'));
        return audio.getAttribute('src');
        """) as? String
    #expect(result?.hasPrefix("blob:") == true)
}

@MainActor @Test func srcAttributeChangesGoThroughTheDecoder() async throws {
    let (page, root) = try await oggPage()
    defer { try? FileManager.default.removeItem(at: root) }
    let result = try await page.run(helpers + """
        const set = document.createElement('audio');
        set.setAttribute('src', 'data/bgm/tone.ogg');
        step('decoding a src set as an attribute');
        await until(() => set.getAttribute('src') && set.getAttribute('src') !== 'data/bgm/tone.ogg');

        // Each element shares again.OGG's decode with `keeper`, which sees it to the end.
        const keeper = new Audio('data/bgm/again.OGG');
        const removed = new Audio('data/bgm/again.OGG');
        const removedPlay = removed.play().then(() => 'played', (e) => e.name);
        removed.removeAttribute('src');
        removed.load();
        const reloaded = new Audio('data/bgm/again.OGG');
        const reloadedPlay = reloaded.play().then(() => 'played', (e) => e.name);
        reloaded.load();
        step('decoding again.OGG');
        await until(() => keeper.getAttribute('src'));
        await new Promise((r) => setTimeout(r, 50));
        const settled = (promise) => Promise.race([promise, new Promise((r) => setTimeout(() => r('pending'), 100))]);
        return [set.getAttribute('src').slice(0, 5), set.src.endsWith('/data/bgm/tone.ogg'),
                await settled(removedPlay), removed.getAttribute('src'), removed.src === '',
                await settled(reloadedPlay), (reloaded.getAttribute('src') || '').slice(0, 5)];
        """) as? [Any]
    #expect(result?[0] as? String == "blob:" && result?[1] as? Bool == true)
    #expect(result?[2] as? String == "AbortError" && result?[3] is NSNull && result?[4] as? Bool == true)
    #expect(result?[5] as? String == "AbortError" && result?[6] as? String == "blob:")
}
