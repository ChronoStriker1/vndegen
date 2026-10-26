import Foundation
import OSLog
import VNCore
import WebKit
#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

/// What the page sees around it: the desktop runtime its PC build shipped in.
nonisolated public enum PageRuntime: Sendable, Equatable {
    /// NW.js. `execPath` is what `process.execPath` reports, a virtual path.
    case nwjs(execPath: String)
    /// Electron with Node integration. `appPath` is what `app.getAppPath()` reports, a virtual path.
    case electron(appPath: String)
    /// A plain browser page with no Node.
    case browser
}

/// What an engine plugin tells the shared web host about its game.
public struct WebGameConfiguration {
    public var folder: GameFolder
    /// What NW.js reports as `nw.App.manifest.name`.
    public var appName: String
    public var runtime: PageRuntime
    /// Scripts injected at document start after the Node shim, in order.
    public var userScripts: [String]
    /// Sent instead of WebKit's own user agent.
    public var userAgent: String?
    /// Where the page's localStorage lives, one file per key. Nil leaves localStorage to WebKit.
    public var storageDirectory: URL?
    /// Serves .ogg files transcoded to AAC, for engines that have no Ogg decoder of their own.
    public var transcodesOgg: Bool
    /// The JavaScript that delivers a button press or release to the game. Nil drops the button.
    public var buttonScript: @MainActor (VirtualButton, _ pressed: Bool) -> String?
    /// The DEBUG input script launch argument, such as "rpgmaker-script".
    public var scriptArgument: String
    public var log: Logger

    /// `runtime` defaults to NW.js with index.html as its executable, which is what RPG Maker expects.
    public init(folder: GameFolder, appName: String, runtime: PageRuntime? = nil, userScripts: [String] = [],
                userAgent: String? = nil, storageDirectory: URL? = nil, transcodesOgg: Bool = false,
                buttonScript: @escaping @MainActor (VirtualButton, _ pressed: Bool) -> String?, scriptArgument: String, log: Logger) {
        self.folder = folder
        self.appName = appName
        self.runtime = runtime ?? .nwjs(execPath: folder.mainModule)
        self.userScripts = userScripts
        self.userAgent = userAgent
        self.storageDirectory = storageDirectory
        self.transcodesOgg = transcodesOgg
        self.buttonScript = buttonScript
        self.scriptArgument = scriptArgument
        self.log = log
    }

    /// A desktop Safari user agent. Engines that check for a phone or tablet in it would otherwise lay themselves out for
    /// a mobile browser.
    nonisolated public static let desktopUserAgent = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.0 Safari/605.1.15"

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        try WebGameSession(launch: launch, configuration: self)
    }
}

/// Runs an HTML5 game in a WKWebView that looks enough like NW.js that the game reads and writes its own folder through
/// Node's `fs`, the way its desktop build does.
final class WebGameSession: NSObject, EngineSession {
    static let bridgePrefix = "\u{1}vndegen:"
    static let messageName = "vndegen"

    let view: PlatformView
    private let webView: GameWebView
    private let folder: GameFolder
    private let bridge: NodeBridge
    private let storage: WebStorage?
    private let buttonScript: @MainActor (VirtualButton, Bool) -> String?
    private let scriptArgument: String
    private let log: Logger
    private let onExit: @MainActor () -> Void
    private var ended = false
    private var paused = false
    #if DEBUG
    private var script: Task<Void, Never>?
    #endif

