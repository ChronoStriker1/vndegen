import Foundation
import Testing
import WebKit

/// The parts of PIXI 5's video texture that VNDegen wraps. Updates the resource asks for are counted, as PIXI stops
/// its own once a video has ended.
private let page = """
    <html><body><script>
    var PIXI = (function () {
        function BaseTexture(resource) { this.resource = resource; this.valid = true; this.dirtyId = 0; }
        BaseTexture.prototype.update = function () { this.dirtyId++; };
        function VideoResource(source) { this.source = source; this.uploaded = []; this.updates = 0; }
        VideoResource.prototype.upload = function (renderer, baseTexture, glTexture, source) {
            this.uploaded.push(source || this.source);
            return true;
        };
        VideoResource.prototype.update = function () { this.updates++; };
        VideoResource.prototype.dispose = function () { this.source = null; };
        return { BaseTexture: BaseTexture, resources: { VideoResource: VideoResource } };
    })();
    </script></body></html>
    """

/// A 16 by 16 VP9 clip with an alpha channel, four frames of 0.14 s: a square of red, green, blue, then yellow on a
/// transparent background whose color plane is white, as GIFs converted to WebM have it. The video element plays
/// nothing, so a test presents its frames and sets the clock. Decoding waits until the test opens the gate.
private let setUp = """
    let now = 1000;
    performance.now = () => now;
    let openGate;
    const gate = new Promise((done) => { openGate = done; });
    const Decoder = window.VideoDecoder;
    window.VideoDecoder = class extends Decoder {
        flush() { return gate.then(() => super.flush()); }
    };
    const video = document.createElement('video');
    for (const [name, value] of Object.entries({
        videoWidth: 16, videoHeight: 16, paused: false, duration: 0.56, currentTime: 0,
        currentSrc: 'data:video/webm;base64,' + clip,
    })) {
        Object.defineProperty(video, name, { value, writable: true });
    }
    let callbacks = [], presented = 0;
    video.requestVideoFrameCallback = (callback) => callbacks.push(callback);
    const present = (time) => {
        presented++;
        const due = callbacks;
        callbacks = [];
        for (const callback of due) callback(now, { mediaTime: time, presentedFrames: presented });
    };
    const resource = new PIXI.resources.VideoResource(video);
    const texture = new PIXI.BaseTexture(resource);
    let uploadedAt = -1;
    // Uploads when the texture changed, as PIXI's renderer does, and returns the RGBA of a corner and the middle.
    const shown = () => {
        texture.update();
        if (texture.dirtyId !== uploadedAt) {
            uploadedAt = texture.dirtyId;
            resource.upload(null, texture, {});
        }
        const source = resource.uploaded[resource.uploaded.length - 1];
        if (!(source instanceof HTMLCanvasElement)) return 'video';
        const context = source.getContext('2d');
        return [Array.from(context.getImageData(0, 0, 1, 1).data), Array.from(context.getImageData(8, 8, 1, 1).data)];
    };
    const until = async (condition) => {
        for (let tries = 0; !condition(); tries++) {
            if (tries > 500) throw new Error('timed out');
            await new Promise((done) => setTimeout(done, 10));
        }
    };
    """

@MainActor private func runWithAlphaClip(_ body: String) async throws -> [String: Any] {
    let here = URL(filePath: #filePath).deletingLastPathComponent()
    let source = here.appending(path: "../../Sources/EngineRPGMaker/Resources/rpgmaker.js").standardized
    let clip = try Data(contentsOf: here.appending(path: "Fixtures/alpha-loop.webm"))
    let configuration = WKWebViewConfiguration()
    for script in ["window.__vndegen = {};", try String(contentsOf: source, encoding: .utf8)] {
        configuration.userContentController.addUserScript(WKUserScript(source: script, injectionTime: .atDocumentStart, forMainFrameOnly: true))
    }
    let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 300), configuration: configuration)
    webView.loadHTMLString(page, baseURL: nil)
    let deadline = ContinuousClock.now + .seconds(20)
    while try await webView.evaluateJavaScript("document.readyState === 'complete' && !!window.PIXI") as? Bool != true {
        guard ContinuousClock.now < deadline else { throw CancellationError() }
        try await Task.sleep(for: .milliseconds(50))
    }
    let result = try await webView.callAsyncJavaScript(setUp + body, arguments: ["clip": clip.base64EncodedString()],
                                                       contentWorld: .page)
    return try #require(result as? [String: Any])
}

/// VP9 stores color as YUV, so each channel comes back near full or empty rather than exactly.
private func square(_ pixel: [Int]) -> [Bool] { pixel.prefix(3).map { $0 > 192 } }

private let red = [true, false, false], green = [false, true, false], blue = [false, false, true], yellow = [true, true, false]

@Test @MainActor func anAlphaClipShowsItsDecodedFramesWithTheirTransparency() async throws {
    for loop in [true, false] {
        let values = try await runWithAlphaClip("""
            video.loop = \(loop);
            const first = shown();
            present(0);
            // While the frames decode, the texture shows nothing rather than the white color plane.
            await until(() => shown() !== 'video');
            const decoding = shown();
            openGate();
            await until(() => shown()[1][3] === 255);
            const frames = [];
            for (let i = 0; i < 5; i++) {
                frames.push(shown());
                now += 140;
            }
            return { first, decoding, frames };
            """)
        // Before the file is read, the video shows as WebKit plays it.
        #expect(values["first"] as? String == "video")
        let decoding = try #require(values["decoding"] as? [[Int]])
        #expect(decoding.allSatisfy { $0[3] == 0 })
        let frames = try #require(values["frames"] as? [[[Int]]])
        // Corners stay transparent. The middle is the square's color, frame after frame.
        #expect(frames.allSatisfy { $0[0][3] == 0 })
        let squares = frames.map { square($0[1]) }
        #expect(Array(squares.prefix(4)) == [red, green, blue, yellow])
        // A loop starts over, and a clip that plays once holds its last frame.
        #expect(squares[4] == (loop ? red : yellow))
    }
}

@Test @MainActor func aClipThatEndedBeforeItsFramesDecodedShowsItsLastFrame() async throws {
    let values = try await runWithAlphaClip("""
        video.loop = false;
        shown();
        present(0);
        await until(() => shown() !== 'video');
        // The clip plays to its end and stops while its frames decode, and PIXI no longer updates its texture.
        video.currentTime = 0.56;
        video.paused = true;
        const updatesBefore = resource.updates;
        openGate();
        await until(() => resource.updates > updatesBefore);
        now += 500;
        return { pixel: shown()[1], asked: resource.updates - updatesBefore };
        """)
    let pixel = try #require(values["pixel"] as? [Int])
    #expect(square(pixel) == yellow)
    #expect(pixel[3] == 255)
    #expect(values["asked"] as? Int == 1)
}
