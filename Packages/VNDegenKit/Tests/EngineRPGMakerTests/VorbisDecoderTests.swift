import Foundation
import OSLog
import Testing
import WebKit
@testable import WebRuntime

/// MZ's js/libs/vorbisdecoder.js 1.0.1 with its decoding left out. The page side is MZ's: decoders numbered as MZ does,
/// and decoded audio handed to the sound's handler. The worker keeps each decoder's read position under its number the
/// way VorbisWorkerDecoder does: the first data opens it, later data replaces the buffer and reads on from where it was,
/// and "loaded" or "destroy" closes it. Data that is not Ogg fails as MZ's does, where stb_vorbis cannot open it and the
/// read position is garbage. A "crash" message throws. The worker acknowledges every message it gets through, and
/// answers "loaded" with one frame of decoded audio.
private let decoder = """
    class VorbisDecoder {
        constructor(context, decodeHandler, errorHandler) {
            VorbisDecoder.id = (VorbisDecoder.id || 0) % 1e3;
            this.id = VorbisDecoder.id++;
            this.context = context;
            this.decodeHandler = decodeHandler;
            this.errorHandler = errorHandler;
            VorbisDecoder.instances = VorbisDecoder.instances || [];
            VorbisDecoder.instances[this.id] = this;
        }
        destroy() {
            VorbisDecoder.worker.postMessage({ id: this.id, destroy: true });
            delete VorbisDecoder.instances[this.id];
        }
        send(arrayBuffer, isLoaded) { VorbisDecoder.worker.postMessage({ id: this.id, arrayBuffer: arrayBuffer, isLoaded: isLoaded }); }
        onMessage(message) {
            const buffer = this.context.createBuffer(1, message.data[0].length, message.sampleRate);
            buffer.getChannelData(0).set(message.data[0]);
            this.decodeHandler(buffer);
        }
    }
    if (typeof WorkerGlobalScope === 'function') {
        const instances = [];
        self.addEventListener('message', (e) => {
            if (e.data.crash) throw new RangeError('length cannot be negative');
            const id = e.data.id;
            const state = instances[id] || (instances[id] = { arrayBuffer: null, byteOffset: 0 });
            if (e.data.arrayBuffer) {
                const head = new Uint8Array(e.data.arrayBuffer, 0, 4);
                if (!state.arrayBuffer) state.byteOffset = String.fromCharCode(...head) === 'OggS' ? 0 : 1e9;
                state.arrayBuffer = e.data.arrayBuffer;
                const remainingBytes = state.arrayBuffer.byteLength - state.byteOffset;
                new Uint8Array(state.arrayBuffer, state.byteOffset, Math.min(remainingBytes, 8192));
                state.byteOffset = state.arrayBuffer.byteLength;
            }
            if (e.data.isLoaded || e.data.destroy) state.arrayBuffer = null;
            if (e.data.destroy) delete instances[id];
            postMessage(e.data.isLoaded ? { id: id, sampleRate: 8000, data: [new Float32Array(1024)] } : { id: id });
        });
    } else {
        VorbisDecoder.worker = new Worker(document.currentScript.src);
        VorbisDecoder.worker.addEventListener('message', (e) => {
            const instance = VorbisDecoder.instances[e.data.id];
            if (instance && e.data.data && instance.decodeHandler) instance.onMessage(e.data);
        });
    }
    """

/// MZ 1.9's WebAudio fetch path, with _onFetch as rmmz_core.js has it. MZ passes chunks on at most once a second; this
/// passes every chunk on as it arrives, the case where the last chunk goes before the fetch ends.
private let webAudio = """
    function WebAudio(decoder) { this._decoder = decoder; this._data = null; this._fetchedSize = 0; this._fetchedData = []; this._isLoaded = false; }
    WebAudio.prototype._onFetch = function(response) {
        if (response.ok) {
            const reader = response.body.getReader();
            const readChunk = ({ done, value }) => {
                if (done) {
                    this._isLoaded = true;
                    if (this._fetchedSize > 0) {
                        this._concatenateFetchedData();
                        this._updateBuffer();
                        this._data = null;
                    }
                    return 0;
                } else {
                    this._onFetchProcess(value);
                    return reader.read().then(readChunk);
                }
            };
            reader
                .read()
                .then(readChunk)
                .catch(() => this._onError());
        } else {
            this._onError();
        }
    };
    WebAudio.prototype._onError = function() { this._isError = true; };
    WebAudio.prototype._onFetchProcess = function(value) {
        this._fetchedSize += value.length;
        this._fetchedData.push(value);
        this._updateBufferOnFetch();
    };
    WebAudio.prototype._updateBufferOnFetch = function() {
        this._concatenateFetchedData();
        this._updateBuffer();
    };
    WebAudio.prototype._concatenateFetchedData = function() {
        const currentData = this._data;
        const currentSize = currentData ? currentData.length : 0;
        const newData = new Uint8Array(currentSize + this._fetchedSize);
        let pos = 0;
        if (currentData) {
            newData.set(currentData);
            pos += currentSize;
        }
        for (const value of this._fetchedData) {
            newData.set(value, pos);
            pos += value.length;
        }
        this._data = newData;
        this._fetchedData = [];
        this._fetchedSize = 0;
    };
    WebAudio.prototype._updateBuffer = function() { this._decoder.send(this._data.buffer.slice(0), this._isLoaded); };
    """