    init(launch: GameLaunch, configuration game: WebGameConfiguration) throws {
        folder = game.folder
        bridge = NodeBridge(folder: folder)
        storage = game.storageDirectory.map(WebStorage.init)
        buttonScript = game.buttonScript
        scriptArgument = game.scriptArgument
        log = game.log
        onExit = launch.onExit
        let shim = try Self.shimSource(for: game)
        let configuration = WKWebViewConfiguration()
        let oggCache = game.transcodesOgg ? OggTranscodeCache(directory: Self.transcodedAudioDirectory, log: log) : nil
        configuration.setURLSchemeHandler(GameSchemeHandler(folder: folder, log: log, oggCache: oggCache), forURLScheme: GameFolder.scheme)
        configuration.mediaTypesRequiringUserActionForPlayback = []
        #if os(iOS)
        configuration.allowsInlineMediaPlayback = true
        configuration.allowsPictureInPictureMediaPlayback = false
        configuration.dataDetectorTypes = []
        configuration.defaultWebpagePreferences.preferredContentMode = .mobile
        #endif
        for source in [shim] + game.userScripts {
            configuration.userContentController.addUserScript(
                WKUserScript(source: source, injectionTime: .atDocumentStart, forMainFrameOnly: true))
        }
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
        webView.allowsLinkPreview = false
        webView.allowsBackForwardNavigationGestures = false
        webView.customUserAgent = game.userAgent
        #if os(iOS)
        webView.isOpaque = true
        webView.backgroundColor = .black
        let scrollView = webView.scrollView
        scrollView.isScrollEnabled = false
        scrollView.bounces = false
        scrollView.bouncesZoom = false
        scrollView.minimumZoomScale = 1
        scrollView.maximumZoomScale = 1
        scrollView.pinchGestureRecognizer?.isEnabled = false
        scrollView.contentInsetAdjustmentBehavior = .never
        scrollView.delaysContentTouches = false
        scrollView.backgroundColor = .black
        #else
        webView.allowsMagnification = false
        #endif
        log.notice("loading \(launch.game.title, privacy: .public) from \(launch.folder.path, privacy: .public)")
        webView.load(URLRequest(url: folder.indexURL))
        #if DEBUG
        startScriptIfRequested()
        #endif
    }

    deinit {
        log.notice("session released")
    }

    /// Shared by every game. The system may empty it when space runs low, which costs only a new transcode.
    static var transcodedAudioDirectory: URL {
        URL.cachesDirectory.appendingPathComponent("TranscodedAudio", isDirectory: true)
    }

    private static func shimSource(for game: WebGameConfiguration) throws -> String {
        guard let url = Bundle.module.url(forResource: "nwjs-shim", withExtension: "js") else { throw WebRuntimeError.shimMissing }
        var config: [String: String] = [
            "mainModule": game.folder.mainModule, "cwd": GameFolder.virtualRoot, "platform": "darwin", "appName": game.appName,
            "execPath": game.folder.mainModule,
        ]
        switch game.runtime {
        case .nwjs(let execPath):
            config["runtime"] = "nwjs"
            config["execPath"] = execPath
        case .electron(let appPath):
            config["runtime"] = "electron"
            config["appPath"] = appPath
        case .browser:
            config["runtime"] = "browser"
        }
        if game.storageDirectory != nil { config["storage"] = "files" }
        let json = String(decoding: try JSONEncoder().encode(config), as: UTF8.self)
        return try String(contentsOf: url, encoding: .utf8).replacingOccurrences(of: "__VNDEGEN_CONFIG__", with: json)
    }

    // MARK: EngineSession

    func send(_ event: ControllerEvent) {
        switch event {
        case let .button(button, pressed):
            guard let script = buttonScript(button, pressed) else { return }
            run(script)
        case .pointer(let event):
            let phase = switch event.phase {
            case .began: "began"
            case .moved: "moved"
            case .ended: "ended"
            case .cancelled: "cancelled"
            }
            run("__vndegen.pointer('\(phase)', \(event.location.x), \(event.location.y), \(Self.buttonNumber(event.button)))")
        case .scroll(let dy):
            run("__vndegen.wheel(\(dy))")
        case let .key(key, pressed):
            run(Self.keyScript(key, pressed: pressed))
        case let .click(button, pressed):
            // The page clicks where the pointer is, which follows the real mouse too.
            run("__vndegen.click('\(pressed ? "began" : "ended")', \(Self.buttonNumber(button)))")
        }
    }

    /// Dispatches a real keydown or keyup, the same way engines deliver their own key strokes.
    nonisolated static func keyScript(_ key: KeyboardKey, pressed: Bool) -> String {
        "__vndegen.key('\(pressed ? "keydown" : "keyup")', \(jsString(key.key)), \(jsString(key.code)), \(key.keyCode))"
    }

