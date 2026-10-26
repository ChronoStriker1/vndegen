import Foundation
import Testing
import VNCore
import WebKit
@testable import WebRuntime

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

@Test @MainActor func gamepadButtonsReachThePageAsAStandardXboxGamepad() async throws {
    let page = try await ShimPage()
    let state = "JSON.stringify(navigator.getGamepads().map((p) => ({ id: p.id, mapping: p.mapping, connected: p.connected, index: p.index, pressed: p.buttons.flatMap((b, i) => (b.pressed && b.value === 1 ? [i] : [])), axes: p.axes, count: p.buttons.length })))"
    try await page.run("window.connects = 0; window.addEventListener('gamepadconnected', (e) => { connects++; window.announced = e.gamepad.mapping; })")
    // Like a browser, the page sees no gamepad before its first press.
    #expect(try await page.run("navigator.getGamepads().length") as? Int == 0)

    try await page.run(WebGameSession.gamepadScript(.a, pressed: true))
    try await page.run(WebGameSession.gamepadScript(.l2, pressed: true))
    try await page.run(WebGameSession.gamepadScript(.view, pressed: true))
    try await page.run(WebGameSession.gamepadScript(.leftStickUp, pressed: true))
    try await page.run(WebGameSession.gamepadScript(.rightStickRight, pressed: true))
    let held = try #require(try await page.run(state) as? String)
    #expect(held.contains(#""mapping":"standard","connected":true,"index":0,"pressed":[0,6,8],"axes":[0,-1,1,0],"count":17"#), "\(held)")
    #expect(held.contains("STANDARD GAMEPAD"))
    #expect(try await page.run("connects") as? Int == 1)
    #expect(try await page.run("announced") as? String == "standard")
    // Each call is a new snapshot, so a game can compare it with the last one.
    #expect(try await page.run("navigator.getGamepads()[0] !== navigator.getGamepads()[0]") as? Bool == true)

    for input in [ControlInput.a, .l2, .view, .leftStickUp, .rightStickRight] {
        try await page.run(WebGameSession.gamepadScript(input, pressed: false))
    }
    #expect(try await page.run(state) as? String == #"[{"id":"Xbox Wireless Controller (STANDARD GAMEPAD Vendor: 045e Product: 0b13)","mapping":"standard","connected":true,"index":0,"pressed":[],"axes":[0,0,0,0],"count":17}]"#)
    #expect(try await page.run("connects") as? Int == 1)
}

@Test @MainActor func everyXboxButtonMovesTheVirtualGamepad() async throws {
    let page = try await ShimPage()
    let snapshot = "JSON.stringify(navigator.getGamepads().map((p) => [p.buttons.map((b) => b.pressed), p.axes]))"
    try await page.run(WebGameSession.gamepadScript(.a, pressed: false))
    let idle = try await page.run(snapshot) as? String
    for input in ControlInput.allCases {
        try await page.run(WebGameSession.gamepadScript(input, pressed: true))
        #expect(try await page.run(snapshot) as? String != idle, "\(input)")
        try await page.run(WebGameSession.gamepadScript(input, pressed: false))
        #expect(try await page.run(snapshot) as? String == idle, "\(input)")
    }
}

@Test @MainActor func aGameListeningAfterThePadConnectedStillHearsOfIt() async throws {
    let page = try await ShimPage()
    try await page.run(WebGameSession.gamepadScript(.a, pressed: true))
    try await page.run("window.heard = []; window.addEventListener('gamepadconnected', (e) => heard.push(e.gamepad.buttons[0].pressed))")
    let deadline = ContinuousClock.now + .seconds(5)
    while try await page.run("heard.length") as? Int == 0, ContinuousClock.now < deadline {
        try await Task.sleep(for: .milliseconds(20))
    }
    #expect(try await page.run("JSON.stringify(heard)") as? String == "[true]")
    // Later presses announce nothing more.
    try await page.run(WebGameSession.gamepadScript(.b, pressed: true))
    try await Task.sleep(for: .milliseconds(100))
    #expect(try await page.run("heard.length") as? Int == 1)
}
