import Foundation
import Testing
import WebKit

/// The parts of PIXI 5's video texture that the loop fix wraps.
private let page = """
    <html><body><script>
    var PIXI = (function () {
        function BaseTexture(resource) { this.resource = resource; this.valid = true; this.dirtyId = 0; }
        BaseTexture.prototype.update = function () { this.dirtyId++; };
        function VideoResource(source) { this.source = source; this.uploaded = []; }
        VideoResource.prototype.upload = function (renderer, baseTexture, glTexture, source) {
            this.uploaded.push(source || this.source);
            return true;
        };
        VideoResource.prototype.dispose = function () { this.source = null; };
        return { BaseTexture: BaseTexture, resources: { VideoResource: VideoResource } };
    })();
    </script></body></html>
    """

/// What each test starts from. The video element plays nothing: a test presents each frame, which drawing the video
/// paints in that frame's color, and sets the clock itself. The video is a looping GIF made WebM: three frames of 0.14 s and a last one of 0.28 s, so the file states
/// 0.7 s. WebKit takes such a clip for shorter, and the frames alone suggest 0.56 s.
private let setUp = """
    let now = 1000;
    performance.now = () => now;
    // The file's length is read in the background. Tests wait for that read rather than for a set time, and can hold
    // it back.
    const reads = [];
    let held = null;
    const arrayBuffer = Response.prototype.arrayBuffer;
    Response.prototype.arrayBuffer = function () {
        const read = held ? held.then(() => arrayBuffer.call(this)) : arrayBuffer.call(this);
        reads.push(read);
        return read;
    };
    const lengthRead = async () => {
        for (let tries = 0; !reads.length; tries++) {
            if (tries > 500) throw new Error('the file was never read');
            await new Promise((done) => setTimeout(done, 10));
        }
        await reads[0];
        await new Promise((done) => setTimeout(done, 0));
    };
    const colors = ['#ff0000', '#00ff00', '#0000ff', '#ffff00'];
    const video = document.createElement('video');
    video.loop = true;
    for (const [name, value] of Object.entries({
        videoWidth: 4, videoHeight: 4, paused: false, duration: 0.44, currentSrc: 'data:video/webm;base64,' + clip,
    })) {
        Object.defineProperty(video, name, { value, writable: true });
    }
    let callbacks = [], presented = 0, color = colors[0];
    video.requestVideoFrameCallback = (callback) => callbacks.push(callback);
    const drawImage = CanvasRenderingContext2D.prototype.drawImage;
    CanvasRenderingContext2D.prototype.drawImage = function (source) {
        if (source !== video) return drawImage.apply(this, arguments);
        this.fillStyle = color;
        this.fillRect(0, 0, 4, 4);
    };
    const present = (index, time = index * 0.14) => {
        color = colors[index];
        presented++;
        const due = callbacks;
        callbacks = [];
        for (const callback of due) callback(now, { mediaTime: time, presentedFrames: presented });
    };
    const resource = new PIXI.resources.VideoResource(video);
    const texture = new PIXI.BaseTexture(resource);
    // Like PIXI's renderer, this uploads only when the texture has changed. It returns the shown frame's color.
    let uploadedAt = -1;
    const shown = () => {
        texture.update();
        if (texture.dirtyId !== uploadedAt) {
            uploadedAt = texture.dirtyId;
            resource.upload(null, texture, {});
        }
        const pixel = resource.uploaded[resource.uploaded.length - 1].getContext('2d').getImageData(0, 0, 1, 1).data;
        return colors.indexOf('#' + Array.from(pixel.slice(0, 3), (c) => c.toString(16).padStart(2, '0')).join(''));
    };
    """