    /// The DOM's `MouseEvent.button`.
    nonisolated private static func buttonNumber(_ button: PointerEvent.Button) -> Int {
        button == .primary ? 0 : 2
    }

    /// A JavaScript string literal. Keys come from library.json, so they are escaped rather than trusted.
    nonisolated private static func jsString(_ text: String) -> String {
        String(decoding: (try? JSONEncoder().encode(text)) ?? Data("\"\"".utf8), as: UTF8.self)
    }

    func setPaused(_ paused: Bool) {
        guard self.paused != paused else { return }
        self.paused = paused
        run("__vndegen.setPaused(\(paused))")
        webView.setAllMediaPlaybackSuspended(paused, completionHandler: nil)
        // The hub's overlay took focus while the game was paused.
        if !paused { webView.takeFocus() }
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
        webView.evaluateJavaScript(javaScript) { [log] _, error in
            if let error {
                log.error("js call failed: \(javaScript, privacy: .public): \(error.localizedDescription, privacy: .public)")
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
        case .storageLoad, .storageSet, .storageRemove, .storageClear:
            guard let storage else { return BridgeReply.failure(BridgeError(code: "ENOSYS", message: "no storage for this game")).json }
            return storage.handle(request).json
        default:
            return bridge.handle(request).json
        }
    }
}

// MARK: - WKUIDelegate

extension WebGameSession: WKUIDelegate {
    func webView(_ webView: WKWebView, runJavaScriptTextInputPanelWithPrompt prompt: String, defaultText: String?,
                 initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping @MainActor (String?) -> Void) {
        guard prompt.hasPrefix(Self.bridgePrefix) else {
            log.notice("prompt ignored: \(prompt, privacy: .public)")
            completionHandler(nil)
            return
        }
        completionHandler(answerBridge(String(prompt.dropFirst(Self.bridgePrefix.count))))
    }

    func webView(_ webView: WKWebView, runJavaScriptAlertPanelWithMessage message: String, initiatedByFrame frame: WKFrameInfo,
                 completionHandler: @escaping @MainActor () -> Void) {
        log.notice("alert: \(message, privacy: .public)")
        completionHandler()
    }

    func webView(_ webView: WKWebView, runJavaScriptConfirmPanelWithMessage message: String, initiatedByFrame frame: WKFrameInfo,
                 completionHandler: @escaping @MainActor (Bool) -> Void) {
        log.notice("confirm answered no: \(message, privacy: .public)")
        completionHandler(false)
    }

    func webView(_ webView: WKWebView, createWebViewWith configuration: WKWebViewConfiguration,
                 for navigationAction: WKNavigationAction, windowFeatures: WKWindowFeatures) -> WKWebView? {
        if let url = navigationAction.request.url { openExternal(url) }
        return nil
    }
}

// MARK: - WKNavigationDelegate

extension WebGameSession: WKNavigationDelegate {
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
        log.error("navigation failed: \(error.localizedDescription, privacy: .public)")
    }

    func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: any Error) {
        log.error("navigation failed: \(error.localizedDescription, privacy: .public)")
    }

    func webViewWebContentProcessDidTerminate(_ webView: WKWebView) {
        log.error("web content process died, reloading the game")
        webView.reload()
    }
}

// MARK: - WKScriptMessageHandler

extension WebGameSession: WKScriptMessageHandler {
    func userContentController(_ userContentController: WKUserContentController, didReceive message: WKScriptMessage) {
        guard let body = message.body as? [String: Any], let text = body["message"] as? String else { return }
        switch body["level"] as? String {
        case "error": log.error("js: \(text, privacy: .public)")
        case "warn": log.warning("js: \(text, privacy: .public)")
        default: log.notice("js: \(text, privacy: .public)")
        }
    }
}

/// WKUserContentController retains its handlers, so the session goes behind a weak link to avoid a cycle with its web view.
private final class WeakMessageHandler: NSObject, WKScriptMessageHandler {
    private weak var target: WebGameSession?

    init(_ target: WebGameSession) {
        self.target = target
    }

    func userContentController(_ userContentController: WKUserContentController, didReceive message: WKScriptMessage) {
        target?.userContentController(userContentController, didReceive: message)
    }
}