/// 800 frames of silence as a WAV file, 16-bit mono at 8 kHz.
private let wav = """
    const wav = new DataView(new ArrayBuffer(44 + 1600));
    const tag = (at, s) => [...s].forEach((c, i) => wav.setUint8(at + i, c.charCodeAt(0)));
    tag(0, 'RIFF'); wav.setUint32(4, 36 + 1600, true); tag(8, 'WAVE'); tag(12, 'fmt ');
    wav.setUint32(16, 16, true); wav.setUint16(20, 1, true); wav.setUint16(22, 1, true); wav.setUint32(24, 8000, true);
    wav.setUint32(28, 16000, true); wav.setUint16(32, 2, true); wav.setUint16(34, 16, true); tag(36, 'data'); wav.setUint32(40, 1600, true);
    """

/// Page helpers: `ogg(n)` is n bytes that start like an Ogg file, `errors` collects what reaches window.onerror, and
/// `acknowledged` counts the worker's replies.
private let helpers = """
    const ogg = (n) => { const b = new Uint8Array(n); b.set([0x4f, 0x67, 0x67, 0x53]); return b.buffer; };
    const errors = [];
    window.addEventListener('error', (e) => errors.push(e.message));
    let acknowledged = 0;
    VorbisDecoder.worker.addEventListener('message', () => acknowledged++);
    const settle = async (expected) => {
        const timeout = Date.now() + 10000;
        while (acknowledged + errors.length < expected && Date.now() < timeout) await new Promise((r) => setTimeout(r, 10));
        await new Promise((r) => setTimeout(r, 200));
    };
    """