@MainActor private func runWithVideo(_ body: String) async throws -> [String: Any] {
    let here = URL(filePath: #filePath).deletingLastPathComponent()
    let source = here.appending(path: "../../Sources/EngineRPGMaker/Resources/rpgmaker.js").standardized
    let clip = try Data(contentsOf: here.appending(path: "Fixtures/gif-loop.webm"))
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

@Test @MainActor func aShortLoopPlaysItsKeptFramesOverTheFilesLength() async throws {
    let values = try await runWithVideo("""
        const changes = [];
        texture.update();
        texture.update();
        changes.push(texture.dirtyId);
        for (const index of [0, 1, 2, 3]) {
            present(index);
            texture.update();
            texture.update();
            changes.push(texture.dirtyId);
            if (index === 0) await lengthRead();
            now += 140;
        }
        // The file lists four frames, so the pass is complete at the fourth, at 0.42 s, before WebKit would stall at
        // its end. The kept frames play from there. The first frame started at 1000 ms.
        const start = 1000;
        const frames = [];
        const at = (ms) => {
            now = start + ms;
            frames.push(shown());
        };
        for (const ms of [430, 600, 699, 710, 850, 990, 1130]) at(ms);
        // The engine stops for two seconds, then goes on one short step later.
        at(3130);
        video.paused = true;
        at(3430);
        video.paused = false;
        at(3530);
        video.playbackRate = 2;
        at(3580);
        const uploads = resource.uploaded.length;
        // The game seeks.
        video.currentTime = 0.3;
        at(3580);
        // A new source starts keeping over, and the old frames are let go.
        const kept = resource.uploaded[resource.uploaded.length - 1];
        video.dispatchEvent(new Event('emptied'));
        texture.update();
        resource.upload(null, texture, {});
        const native = resource.uploaded[resource.uploaded.length - 1] === video;
        return { changes, frames, uploads, native, released: kept.width };
        """)
    // One change for the first update, then one for each presented frame however often the engine updates.
    #expect(values["changes"] as? [Int] == [1, 2, 3, 4, 5])
    // The last frame shows from 0.42 s to the stated 0.7 s, where the next pass starts. After the stop, the clip goes
    // on 0.25 s later. Paused, it holds. At twice the speed, 50 ms moves it 0.1 s. A seek to 0.3 s shows the third
    // frame.
    #expect(values["frames"] as? [Int] == [3, 3, 3, 0, 1, 2, 3, 3, 3, 0, 1, 2])
    // A kept frame is uploaded only when the shown frame changes.
    #expect(values["uploads"] as? Int == 7)
    #expect(values["native"] as? Bool == true)
    #expect(values["released"] as? Int == 0)
}

/// Frames close together at the start, which do not match the file's frame times, so their pass ends where the video
/// wraps. Then a seek while the length is still being read, and a paused session, during which PIXI's own ticker goes
/// on updating the texture.
@Test @MainActor func keptFramesFollowSeeksAndSessionPauses() async throws {
    let values = try await runWithVideo("""
        let release;
        held = new Promise((done) => { release = done; });
        texture.update();
        // Frames at 60 fps, two of them within the first 20 ms.
        for (const index of [0, 1, 2, 3]) {
            present(index, index / 60);
            now += 17;
        }
        present(0, 0);
        // The pass is complete, but the length is not read yet. The game seeks to the third frame.
        video.currentTime = 0.04;
        release();
        await lengthRead();
        const frames = [shown()];
        __vndegen.onPause(true);
        for (let step = 0; step < 10; step++) {
            now += 100;
            frames.push(shown());
        }
        __vndegen.onPause(false);
        now += 15;
        frames.push(shown());
        // The game restarts the clip, then the session pauses for two seconds with no updates at all.
        video.currentTime = 0;
        frames.push(shown());
        __vndegen.onPause(true);
        now += 2000;
        __vndegen.onPause(false);
        now += 10;
        frames.push(shown());
        now += 10;
        frames.push(shown());
        return { frames };
        """)
    // The seek holds through the read, the paused session holds the frame, and 15 ms later the fourth frame is due. A
    // seek to 0 shows the first frame, and after the pause only the 10 ms since it count, then 20 ms.
    #expect(values["frames"] as? [Int] == [2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 0, 0, 1])
}

/// The file's frame times arrive only after the last frame has been kept. The kept frames take over then, without
/// waiting for the video to wrap.
@Test @MainActor func keptFramesTakeOverWhenTheFrameTimesArriveLate() async throws {
    let values = try await runWithVideo("""
        let release;
        held = new Promise((done) => { release = done; });
        texture.update();
        for (const index of [0, 1, 2, 3]) {
            present(index);
            now += 140;
        }
        release();
        await lengthRead();
        const frames = [shown()];
        const kept = resource.uploaded[resource.uploaded.length - 1] !== video;
        now += 300;
        frames.push(shown());
        return { frames, kept };
        """)
    // The fourth frame shows from the moment the times arrive, then 0.3 s later the next pass has started.
    #expect(values["frames"] as? [Int] == [3, 0])
    #expect(values["kept"] as? Bool == true)
}
