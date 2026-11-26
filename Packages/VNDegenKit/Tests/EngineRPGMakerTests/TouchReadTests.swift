import Foundation
import Testing
import WebKit

private let core = """
    var Graphics = {
        pageToCanvasX: (x) => x, pageToCanvasY: (y) => y,
        isInsideCanvas: (x, y) => x >= 0 && x < 400 && y >= 0 && y < 300,
    };
    function TouchInput() {}
    TouchInput.initialize = function () { this.clear(); this._setupEventHandlers(); };
    TouchInput._createNewState = () => ({ triggered: false, hovered: false, released: false });
    TouchInput.clear = function () {
        this._mousePressed = false; this._screenPressed = false;
        this._newState = this._createNewState(); this._currentState = this._createNewState();
    };
    TouchInput.update = function () { this._currentState = this._newState; this._newState = this._createNewState(); };
    TouchInput.isPressed = function () { return this._mousePressed || this._screenPressed; };
    TouchInput._setupEventHandlers = function () {
        document.addEventListener('mousemove', this._onMouseMove.bind(this));
        document.addEventListener('touchstart', this._onTouchStart.bind(this), { passive: false });
        document.addEventListener('touchend', this._onTouchEnd.bind(this));
    };
    TouchInput._onMouseMove = function (event) {
        if (!this._mousePressed && Graphics.isInsideCanvas(event.pageX, event.pageY)) this._newState.hovered = true;
    };
    TouchInput._onTouchStart = function (event) {
        for (const touch of event.changedTouches) {
            if (Graphics.isInsideCanvas(touch.pageX, touch.pageY)) {
                this._screenPressed = true;
                this._newState.triggered = true;
                event.preventDefault();
            }
        }
    };
    TouchInput._onTouchEnd = function (event) {
        for (const touch of event.changedTouches) { this._screenPressed = false; this._newState.released = true; }
    };
    """

private let start = """
    TouchInput.initialize();
    window.tap = (type, x, y) => {
        if (type === 'touchstart') {
            document.dispatchEvent(new MouseEvent('mousemove', { clientX: x, clientY: y, bubbles: true, cancelable: true }));
        }
        const event = new Event(type, { bubbles: true, cancelable: true });
        Object.defineProperty(event, 'changedTouches', { value: [{ pageX: x, pageY: y }] });
        document.dispatchEvent(event);
        return event.defaultPrevented;
    };
    window.read = () => {
        TouchInput.update();
        const state = TouchInput._currentState;
        return ['hovered', 'triggered', 'released'].filter((name) => state[name])
            .concat(TouchInput.isPressed() ? ['pressed'] : []).join(' ');
    };
    """

@MainActor private func gamePage() async throws -> WKWebView {
    let source = URL(filePath: #filePath).deletingLastPathComponent()
        .appending(path: "../../Sources/EngineRPGMaker/Resources/rpgmaker.js").standardized
    let configuration = WKWebViewConfiguration()
    for script in ["window.__vndegen = {};", try String(contentsOf: source, encoding: .utf8)] {
        configuration.userContentController.addUserScript(WKUserScript(source: script, injectionTime: .atDocumentStart, forMainFrameOnly: true))
    }
    let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 300), configuration: configuration)
    let tags = [core, start].map { "<script src=\"data:text/javascript;base64,\(Data($0.utf8).base64EncodedString())\"></script>" }
    webView.loadHTMLString("<html><body>\(tags.joined())</body></html>", baseURL: nil)
    let deadline = ContinuousClock.now + .seconds(20)
    while try await webView.evaluateJavaScript("document.readyState === 'complete' && !!window.read") as? Bool != true {
        guard ContinuousClock.now < deadline else { throw CancellationError() }
        try await Task.sleep(for: .milliseconds(50))
    }
    return webView
}

@MainActor private func run(_ webView: WKWebView, _ script: String) async throws -> Any? {
    try await webView.evaluateJavaScript(script)
}

@Test @MainActor func aTapIsOverItsSpotForAnUpdateBeforeItPresses() async throws {
    let page = try await gamePage()
    #expect(try await run(page, "tap('touchstart', 120, 80)") as? Bool == true)
    #expect(try await run(page, "read()") as? String == "hovered")
    #expect(try await run(page, "read()") as? String == "triggered pressed")
    #expect(try await run(page, "tap('touchend', 120, 80); read()") as? String == "pressed")
    #expect(try await run(page, "read()") as? String == "released")
    #expect(try await run(page, "read()") as? String == "")
}

@Test @MainActor func aTapOutsideTheGameIsLeftToThePage() async throws {
    let page = try await gamePage()
    #expect(try await run(page, "tap('touchstart', 500, 80)") as? Bool == false)
    #expect(try await run(page, "read()") as? String == "")
    #expect(try await run(page, "read()") as? String == "")
}

@Test @MainActor func aClearDropsTouchesStillWaiting() async throws {
    let page = try await gamePage()
    #expect(try await run(page, "tap('touchstart', 120, 80); TouchInput.clear(); read()") as? String == "")
    #expect(try await run(page, "read()") as? String == "")
}
