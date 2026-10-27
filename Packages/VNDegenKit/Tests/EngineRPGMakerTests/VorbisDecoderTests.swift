import Foundation
import OSLog
import Testing
import WebKit
@testable import WebRuntime

/// MZ's js/libs/vorbisdecoder.js 1.0.1 with its decoding left out. The page side numbers decoders as MZ does. The worker
/// keeps each decoder's read position under its number the way VorbisWorkerDecoder does: the first data opens it, later
/// data replaces the buffer and reads on from where it was, and "loaded" or "destroy" closes it. It acknowledges every
/// message it gets through.
private let decoder = """
    class VorbisDecoder {
        constructor() {
            VorbisDecoder.id = (VorbisDecoder.id || 0) % 1e3;
            this.id = VorbisDecoder.id++;
            VorbisDecoder.instances = VorbisDecoder.instances || [];
            VorbisDecoder.instances[this.id] = this;
        }
        destroy() {
            VorbisDecoder.worker.postMessage({ id: this.id, destroy: true });
            delete VorbisDecoder.instances[this.id];
        }
        send(arrayBuffer, isLoaded) { VorbisDecoder.worker.postMessage({ id: this.id, arrayBuffer: arrayBuffer, isLoaded: isLoaded }); }
    }
    if (typeof WorkerGlobalScope === 'function') {
        const instances = [];
        self.addEventListener('message', (e) => {
            const id = e.data.id;
            const state = instances[id] || (instances[id] = { arrayBuffer: null, byteOffset: 0 });
            if (e.data.arrayBuffer) {
                if (!state.arrayBuffer) state.byteOffset = 0;
                state.arrayBuffer = e.data.arrayBuffer;
                const remainingBytes = state.arrayBuffer.byteLength - state.byteOffset;
                new Uint8Array(state.arrayBuffer, state.byteOffset, Math.min(remainingBytes, 8192));
                state.byteOffset = state.arrayBuffer.byteLength;
            }
            if (e.data.isLoaded || e.data.destroy) state.arrayBuffer = null;
            if (e.data.destroy) delete instances[id];
            postMessage({ id: id });
        });
    } else {
        VorbisDecoder.worker = new Worker(document.currentScript.src);
    }
    """

/// A track keeps its decoder open while a thousand sounds come and go, as footsteps and letter sounds do in a long stay
/// on one map. The thousandth sound got the track's number, and the worker read its 4 KB from 300 KB in, past its end.
@Test @MainActor func decodersNeverShareAWorkerStateWithAnOpenTrack() async throws {
    let fm = FileManager.default
    let root = fm.temporaryDirectory.appending(path: "vndegen-vorbis-\(UUID().uuidString)", directoryHint: .isDirectory)
    defer { try? fm.removeItem(at: root) }
    let libs = root.appending(path: "js/libs", directoryHint: .isDirectory)
    try fm.createDirectory(at: libs, withIntermediateDirectories: true)
    try Data(#"<html><body><script src="js/libs/vorbisdecoder.js"></script></body></html>"#.utf8).write(to: root.appending(path: "index.html"))
    try Data(decoder.utf8).write(to: libs.appending(path: "vorbisdecoder.js"))

    let source = URL(filePath: #filePath).deletingLastPathComponent()
        .appending(path: "../../Sources/EngineRPGMaker/Resources/rpgmaker.js").standardized
    let folder = GameFolder(root: root, webRoot: "")
    let configuration = WKWebViewConfiguration()
    let scheme = GameSchemeHandler(folder: folder, log: Logger())
    configuration.setURLSchemeHandler(scheme, forURLScheme: GameFolder.scheme)
    for script in ["window.__vndegen = {};", try String(contentsOf: source, encoding: .utf8)] {
        configuration.userContentController.addUserScript(WKUserScript(source: script, injectionTime: .atDocumentStart, forMainFrameOnly: true))
    }
    let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
    webView.load(URLRequest(url: folder.indexURL))
    let deadline = ContinuousClock.now + .seconds(20)
    while try await webView.evaluateJavaScript("document.readyState === 'complete' && typeof VorbisDecoder === 'function' && !!VorbisDecoder.worker") as? Bool != true {
        guard ContinuousClock.now < deadline else { throw CancellationError() }
        try await Task.sleep(for: .milliseconds(50))
    }

    let result = try await webView.callAsyncJavaScript("""
        const errors = [];
        window.addEventListener('error', (e) => errors.push(e.message));
        let acknowledged = 0;
        VorbisDecoder.worker.addEventListener('message', () => acknowledged++);
        const track = new VorbisDecoder();
        track.send(new ArrayBuffer(300000), false);
        let sent = 1;
        for (let i = 0; i < 1000; i++) {
            const sound = new VorbisDecoder();
            sound.send(new ArrayBuffer(4096), true);
            sound.destroy();
            sent += 2;
        }
        const timeout = Date.now() + 10000;
        while (acknowledged + errors.length < sent && Date.now() < timeout) await new Promise((r) => setTimeout(r, 10));
        return { errors: errors, acknowledged: acknowledged, sent: sent, track: track.id, next: VorbisDecoder.id };
        """, arguments: [:], contentWorld: .page) as? [String: Any]
    #expect(result?["errors"] as? [String] == [])
    #expect(result?["acknowledged"] as? Int == 2001)
    #expect(result?["track"] as? Int == 0)
    #expect(result?["next"] as? Int == 1001)
}
