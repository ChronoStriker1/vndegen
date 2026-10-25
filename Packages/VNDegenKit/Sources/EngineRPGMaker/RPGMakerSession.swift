import Foundation
import OSLog
import VNCore
import WebKit
#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

nonisolated let rpgLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "rpgmaker")

/// Runs an RPG Maker MV or MZ game in a WKWebView, looking enough like NW.js that the game saves to its own `save/`
/// folder in the format the desktop build uses.
final class RPGMakerSession: NSObject, EngineSession {
    static let bridgePrefix = "\u{1}vndegen:"
    static let messageName = "vndegen"
    /// Hides the iPhone from MZ's `Utils.isMobileDevice`, which would otherwise give up 10% of the height to browser chrome
    /// that a web view does not have. iPad and macOS already report this shape.
    static let desktopUserAgent = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.0 Safari/605.1.15"

    let view: PlatformView
    private let webView: GameWebView
    private let folder: GameFolder
    private let bridge: NodeBridge
    private let onExit: @MainActor () -> Void
    private var ended = false
    private var paused = false
    #if DEBUG
    private var script: Task<Void, Never>?
    #endif

    init(launch: GameLaunch) throws {
        folder = GameFolder(root: launch.folder)
        bridge = NodeBridge(folder: folder)
        onExit = launch.onExit
        let shim = try Self.shimSource(for: folder)
        let configuration = WKWebViewConfiguration()
        configuration.setURLSchemeHandler(GameSchemeHandler(folder: folder), forURLScheme: GameFolder.scheme)
        configuration.mediaTypesRequiringUserActionForPlayback = []
        #if os(iOS)
        configuration.allowsInlineMediaPlayback = true
        configuration.allowsPictureInPictureMediaPlayback = false
        configuration.defaultWebpagePreferences.preferredContentMode = .mobile
        #endif
        configuration.userContentController.addUserScript(
            WKUserScript(source: shim, injectionTime: .atDocumentStart, forMainFrameOnly: true))
        webView = GameWebView(frame: .zero, configuration: configuration)
        view = webView
        super.init()

        configuration.userContentController.add(WeakMessageHandler(self), name: Self.messageName)
        webView.uiDelegate = self
        webView.navigationDelegate = self
        webView.underPageBackgroundColor = .black
        #if DEBUG
        webView.isInspectable = true
        #endif
        #if os(iOS)
        webView.customUserAgent = Self.desktopUserAgent
        webView.scrollView.isScrollEnabled = false
        webView.scrollView.bounces = false
        webView.scrollView.contentInsetAdjustmentBehavior = .never
        webView.backgroundColor = .black
        #else
        webView.allowsMagnification = false
        webView.allowsBackForwardNavigationGestures = false
        #endif
        rpgLog.notice("loading \(launch.game.title, privacy: .public) from \(launch.folder.path, privacy: .public)")
        webView.load(URLRequest(url: folder.indexURL))
        #if DEBUG
        startScriptIfRequested()
        #endif
    }

    private static func shimSource(for folder: GameFolder) throws -> String {
        guard let url = Bundle.module.url(forResource: "nwjs-shim", withExtension: "js") else { throw RPGMakerError.shimMissing }
        let config = ["mainModule": folder.mainModule, "cwd": GameFolder.virtualRoot, "platform": "darwin"]
        let json = String(decoding: try JSONEncoder().encode(config), as: UTF8.self)
        return try String(contentsOf: url, encoding: .utf8).replacingOccurrences(of: "__VNDEGEN_CONFIG__", with: json)
    }

    // MARK: EngineSession

    func send(_ event: ControllerEvent) {
        switch event {
        case let .button(button, pressed):
            guard let stroke = KeyStroke.stroke(for: button) else { return }
            run("__vndegen.key('\(pressed ? "keydown" : "keyup")', '\(stroke.key)', '\(stroke.code)', \(stroke.keyCode))")
        case .pointer(let pointer):
            let phase = switch pointer.phase {
            case .began: "began"
            case .moved: "moved"
            case .ended: "ended"
            case .cancelled: "cancelled"
            }
            run("__vndegen.pointer('\(phase)', \(pointer.location.x), \(pointer.location.y), \(pointer.button == .primary ? 0 : 2))")
        case .scroll(let dy):
            run("__vndegen.wheel(\(dy))")
        }
    }

