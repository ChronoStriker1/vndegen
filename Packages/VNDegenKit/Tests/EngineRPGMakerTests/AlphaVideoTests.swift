import Foundation
import Testing
import WebKit

/// The parts of PIXI 5's video texture that VNDegen wraps. As in PIXI, the base Resource's update reaches the texture,
/// and VideoResource's own update is throttled, here so much that it never does. Forced updates are counted.
private let page = """
    <html><body><script>
    var PIXI = (function () {
        function BaseTexture(resource) { this.resource = resource; this.valid = true; this.dirtyId = 0; resource.texture = this; }
        BaseTexture.prototype.update = function () { this.dirtyId++; };
        function Resource() {}
        Resource.prototype.update = function () { this.forced++; this.texture.update(); };
        function VideoResource(source) { this.source = source; this.uploaded = []; this.forced = 0; }
        VideoResource.prototype = Object.create(Resource.prototype);
        VideoResource.prototype.upload = function (renderer, baseTexture, glTexture, source) {
            this.uploaded.push(source || this.source);
            return true;
        };
        VideoResource.prototype.update = function () {};
        VideoResource.prototype.dispose = function () { this.source = null; };
        return { BaseTexture: BaseTexture, resources: { Resource: Resource, VideoResource: VideoResource } };
    })();
    </script></body></html>
    """

/// A 64 by 64 VP9 clip with an alpha channel, four frames of 0.14 s: a square of red, green, blue, then yellow on a
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
        videoWidth: 64, videoHeight: 64, paused: false, duration: 0.56, currentTime: 0,
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
    // Uploads when the texture changed, as PIXI's renderer does, and returns the RGBA of a corner and the middle. A
    // stopped video's texture gets no update from PIXI, so `drawn` uploads only what an update already marked.
    const drawn = () => {
        if (texture.dirtyId !== uploadedAt) {
            uploadedAt = texture.dirtyId;
            resource.upload(null, texture, {});
        }
        const source = resource.uploaded[resource.uploaded.length - 1];
        if (!(source instanceof HTMLCanvasElement)) return 'video';
        const context = source.getContext('2d');
        return [Array.from(context.getImageData(0, 0, 1, 1).data), Array.from(context.getImageData(32, 32, 1, 1).data)];
    };
    const shown = () => {
        texture.update();
        return drawn();
    };
    const square = (pixel) => pixel.slice(0, 3).map((channel) => channel > 192);
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
        const forced = resource.forced;
        openGate();
        await until(() => resource.forced > forced);
        return { pixel: drawn()[1], forced: resource.forced - forced };
        """)
    let pixel = try #require(values["pixel"] as? [Int])
    #expect(square(pixel) == yellow)
    #expect(pixel[3] == 255)
    #expect(values["forced"] as? Int == 1)
}

@Test @MainActor func aClipThatEndsAfterItsFramesDecodedShowsItsLastFrame() async throws {
    let values = try await runWithAlphaClip("""
        video.loop = false;
        shown();
        present(0);
        await until(() => shown() !== 'video');
        openGate();
        await until(() => shown()[1][3] === 255);
        // Updates at 0.2 s and 0.35 s reach the third frame, then the video stalls through its end. Each update moves
        // the kept frames at most 0.25 s, so it takes two.
        now += 200;
        shown();
        now += 150;
        const before = square(shown()[1]);
        video.currentTime = 0.56;
        video.paused = true;
        const forced = resource.forced;
        video.dispatchEvent(new Event('ended'));
        return { before, after: drawn()[1], forced: resource.forced - forced };
        """)
    #expect(values["before"] as? [Bool] == blue)
    let after = try #require(values["after"] as? [Int])
    #expect(square(after) == yellow)
    #expect(values["forced"] as? Int == 1)
}
