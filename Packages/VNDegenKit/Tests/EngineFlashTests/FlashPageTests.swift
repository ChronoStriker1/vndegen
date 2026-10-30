import Foundation
import OSLog
import Testing
import WebKit
@testable import EngineFlash
@testable import WebRuntime

/// flash-page.js on a page with the NW.js shim, as the session runs them, and two stand-ins for Ruffle's players: custom
/// elements with a canvas in an open shadow root and a `ruffle()` that records what the page asks of it.
@MainActor private final class FlashPage: NSObject, WKUIDelegate, WKNavigationDelegate {
    let webView: WKWebView
    private(set) var requests: [BridgeRequest] = []
    private var loaded: CheckedContinuation<Void, any Error>?
    private let origin: URL

    init(origin: URL, fileStorage: Bool = true) throws {
        self.origin = origin
        let game = WebGameConfiguration(folder: GameFolder(root: FileManager.default.temporaryDirectory, webRoot: ""), appName: "test",
                                        runtime: .browser, storageDirectory: fileStorage ? FileManager.default.temporaryDirectory : nil,
                                        buttonScript: { _, _ in nil }, scriptArgument: "test-script", log: Logger())
        let page = try String(contentsOf: #require(FlashPlugin.hostDirectory).appending(path: "flash-page.js"), encoding: .utf8)
        let configuration = WKWebViewConfiguration()
        for source in [try WebGameSession.shimSource(for: game), try FlashFonts.pageScript(page)] {
            configuration.userContentController.addUserScript(WKUserScript(source: source, injectionTime: .atDocumentStart, forMainFrameOnly: true))
        }
        webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
        super.init()
        webView.uiDelegate = self
        webView.navigationDelegate = self
    }

    func load() async throws {
        let html = """
        <html><body style="margin:0">
        <script>
        class Player extends HTMLElement {
            constructor() {
                super();
                this.canvas = document.createElement('canvas');
                this.attachShadow({ mode: 'open' }).appendChild(this.canvas);
                this.calls = [];
                this.handlers = [];
                this.events = [];
                for (const type of ['pointerdown', 'pointermove', 'pointerup', 'wheel']) {
                    this.canvas.addEventListener(type, (event) => this.events.push(type + ' ' + Math.round(event.clientX)));
                }
            }
            ruffle() {
                return {
                    suspend: () => this.calls.push('suspend'), resume: () => this.calls.push('resume'),
                    addFSCommandHandler: (handler) => this.handlers.push(handler),
                };
            }
        }
        customElements.define('ruffle-player', Player);
        window.hides = 0;
        window.addEventListener('pagehide', () => window.hides++);
        </script>
        <ruffle-player id="a" tabindex="0" style="position:absolute;left:0;top:0;width:400px;height:600px"></ruffle-player>
        <ruffle-player id="b" tabindex="0" style="position:absolute;left:400px;top:0;width:400px;height:600px"></ruffle-player>
        </body></html>
        """
        try await withCheckedThrowingContinuation { loaded = $0; webView.loadHTMLString(html, baseURL: origin) }
    }

    @discardableResult
    func run(_ body: String) async throws -> Any? {
        try await webView.callAsyncJavaScript(body, contentWorld: .page)
    }

    func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) {
        loaded?.resume()
        loaded = nil
    }

    func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: any Error) {
        loaded?.resume(throwing: error)
        loaded = nil
    }

    func webView(_ webView: WKWebView, runJavaScriptTextInputPanelWithPrompt prompt: String, defaultText: String?,
                 initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping @MainActor (String?) -> Void) {
        let json = String(prompt.dropFirst(WebGameSession.bridgePrefix.count))
        let request = try? JSONDecoder().decode(BridgeRequest.self, from: Data(json.utf8))
        if let request { requests.append(request) }
        completionHandler((request == .storageLoad ? BridgeReply.value(.dictionary([:])) : BridgeReply.value(.null)).json)
    }
}

@MainActor @Test func theSessionReachesEveryPlayerOnThePage() async throws {
    let page = try FlashPage(origin: URL(string: "https://localhost/")!)
    try await page.load()

    // The pointer and the wheel reach the canvas of the player under the pointer, once each.
    try await page.run("__vndegen.pointer('began', 0.75, 0.5, 0); __vndegen.pointer('ended', 0.75, 0.5, 0); __vndegen.wheel(-30);")
    #expect(try await page.run("return document.getElementById('b').events.join(', ')") as? String
        == "pointermove 600, pointerdown 600, pointerup 600, wheel 600")
    #expect(try await page.run("return document.getElementById('a').events.length") as? Int == 0)

    // Pausing writes saves and suspends every player. Resuming gives focus back to the player that had it.
    try await page.run("document.getElementById('b').focus(); __vndegen.setPaused(true); document.activeElement.blur(); __vndegen.setPaused(false);")
    #expect(try await page.run("return window.hides") as? Int == 1)
    #expect(try await page.run("return document.getElementById('a').calls.join(', ')") as? String == "suspend, resume")
    #expect(try await page.run("return document.activeElement.id") as? String == "b")

    // Quitting writes saves first, and a movie's fscommand("quit") ends the session.
    try await page.run("__vndegen.beforeQuit();")
    #expect(try await page.run("return window.hides") as? Int == 2)
    try await page.run("document.getElementById('a').handlers[0]('quit', '');")
    #expect(page.requests.contains(.quit))
}

@MainActor @Test func ruffleStorageKeysTakeOneForm() async throws {
    let page = try FlashPage(origin: URL(string: "https://localhost/")!)
    try await page.load()
    // A launcher's own encoding of a movie's path, and lowercase escapes, reach storage as the app encodes the path.
    // The name, and keys of other hosts, stay as they are.
    try await page.run("""
        localStorage['localhost/Folder%21/My%20Game/game.swf/save'] = 'a';
        localStorage['localhost/%2a%e6%89%93/game.swf/#Folder%21/slot'] = 'b';
        localStorage['other/Folder%21/game.swf/save'] = 'c';
        """)
    let sets = page.requests.compactMap { if case let .storageSet(key, _) = $0 { key } else { nil } }
    #expect(sets == ["localhost/Folder!/My%20Game/game.swf/save", "localhost/*%E6%89%93/game.swf/#Folder%21/slot", "other/Folder%21/game.swf/save"])
    #expect(try await page.run("return localStorage['localhost/Folder!/My Game/game.swf/save'] + localStorage['localhost/Folder%21/My%20Game/game.swf/save']") as? String == "aa")
}

@MainActor @Test func webKitsOwnStorageStillWorksThroughThePageScript() async throws {
    let page = try FlashPage(origin: URL(string: "https://localhost/")!, fileStorage: false)
    try await page.load()
    // Storage's own methods run on Storage, and a value stored under another form of a key reads as the key's one form.
    #expect(try await page.run("""
        localStorage.clear();
        localStorage.setItem('localhost/Folder%21/game.swf/save', 'new');
        localStorage.setItem('x', '1');
        const read = localStorage['localhost/Folder%21/game.swf/save'] + ' ' + localStorage.getItem('localhost/Folder!/game.swf/save');
        const first = localStorage.key(0) !== null;
        localStorage.clear();
        return read + ' ' + first + ' ' + localStorage.length;
        """) as? String == "new new true 0")
}