/// Serves the stand-in decoder as `script` to a page running the RPG Maker additions, and waits for its worker.
@MainActor private func decoderPage(script: String = "js/libs/vorbisdecoder.js") async throws -> (WKWebView, URL) {
    let fm = FileManager.default
    let root = fm.temporaryDirectory.appending(path: "vndegen-vorbis-\(UUID().uuidString)", directoryHint: .isDirectory)
    let libs = root.appending(path: "js/libs", directoryHint: .isDirectory)
    try fm.createDirectory(at: libs, withIntermediateDirectories: true)
    try Data(#"<html><body><script src="\#(script)"></script><script src="js/rmmz_core.js"></script></body></html>"#.utf8)
        .write(to: root.appending(path: "index.html"))
    try Data(decoder.utf8).write(to: root.appending(path: String(script.prefix { $0 != "?" })))
    try Data(webAudio.utf8).write(to: root.appending(path: "js/rmmz_core.js"))

    let source = URL(filePath: #filePath).deletingLastPathComponent()
        .appending(path: "../../Sources/EngineRPGMaker/Resources/rpgmaker.js").standardized
    let folder = GameFolder(root: root, webRoot: "")
    let configuration = WKWebViewConfiguration()
    configuration.setURLSchemeHandler(GameSchemeHandler(folder: folder, log: Logger()), forURLScheme: GameFolder.scheme)
    for script in ["window.__vndegen = {};", try String(contentsOf: source, encoding: .utf8)] {
        configuration.userContentController.addUserScript(WKUserScript(source: script, injectionTime: .atDocumentStart, forMainFrameOnly: true))
    }
    let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
    webView.load(URLRequest(url: folder.indexURL))
    let deadline = ContinuousClock.now + .seconds(20)
    while try await webView.evaluateJavaScript("document.readyState === 'complete' && typeof VorbisDecoder === 'function' && !!VorbisDecoder.worker && typeof WebAudio === 'function'") as? Bool != true {
        guard ContinuousClock.now < deadline else { throw CancellationError() }
        try await Task.sleep(for: .milliseconds(50))
    }
    return (webView, root)
}

/// A track keeps its decoder open while a thousand sounds come and go, as footsteps and letter sounds do in a long stay
/// on one map. The thousandth sound got the track's number, and the worker read its 4 KB from 300 KB in, past its end.
/// The page loads the decoder as MZ ships it, with a cache-busting query, and under another name.
@Test(arguments: ["js/libs/vorbisdecoder.js", "js/libs/vorbisdecoder.js?v=1.0.1", "js/libs/ogg.js"])
@MainActor func decodersNeverShareAWorkerStateWithAnOpenTrack(script: String) async throws {
    let (webView, root) = try await decoderPage(script: script)
    defer { try? FileManager.default.removeItem(at: root) }
    let result = try await webView.callAsyncJavaScript(helpers + """
        const track = new VorbisDecoder();
        track.send(ogg(300000), false);
        let sent = 1;
        for (let i = 0; i < 1000; i++) {
            const sound = new VorbisDecoder();
            sound.send(ogg(4096), true);
            sound.destroy();
            sent += 2;
        }
        await settle(sent);
        return { errors: errors, acknowledged: acknowledged, sent: sent, track: track.id, next: VorbisDecoder.id };
        """, arguments: [:], contentWorld: .page) as? [String: Any]
    #expect(result?["errors"] as? [String] == [])
    #expect(result?["acknowledged"] as? Int == 2001)
    #expect(result?["track"] as? Int == 0)
    #expect(result?["next"] as? Int == 1001)
}

/// A WAV file under an .ogg name, as games ship and Chromium plays. MZ's decoder cannot open it and read its offsets
/// from garbage. It decodes in WebKit instead, and the worker never sees it.
@Test @MainActor func soundsThatAreNotOggDecodeInWebKit() async throws {
    let (webView, root) = try await decoderPage()
    defer { try? FileManager.default.removeItem(at: root) }
    let result = try await webView.callAsyncJavaScript(helpers + wav + """
        const context = new OfflineAudioContext(1, 8000, 8000);
        const decoded = await Promise.race([
            new Promise((resolve) => new VorbisDecoder(context, (b) => resolve(b.length + ' at ' + b.sampleRate), () => resolve('failed')).send(wav.buffer, true)),
            new Promise((resolve) => setTimeout(() => resolve('nothing'), 3000)),
        ]);
        await new Promise((r) => setTimeout(r, 200));
        return { decoded: decoded, errors: errors, acknowledged: acknowledged };
        """, arguments: [:], contentWorld: .page) as? [String: Any]
    #expect(result?["decoded"] as? String == "800 at 8000")
    #expect(result?["errors"] as? [String] == [])
    #expect(result?["acknowledged"] as? Int == 0)
}

/// MZ stops the game on any uncaught error, and an error in the decoder's worker reached the page as one. Now it costs
/// only that sound, and the worker goes on decoding.
@Test @MainActor func aDecoderWorkerErrorLeavesTheGameRunning() async throws {
    let (webView, root) = try await decoderPage()
    defer { try? FileManager.default.removeItem(at: root) }
    let result = try await webView.callAsyncJavaScript(helpers + """
        VorbisDecoder.worker.postMessage({ id: 0, crash: true });
        await new Promise((r) => setTimeout(r, 300));
        const context = new OfflineAudioContext(1, 8000, 8000);
        const decoded = await Promise.race([
            new Promise((resolve) => new VorbisDecoder(context, (b) => resolve(b.length), () => resolve('failed')).send(ogg(4096), true)),
            new Promise((resolve) => setTimeout(() => resolve('nothing'), 3000)),
        ]);
        return { decoded: decoded, errors: errors };
        """, arguments: [:], contentWorld: .page) as? [String: Any]
    #expect(result?["errors"] as? [String] == [])
    #expect(result?["decoded"] as? Int == 1024)
}

/// MZ tells the decoder a sound has loaded only when chunks are left over when the fetch ends. A WAV under an .ogg name,
/// which WebKit decodes once it has all of it, never played when the last chunk had already been passed on.
@Test @MainActor func aSoundWhoseLastChunkWentEarlyStillFinishesLoading() async throws {
    let (webView, root) = try await decoderPage()
    defer { try? FileManager.default.removeItem(at: root) }
    let result = try await webView.callAsyncJavaScript(helpers + wav + """
        const context = new OfflineAudioContext(1, 8000, 8000);
        const decoded = await Promise.race([
            new Promise((resolve) => {
                const decoder = new VorbisDecoder(context, (b) => resolve(b.length + ' at ' + b.sampleRate), () => resolve('failed'));
                new WebAudio(decoder)._onFetch(new Response(new Blob([wav.buffer])));
            }),
            new Promise((resolve) => setTimeout(() => resolve('nothing'), 3000)),
        ]);
        return { decoded: decoded, errors: errors };
        """, arguments: [:], contentWorld: .page) as? [String: Any]
    #expect(result?["decoded"] as? String == "800 at 8000")
    #expect(result?["errors"] as? [String] == [])
}
