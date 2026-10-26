import Foundation
import Testing
import WebKit

/// A blank page with the real shim injected, logging the key and mouse events that reach it.
@MainActor
private final class ShimPage {
    let webView: WKWebView

    init() async throws {
        let source = URL(filePath: #filePath).deletingLastPathComponent()
            .appending(path: "../../Sources/WebRuntime/Resources/nwjs-shim.js").standardized
        let config = #"{"runtime":"browser","cwd":"/","platform":"darwin","appName":"test","mainModule":"/index.html","execPath":"/index.html"}"#
        let shim = try String(contentsOf: source, encoding: .utf8).replacingOccurrences(of: "__VNDEGEN_CONFIG__", with: config)
        let configuration = WKWebViewConfiguration()
        configuration.userContentController.addUserScript(WKUserScript(source: shim, injectionTime: .atDocumentStart, forMainFrameOnly: true))
        webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 300), configuration: configuration)
        webView.loadHTMLString("""
            <html><body style="margin:0;width:400px;height:300px"><script>
            window.log = [];
            for (const type of ['keydown', 'keyup']) document.addEventListener(type, (e) => log.push(type + ' ' + e.code + ' ' + e.keyCode));
            for (const type of ['mousedown', 'mouseup']) document.addEventListener(type, (e) => log.push(type + ' ' + e.clientX + ',' + e.clientY));
            </script></body></html>
            """, baseURL: nil)
        let deadline = ContinuousClock.now + .seconds(20)
        while try await run("document.readyState === 'complete' && !!window.__vndegen") as? Bool != true {
            guard ContinuousClock.now < deadline else { throw CancellationError() }
            try await Task.sleep(for: .milliseconds(50))
        }
    }

    @discardableResult
    func run(_ javaScript: String) async throws -> Any? {
        try await webView.callAsyncJavaScript("return eval(code)", arguments: ["code": javaScript], contentWorld: .page)
    }

    /// Everything logged since the last call.
    func takeLog() async throws -> [String] {
        try await run("log.splice(0)") as? [String] ?? []
    }
}

@Test @MainActor func keySentByTwoButtonsGoesUpWithTheLastRelease() async throws {
    let page = try await ShimPage()
    let down = "__vndegen.key('keydown', 'Enter', 'Enter', 13)", up = "__vndegen.key('keyup', 'Enter', 'Enter', 13)"
    try await page.run([down, down, up].joined(separator: ";"))
    #expect(try await page.takeLog() == ["keydown Enter 13"])
    try await page.run(up)
    #expect(try await page.takeLog() == ["keyup Enter 13"])
    try await page.run(up)
    #expect(try await page.takeLog() == [])
}

@Test @MainActor func mappedClickLandsWhereThePointerIs() async throws {
    let page = try await ShimPage()
    try await page.run("__vndegen.click('began', 0); __vndegen.click('ended', 0)")
    #expect(try await page.takeLog() == ["mousedown 200,150", "mouseup 200,150"])
    // A synthetic stand-in for the real mouse moving over the page.
    try await page.run("document.body.dispatchEvent(new PointerEvent('pointermove', { clientX: 120, clientY: 80, bubbles: true }))")
    try await page.run("__vndegen.click('began', 0); __vndegen.click('ended', 0)")
    #expect(try await page.takeLog() == ["mousedown 120,80", "mouseup 120,80"])
}

@Test @MainActor func touchMovesTheMouseToItFirst() async throws {
    let page = try await ShimPage()
    try await page.run("""
        document.addEventListener('mousemove', (e) => log.push('mousemove ' + e.clientX + ',' + e.clientY));
        document.addEventListener('touchstart', () => log.push('touchstart'));
        """)
    // The Mac's WebKit has no TouchEvent, so a plain event carries the touch.
    let touch = { (type: String, x: Int) in
        """
        const touch = new Event('\(type)', { bubbles: true });
        Object.defineProperty(touch, 'changedTouches', { value: [{ clientX: \(x), clientY: 80, screenX: \(x), screenY: 80 }] });
        document.body.dispatchEvent(touch);
        """
    }
    try await page.run(touch("touchstart", 120))
    #expect(try await page.takeLog() == ["mousemove 120,80", "touchstart"])
    // A drag leaves the mouse where the touch began.
    try await page.run(touch("touchmove", 140))
    #expect(try await page.takeLog() == [])
}