    func setPaused(_ paused: Bool) {
        guard self.paused != paused else { return }
        self.paused = paused
        run("__vndegen.setPaused(\(paused))")
        webView.setAllMediaPlaybackSuspended(paused, completionHandler: nil)
    }

    func terminate() {
        ended = true
        #if DEBUG
        script?.cancel()
        #endif
        webView.stopLoading()
        webView.configuration.userContentController.removeAllUserScripts()
        webView.configuration.userContentController.removeScriptMessageHandler(forName: Self.messageName)
        webView.uiDelegate = nil
        webView.navigationDelegate = nil
        // Leaving the page fires the engine's unload handler, which stops audio before the view goes away.
        webView.loadHTMLString("", baseURL: nil)
        webView.removeFromSuperview()
    }

    // MARK: Helpers

    private func run(_ javaScript: String) {
        guard !ended else { return }
        webView.evaluateJavaScript(javaScript) { _, error in
            if let error {
                rpgLog.error("js call failed: \(javaScript, privacy: .public): \(error.localizedDescription, privacy: .public)")
            }
        }
    }

    /// The game asked to quit. Runs `onExit` after the current callback returns, because `onExit` tears this session down.
    private func quitFromGame() {
        guard !ended else { return }
        ended = true
        let onExit = onExit
        Task { onExit() }
    }

    private func openExternal(_ url: URL) {
        guard ["http", "https", "mailto"].contains(url.scheme?.lowercased() ?? "") else { return }
        #if canImport(UIKit)
        UIApplication.shared.open(url)
        #else
        NSWorkspace.shared.open(url)
        #endif
    }

    private func answerBridge(_ json: String) -> String {
        let request: BridgeRequest
        do {
            request = try JSONDecoder().decode(BridgeRequest.self, from: Data(json.utf8))
        } catch {
            return BridgeReply.failure(BridgeError(code: "EINVAL", message: "bad bridge request: \(error)")).json
        }
        switch request {
        case .quit:
            quitFromGame()
            return BridgeReply.value(.null).json
        case .openExternal(let raw):
            if let url = URL(string: raw) { openExternal(url) }
            return BridgeReply.value(.null).json
        default:
            return bridge.handle(request).json
        }
    }
}

// MARK: - WKUIDelegate

extension RPGMakerSession: WKUIDelegate {
    func webView(_ webView: WKWebView, runJavaScriptTextInputPanelWithPrompt prompt: String, defaultText: String?,
                 initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping @MainActor (String?) -> Void) {
        guard prompt.hasPrefix(Self.bridgePrefix) else {
            rpgLog.notice("prompt ignored: \(prompt, privacy: .public)")
            completionHandler(nil)
            return
        }
        completionHandler(answerBridge(String(prompt.dropFirst(Self.bridgePrefix.count))))
    }

    func webView(_ webView: WKWebView, runJavaScriptAlertPanelWithMessage message: String, initiatedByFrame frame: WKFrameInfo,
                 completionHandler: @escaping @MainActor () -> Void) {
        rpgLog.notice("alert: \(message, privacy: .public)")
        completionHandler()
    }

    func webView(_ webView: WKWebView, runJavaScriptConfirmPanelWithMessage message: String, initiatedByFrame frame: WKFrameInfo,
                 completionHandler: @escaping @MainActor (Bool) -> Void) {
        rpgLog.notice("confirm answered no: \(message, privacy: .public)")
        completionHandler(false)
    }

    func webView(_ webView: WKWebView, createWebViewWith configuration: WKWebViewConfiguration,
                 for navigationAction: WKNavigationAction, windowFeatures: WKWindowFeatures) -> WKWebView? {
        if let url = navigationAction.request.url { openExternal(url) }
        return nil
    }
}

// MARK: - WKNavigationDelegate

extension RPGMakerSession: WKNavigationDelegate {
    func webView(_ webView: WKWebView, decidePolicyFor navigationAction: WKNavigationAction,
                 decisionHandler: @escaping @MainActor (WKNavigationActionPolicy) -> Void) {
        guard let url = navigationAction.request.url else { return decisionHandler(.cancel) }
        if url.scheme == GameFolder.scheme || url.scheme == "about" {
            decisionHandler(.allow)
        } else {
            openExternal(url)
            decisionHandler(.cancel)
        }
    }

    func webView(_ webView: WKWebView, didFailProvisionalNavigation navigation: WKNavigation!, withError error: any Error) {
        rpgLog.error("navigation failed: \(error.localizedDescription, privacy: .public)")
    }

