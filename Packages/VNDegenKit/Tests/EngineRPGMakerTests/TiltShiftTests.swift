import Foundation
import Testing
import WebKit

/// The tilt-shift from pixi-filters 3, which reads the window size once when made, as No Socks RPG bundles it.
private let page = """
    <html><body><script>
    var PIXI = (function () {
        function Point(x, y) { this.x = x; this.y = y; }
        Point.prototype.set = function (x, y) { this.x = x; this.y = y; };
        function Filter() { this.uniforms = {}; }
        Filter.prototype.apply = function () {
            const u = this.uniforms;
            this.applied = [u.texSize.x, u.texSize.y, u.start.y, u.end.y];
        };
        return { Point: Point, Filter: Filter, filters: {} };
    })();
    </script><script>
    (function (t) {
        function TiltShiftAxisFilter(start, end) {
            t.Filter.call(this);
            this.uniforms.start = start || new t.Point(0, window.innerHeight / 2);
            this.uniforms.end = end || new t.Point(600, window.innerHeight / 2);
            this.uniforms.texSize = new t.Point(window.innerWidth, window.innerHeight);
        }
        TiltShiftAxisFilter.prototype = Object.create(t.Filter.prototype);
        Object.assign(PIXI.filters, { TiltShiftAxisFilter: TiltShiftAxisFilter });
    })(PIXI);
    window.Graphics = { width: 916, height: 524 };
    </script></body></html>
    """

@Test @MainActor func tiltShiftBlursAtTheGameResolutionNotTheViewSize() async throws {
    let source = URL(filePath: #filePath).deletingLastPathComponent()
        .appending(path: "../../Sources/EngineRPGMaker/Resources/rpgmaker.js").standardized
    let configuration = WKWebViewConfiguration()
    for script in ["window.__vndegen = {};", try String(contentsOf: source, encoding: .utf8)] {
        configuration.userContentController.addUserScript(WKUserScript(source: script, injectionTime: .atDocumentStart, forMainFrameOnly: true))
    }
    // A portrait iPad, the size the filter saw when the game started there.
    let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 1032, height: 1376), configuration: configuration)
    webView.loadHTMLString(page, baseURL: nil)
    let deadline = ContinuousClock.now + .seconds(20)
    while try await webView.evaluateJavaScript("document.readyState === 'complete' && !!window.Graphics") as? Bool != true {
        guard ContinuousClock.now < deadline else { throw CancellationError() }
        try await Task.sleep(for: .milliseconds(50))
    }
    // One filter keeps the default band, across the middle of the window. The others have bands the game placed, one
    // across the middle of the game, which must stay put when the game's resolution later changes.
    let applied = try await webView.callAsyncJavaScript("""
        const fallback = new PIXI.filters.TiltShiftAxisFilter();
        const placed = new PIXI.filters.TiltShiftAxisFilter(new PIXI.Point(0, 205.5), new PIXI.Point(600, 205.5));
        const middle = new PIXI.filters.TiltShiftAxisFilter(new PIXI.Point(0, 262), new PIXI.Point(600, 262));
        fallback.apply();
        placed.apply();
        middle.apply();
        Graphics.height = 600;
        middle.apply();
        return [fallback.applied, placed.applied, middle.applied];
        """, arguments: [:], contentWorld: .page) as? [[Double]]
    #expect(applied == [[916, 524, 262, 262], [916, 524, 205.5, 205.5], [916, 600, 262, 262]])
}
