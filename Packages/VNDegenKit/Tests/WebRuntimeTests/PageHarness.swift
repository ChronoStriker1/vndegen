import Foundation
import OSLog
import WebKit
@testable import WebRuntime

/// A web view running the NW.js shim on a blank page, with the app's side of the bridge answered by `answer`.
@MainActor final class PageHarness: NSObject, WKUIDelegate {
    let webView: WKWebView
    var answer: (BridgeRequest) -> BridgeReply

    init(storage: Bool = true, scripts: [String] = [], scheme: (any WKURLSchemeHandler)? = nil,
         answer: @escaping (BridgeRequest) -> BridgeReply) throws {
        self.answer = answer
        let root = FileManager.default.temporaryDirectory
        let game = WebGameConfiguration(folder: GameFolder(root: root, webRoot: ""), appName: "test",
                                        storageDirectory: storage ? root : nil, buttonScript: { _, _ in nil },
                                        scriptArgument: "test-script", log: Logger())
        let configuration = WKWebViewConfiguration()
        if let scheme { configuration.setURLSchemeHandler(scheme, forURLScheme: GameFolder.scheme) }
        for source in [try WebGameSession.shimSource(for: game)] + scripts {
            configuration.userContentController.addUserScript(WKUserScript(source: source, injectionTime: .atDocumentStart, forMainFrameOnly: true))
        }
        webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
        super.init()
        webView.uiDelegate = self
    }

    /// Loads `html` and waits for it to finish.
    func load(_ html: String, baseURL: URL = URL(string: "https://localhost/")!) async throws {
        webView.loadHTMLString(html, baseURL: baseURL)
        try await waitUntilLoaded()
    }

    func load(_ request: URLRequest) async throws {
        webView.load(request)
        try await waitUntilLoaded()
    }

    private func waitUntilLoaded() async throws {
        let start = ContinuousClock.now
        repeat { try await Task.sleep(for: .milliseconds(20)) } while webView.isLoading && ContinuousClock.now - start < .seconds(20)
    }

    /// Runs `body` as an async function in the page and returns what it returns.
    @discardableResult
    func run(_ body: String) async throws -> Any? {
        try await webView.callAsyncJavaScript(body, arguments: [:], contentWorld: .page)
    }

    func webView(_ webView: WKWebView, runJavaScriptTextInputPanelWithPrompt prompt: String, defaultText: String?,
                 initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping @MainActor (String?) -> Void) {
        guard prompt.hasPrefix(WebGameSession.bridgePrefix),
              let request = try? JSONDecoder().decode(BridgeRequest.self, from: Data(prompt.dropFirst(WebGameSession.bridgePrefix.count).utf8))
        else { return completionHandler(nil) }
        completionHandler(answer(request).json)
    }
}