/// Takes keyboard focus whenever it is on screen. Without it the page's document never has focus, so `:focus` matches
/// nothing and engines that move a keyboard focus between buttons, such as TyranoScript 5, cannot.
final class GameWebView: WKWebView {
    #if os(macOS)
    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        takeFocus()
    }

    func takeFocus() {
        window?.makeFirstResponder(self)
    }
    #else
    override func didMoveToWindow() {
        super.didMoveToWindow()
        takeFocus()
    }

    func takeFocus() {
        if window != nil { becomeFirstResponder() }
    }
    #endif
}

nonisolated enum WebRuntimeError: LocalizedError {
    case shimMissing

    var errorDescription: String? {
        switch self {
        case .shimMissing: "The NW.js compatibility script is missing from the app bundle."
        }
    }
}

// MARK: - Debug script

#if DEBUG
extension WebGameSession {
    private func startScriptIfRequested() {
        let steps: [InputStep]
        do {
            guard let parsed = try InputScript.fromLaunchArguments(scriptArgument) else { return }
            steps = parsed
        } catch {
            log.error("script: \(String(describing: error), privacy: .public)")
            return
        }
        log.notice("script: \(steps.count) steps")
        script = Task { [weak self, log] in
            for step in steps {
                guard let self, !Task.isCancelled else { return }
                await self.perform(step)
            }
            log.notice("script: done")
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
        case .js(let code):
            let result = await evaluate(code)
            log.notice("script js: \(code, privacy: .public) => \(result, privacy: .public)")
        case let .until(seconds, condition):
            await poll(condition, for: seconds, every: 0.25, label: "until") {}
        case let .repeatPress(button, seconds, condition):
            await poll(condition, for: seconds, every: 0.8, label: "repeat \(button.rawValue)") {
                await self.perform(.press(button))
            }
        case .fps(let seconds):
            await measureFrameRate(seconds)
        case .log(let text):
            log.notice("script: \(text, privacy: .public)")
        case .quit:
            quitFromGame()
        }
    }

    /// Evaluates `condition` until it is truthy or `seconds` pass, running `step` and waiting `interval` between tries.
    private func poll(_ condition: String, for seconds: Double, every interval: Double, label: String, step: () async -> Void) async {
        let start = Date()
        var met = false
        var tries = 0
        while !Task.isCancelled, Date().timeIntervalSince(start) < seconds {
            met = await evaluate("!!(\(condition))") == "1"
            if met { break }
            await step()
            tries += 1
            await sleep(interval)
        }
        log.notice("script \(label, privacy: .public): \(met ? "met" : "timed out", privacy: .public) after \(tries) tries, \(Date().timeIntervalSince(start), format: .fixed(precision: 1)) s: \(condition, privacy: .public)")
    }

    /// Counts requestAnimationFrame callbacks (what the compositor delivers) and RPG Maker's Graphics.frameCount (engine updates)
    /// over a window. The sampler exists only while this step runs.
    private func measureFrameRate(_ seconds: Double) async {
        let sampler = """
        const start = performance.now(), engineStart = window.Graphics ? Graphics.frameCount : 0;
        let frames = 0, worst = 0, last = start;
        await new Promise((done) => {
            const tick = (now) => {
                frames++;
                worst = Math.max(worst, now - last);
                last = now;
                if (now - start < seconds * 1000) requestAnimationFrame(tick); else done();
            };
            requestAnimationFrame(tick);
        });
        const elapsed = (performance.now() - start) / 1000;
        const engine = window.Graphics ? Graphics.frameCount - engineStart : 0;
        return `raf ${(frames / elapsed).toFixed(1)} fps, engine ${(engine / elapsed).toFixed(1)} fps, worst frame ${worst.toFixed(1)} ms over ${elapsed.toFixed(1)} s`;
        """
        do {
            let result = try await webView.callAsyncJavaScript(sampler, arguments: ["seconds": seconds], contentWorld: .page)
            log.notice("script fps: \(String(describing: result ?? "nil"), privacy: .public)")
        } catch {
            log.error("script fps failed: \(error.localizedDescription, privacy: .public)")
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
}
#endif
