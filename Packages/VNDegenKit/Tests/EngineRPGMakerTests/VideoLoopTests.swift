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

/// A looping GIF made WebM: three frames of 0.14 s and a last one of 0.28 s, so the file states 0.7 s. WebKit takes such a
/// clip for shorter, and the frames alone suggest 0.56 s. A canvas stands in for the video, so the test presents each
/// frame and sets the clock itself.
@Test @MainActor func aShortLoopPlaysItsKeptFramesOverTheFilesLength() async throws {
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
    let result = try await webView.callAsyncJavaScript("""
        let now = 1000;
        performance.now = () => now;
        // The file's length is read in the background. The test waits for that read rather than for a set time.
        const reads = [];
        const arrayBuffer = Response.prototype.arrayBuffer;
        Response.prototype.arrayBuffer = function () {
            const read = arrayBuffer.call(this);
            reads.push(read);
            return read;
        };
        const colors = ['#ff0000', '#00ff00', '#0000ff', '#ffff00'];
        const video = document.createElement('canvas');
        video.width = video.height = 4;
        let callbacks = [], presented = 0;
        Object.assign(video, {
            videoWidth: 4, videoHeight: 4, loop: true, paused: false, playbackRate: 1, duration: 0.44, currentTime: 0,
            currentSrc: 'data:video/webm;base64,' + clip,
            requestVideoFrameCallback(callback) { callbacks.push(callback); },
        });
        const present = (index) => {
            const context = video.getContext('2d');
            context.fillStyle = colors[index];
            context.fillRect(0, 0, 4, 4);
            presented++;
            const due = callbacks;
            callbacks = [];
            for (const callback of due) callback(now, { mediaTime: index * 0.14, presentedFrames: presented });
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
        const changes = [];
        texture.update();
        texture.update();
        changes.push(texture.dirtyId);
        for (const index of [0, 1, 2, 3]) {
            present(index);
            texture.update();
            texture.update();
            changes.push(texture.dirtyId);
            if (index === 0) {
                for (let tries = 0; !reads.length; tries++) {
                    if (tries > 500) throw new Error('the file was never read');
                    await new Promise((done) => setTimeout(done, 10));
                }
                await reads[0];
                await new Promise((done) => setTimeout(done, 0));
            }
            now += 140;
        }
        // The first frame again: the pass is complete, and the kept frames play from here.
        present(0);
        const start = now;
        const frames = [];
        const at = (ms) => {
            now = start + ms;
            frames.push(shown());
        };
        for (const ms of [0, 150, 290, 430, 600, 699, 710]) at(ms);
        // The engine stops for two seconds, then goes on one short step later.
        at(2710);
        video.paused = true;
        at(3010);
        video.paused = false;
        at(3110);
        video.playbackRate = 2;
        at(3160);
        const uploads = resource.uploaded.length;
        // The game seeks.
        video.currentTime = 0.15;
        video.dispatchEvent(new Event('seeked'));
        at(3160);
        // A new source starts keeping over, and the old frames are let go.
        const kept = resource.uploaded[resource.uploaded.length - 1];
        video.dispatchEvent(new Event('emptied'));
        texture.update();
        resource.upload(null, texture, {});
        const native = resource.uploaded[resource.uploaded.length - 1] === video;
        return { changes, frames, uploads, native, released: kept.width };
        """, arguments: ["clip": clip.base64EncodedString()], contentWorld: .page) as? [String: Any]
    let values = try #require(result)
    // One change for the first update, then one for each presented frame however often the engine updates.
    #expect(values["changes"] as? [Int] == [1, 2, 3, 4, 5])
    // The last frame shows from 0.42 s to the stated 0.7 s, where the next pass starts. After the stop, the clip goes
    // on 0.25 s later. Paused, it holds. At twice the speed, 50 ms moves it 0.1 s. A seek to 0.15 s shows the second
    // frame.
    #expect(values["frames"] as? [Int] == [0, 1, 2, 3, 3, 3, 0, 1, 1, 2, 3, 1])
    // A kept frame is uploaded only when the shown frame changes.
    #expect(values["uploads"] as? Int == 8)
    #expect(values["native"] as? Bool == true)
    #expect(values["released"] as? Int == 0)
}