    func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: any Error) {
        rpgLog.error("navigation failed: \(error.localizedDescription, privacy: .public)")
    }

    func webViewWebContentProcessDidTerminate(_ webView: WKWebView) {
        rpgLog.error("web content process died, reloading the game")
        webView.reload()
    }
}

// MARK: - WKScriptMessageHandler

extension RPGMakerSession: WKScriptMessageHandler {
    func userContentController(_ userContentController: WKUserContentController, didReceive message: WKScriptMessage) {
        guard let body = message.body as? [String: Any], let text = body["message"] as? String else { return }
        switch body["level"] as? String {
        case "error": rpgLog.error("js: \(text, privacy: .public)")
        case "warn": rpgLog.warning("js: \(text, privacy: .public)")
        default: rpgLog.notice("js: \(text, privacy: .public)")
        }
    }
}

/// WKUserContentController retains its handlers, so the session goes behind a weak link to avoid a cycle with its web view.
private final class WeakMessageHandler: NSObject, WKScriptMessageHandler {
    private weak var target: RPGMakerSession?

    init(_ target: RPGMakerSession) {
        self.target = target
    }

    func userContentController(_ userContentController: WKUserContentController, didReceive message: WKScriptMessage) {
        target?.userContentController(userContentController, didReceive: message)
    }
}

final class GameWebView: WKWebView {
    #if os(macOS)
    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        window?.makeFirstResponder(self)
    }
    #endif
}

// MARK: - Debug script

#if DEBUG
extension RPGMakerSession {
    private func startScriptIfRequested() {
        let steps: [InputStep]
        do {
            guard let parsed = try InputScript.fromLaunchArguments() else { return }
            steps = parsed
        } catch {
            rpgLog.error("script: \(String(describing: error), privacy: .public)")
            return
        }
        rpgLog.notice("script: \(steps.count) steps")
        script = Task { [weak self] in
            for step in steps {
                guard let self, !Task.isCancelled else { return }
                await self.perform(step)
            }
            rpgLog.notice("script: done")
        }
    }

    private func perform(_ step: InputStep) async {
        switch step {
        case .wait(let seconds):
            await sleep(seconds)
        case .press(let button):
            send(.button(button, pressed: true))
            await sleep(0.1)
            send(.button(button, pressed: false))
        case let .hold(button, seconds):
            send(.button(button, pressed: true))
            await sleep(seconds)
            send(.button(button, pressed: false))
        case .tap(let point):
            send(.pointer(PointerEvent(phase: .began, location: point)))
            await sleep(0.08)
            send(.pointer(PointerEvent(phase: .ended, location: point)))
        case .scroll(let dy):
            send(.scroll(dy: dy))
        case .snapshot(let path):
            await snapshot(to: path)
        case .js(let code):
            let result = await evaluate(code)
            rpgLog.notice("script js: \(code, privacy: .public) => \(result, privacy: .public)")
        case .log(let text):
            rpgLog.notice("script: \(text, privacy: .public)")
        case .quit:
            quitFromGame()
        }
    }

    private func sleep(_ seconds: Double) async {
        try? await Task.sleep(for: .seconds(seconds))
    }

    private func evaluate(_ javaScript: String) async -> String {
        await withCheckedContinuation { continuation in
            webView.evaluateJavaScript(javaScript) { value, error in
                if let error {
                    continuation.resume(returning: "error: \(error.localizedDescription)")
                } else {
                    continuation.resume(returning: value.map { String(describing: $0) } ?? "undefined")
                }
            }
        }
    }

    private func snapshot(to path: String) async {
        let png: Data? = await withCheckedContinuation { continuation in
            webView.takeSnapshot(with: nil) { image, error in
                if let error { rpgLog.error("script: snapshot failed: \(error.localizedDescription, privacy: .public)") }
                continuation.resume(returning: image.flatMap(Self.pngData))
            }
        }
        guard let png else { return }
        do {
            let url = URL(fileURLWithPath: path)
            try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
            try png.write(to: url)
            rpgLog.notice("script: wrote \(path, privacy: .public)")
        } catch {
            rpgLog.error("script: could not write \(path, privacy: .public): \(error.localizedDescription, privacy: .public)")
        }
    }

    private static func pngData(_ image: PlatformImage) -> Data? {
        #if canImport(UIKit)
        image.pngData()
        #else
        image.tiffRepresentation.flatMap(NSBitmapImageRep.init(data:))?.representation(using: .png, properties: [:])
        #endif
    }
}
#endif
