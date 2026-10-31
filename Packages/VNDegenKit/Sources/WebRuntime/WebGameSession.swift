import FontPack
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
    /// Where the page's localStorage lives. Nil leaves localStorage to WebKit.
    public var storage: (any PageStorage)?
    /// The served path, with any query, of the page the game starts on.
    public var startPage: String
    /// Decodes Ogg Vorbis in the page, for engines that have no Ogg decoder of their own.
    public var decodesOgg: Bool
    /// Windows font names, as `FontPack.referencedNames` finds them in the game's files, whose substitutes load before
    /// the page draws. Every other substitute loads when the page first uses it.
    public var preloadFonts: Set<String> = []
    /// The JavaScript that delivers an engine-neutral button press or release to the game, as DEBUG input scripts send
    /// them. Controllers reach the game as a gamepad instead. Nil drops the button.
    public var buttonScript: @MainActor (VirtualButton, _ pressed: Bool) -> String?
    /// The DEBUG input script launch argument, such as "rpgmaker-script".
    public var scriptArgument: String
    public var log: Logger
    /// The game's cheats, made the first time VNDegen's menu asks for them, around the session's page. Nil for engines
    /// that offer none.
    public var cheats: (@MainActor (any GamePage) -> any CheatSession)?

    /// `runtime` defaults to NW.js with index.html as its executable, which is what RPG Maker expects. A
    /// `storageDirectory` keeps localStorage in `WebStorage` files there, and `storage` replaces it with another store.
    public init(folder: GameFolder, appName: String, runtime: PageRuntime? = nil, userScripts: [String] = [],
                userAgent: String? = nil, storageDirectory: URL? = nil, storage: (any PageStorage)? = nil,
                startPage: String = "/index.html", decodesOgg: Bool = false,
                buttonScript: @escaping @MainActor (VirtualButton, _ pressed: Bool) -> String?, scriptArgument: String, log: Logger) {
        self.folder = folder
        self.appName = appName
        self.runtime = runtime ?? .nwjs(execPath: folder.mainModule)
        self.userScripts = userScripts
        self.userAgent = userAgent
        self.storage = storage ?? storageDirectory.map(WebStorage.init)
        self.startPage = startPage
        self.decodesOgg = decodesOgg
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

/// A running game's page, for engine code that reads and changes the game's state.
@MainActor
public protocol GamePage: AnyObject {
    /// Runs `body` as the body of an async function whose parameters are `arguments`' keys, and returns what it returns.
    func callFunction(_ body: String, arguments: [String: Any]) async throws -> Any?
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
    private let storage: (any PageStorage)?
    private let buttonScript: @MainActor (VirtualButton, Bool) -> String?
    private let scriptArgument: String
    private let log: Logger
    private let onExit: @MainActor () -> Void
    private let makeCheats: (@MainActor (any GamePage) -> any CheatSession)?
    /// Made once, the first time the menu asks, so the cheats keep what they track for the whole session.
    lazy var cheats: (any CheatSession)? = makeCheats?(self)
    private var ended = false
    private var paused = false
    /// Ends once the page has had its last word after `terminate()` and the view is torn down.
    private var stopping: Task<Void, Never>?
    #if DEBUG
    private var script: Task<Void, Never>?
    #endif

    init(launch: GameLaunch, configuration game: WebGameConfiguration) throws {
        folder = game.folder
        bridge = NodeBridge(folder: folder)
        storage = game.storage
        buttonScript = game.buttonScript
        scriptArgument = game.scriptArgument
        log = game.log
        onExit = launch.onExit
        makeCheats = game.cheats
        let shim = try Self.shimSource(for: game)
        let fonts = try Self.fontScript(preload: game.preloadFonts)
        let ogg = game.decodesOgg ? [try Self.oggScript()] : []
        // Version 0.2.0 transcoded Ogg to AAC files here.
        Task.detached(priority: .background) {
            try? FileManager.default.removeItem(at: URL.cachesDirectory.appendingPathComponent("TranscodedAudio"))
        }
        let configuration = WKWebViewConfiguration()
        configuration.setURLSchemeHandler(GameSchemeHandler(folder: folder, log: log), forURLScheme: GameFolder.scheme)
        configuration.mediaTypesRequiringUserActionForPlayback = []
        #if os(iOS)
        configuration.allowsInlineMediaPlayback = true
        configuration.allowsPictureInPictureMediaPlayback = false
        configuration.dataDetectorTypes = []
        configuration.defaultWebpagePreferences.preferredContentMode = .mobile
        #endif
        for source in [shim, fonts] + ogg + game.userScripts {
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
        #if os(macOS)
        // -ignoreOcclusion YES keeps the page running while no window shows, as on a test Mac whose screen is locked,
        // where WebKit would hide the page and stop its animation frames.
        if UserDefaults.standard.bool(forKey: "ignoreOcclusion") { Self.setOcclusionDetection(false, of: webView) }
        #endif
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
        webView.load(URLRequest(url: URL(string: GameFolder.origin + game.startPage) ?? folder.indexURL))
        #if DEBUG
        startScriptIfRequested()
        #endif
    }

    deinit {
        log.notice("session released")
    }

    #if DEBUG && os(macOS)
    /// WebKit's `-[WKWebView _setWindowOcclusionDetectionEnabled:]`, for test runs only.
    private static func setOcclusionDetection(_ enabled: Bool, of webView: WKWebView) {
        let selector = NSSelectorFromString("_setWindowOcclusionDetectionEnabled:")
        guard let method = class_getInstanceMethod(WKWebView.self, selector) else { return }
        typealias Setter = @convention(c) (AnyObject, Selector, Bool) -> Void
        unsafeBitCast(method_getImplementation(method), to: Setter.self)(webView, selector, enabled)
    }
    #endif

    /// What the page may use to decode Ogg Vorbis. A long track at 44.1 kHz in stereo is about 4.5 million samples a
    /// minute, so `maxSamples` allows about 11 minutes.
    struct OggLimits: Encodable {
        var workers = 2
        var maxInputBytes = 64 << 20
        var maxSamples = 60_000_000
        var maxCacheBytes = 192 << 20
    }

    static func oggScript(limits: OggLimits = OggLimits()) throws -> String {
        guard let script = Bundle.module.url(forResource: "ogg-vorbis", withExtension: "js"),
              let wasm = Bundle.module.url(forResource: "vorbis", withExtension: "wasm")
        else { throw WebRuntimeError.oggDecoderMissing }
        struct Config: Encodable {
            var wasm: String
            var limits: OggLimits
        }
        let json = try JSONEncoder().encode(Config(wasm: try Data(contentsOf: wasm).base64EncodedString(), limits: limits))
        return try String(contentsOf: script, encoding: .utf8)
            .replacingOccurrences(of: "__VNDEGEN_OGG__", with: String(decoding: json, as: UTF8.self))
    }

    /// Where the page finds the font substitutes. No game has a folder by this name.
    nonisolated static let fontPath = GameFolder.fontPath

    /// Gives the page a font face for each Windows name in the font table whose substitute is installed.
    static func fontScript(preload: Set<String>) throws -> String {
        guard let url = Bundle.module.url(forResource: "fonts", withExtension: "js") else { throw WebRuntimeError.fontScriptMissing }
        struct Face: Encodable {
            var name: String
            var file: String
            var bold: Bool
            var italic: Bool
        }
        struct Config: Encodable {
            var base: String
            var faces: [Face]
            var preload: [String]
        }
        let faces = FontPack.entries.map {
            Face(name: $0.name, file: $0.match.file.lastPathComponent, bold: $0.match.bold, italic: $0.match.italic)
        }
        let config = Config(base: GameFolder.origin + fontPath, faces: faces, preload: preload.sorted())
        let json = String(decoding: try JSONEncoder().encode(config), as: UTF8.self)
        return try String(contentsOf: url, encoding: .utf8).replacingOccurrences(of: "__VNDEGEN_FONTS__", with: json)
    }

    static func shimSource(for game: WebGameConfiguration) throws -> String {
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
        if game.storage != nil { config["storage"] = "files" }
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
        case let .gamepad(input, pressed):
            run(Self.gamepadScript(input, pressed: pressed))
        }
    }

    /// Text goes into the page's text field, as typing would put it there. A page with none gets key presses.
    func type(_ typed: TypedText) {
        switch typed {
        case let .text(text): run("__vndegen.text('insert', \(Self.jsString(text)))")
        case .newline: run("__vndegen.text('newline', '')")
        case .deleteBackward: run("__vndegen.text('delete', '')")
        }
    }

    /// Presses or releases a button of the page's virtual Xbox gamepad. The engine-neutral action goes along for engine
    /// scripts that turn the gamepad into their own input when the game reads no gamepad.
    nonisolated static func gamepadScript(_ input: ControlInput, pressed: Bool) -> String {
        "__vndegen.gamepad('\(input.rawValue)', \(pressed), \(input.virtualButton.map { "'\($0.rawValue)'" } ?? "null"))"
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
        guard stopping == nil else { return }
        ended = true
        #if DEBUG
        script?.cancel()
        #endif
        // The page pauses, then writes what it keeps until it quits, such as Flash saves, through the bridge, which needs
        // the delegates this tears down. A page that doesn't answer gets a few seconds.
        stopping = Task {
            await evaluate(Self.beforeQuitScript, within: .seconds(5))
            tearDown()
        }
    }

    func waitUntilStopped() async {
        await stopping?.value
    }

    nonisolated static let beforeQuitScript = "window.__vndegen && (__vndegen.setPaused(true), __vndegen.beforeQuit())"

    /// Runs `javaScript` and returns once it has run or `limit` has passed.
    private func evaluate(_ javaScript: String, within limit: Duration) async {
        let once = Once()
        await withCheckedContinuation { (done: CheckedContinuation<Void, Never>) in
            once.action = { done.resume() }
            webView.evaluateJavaScript(javaScript) { [log] _, error in
                if let error { log.error("before quit: \(error.localizedDescription, privacy: .public)") }
                once.run()
            }
            Task {
                try? await Task.sleep(for: limit)
                once.run()
            }
        }
    }

    private func tearDown() {
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

// MARK: - GamePage

extension WebGameSession: GamePage {
    func callFunction(_ body: String, arguments: [String: Any]) async throws -> Any? {
        guard !ended else { throw CancellationError() }
        return try await webView.callAsyncJavaScript(body, arguments: arguments, contentWorld: .page)
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

/// Runs its action the first time `run` is called.
@MainActor private final class Once {
    var action: (() -> Void)?

    func run() {
        action?()
        action = nil
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
    case oggDecoderMissing
    case fontScriptMissing

    var errorDescription: String? {
        switch self {
        case .shimMissing: "The NW.js compatibility script is missing from the app bundle."
        case .oggDecoderMissing: "The Ogg Vorbis decoder is missing from the app bundle."
        case .fontScriptMissing: "The font script is missing from the app bundle."
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
        case let .mash(input, count, hold, gap):
            for _ in 0..<count {
                send(.gamepad(input, pressed: true))
                await sleep(hold)
                send(.gamepad(input, pressed: false))
                await sleep(gap)
            }
        case let .mashKey(button, count, hold, gap):
            for _ in 0..<count {
                send(.button(button, pressed: true))
                await sleep(hold)
                send(.button(button, pressed: false))
                await sleep(gap)
            }
        case let .padWhen(input, seconds, condition):
            if await poll(condition, for: seconds, every: 0.016, label: "padwhen \(input.rawValue)", step: {}) {
                await perform(.mash(input, count: 1, hold: 0.05, gap: 0))
            }
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
    /// Returns whether the condition was met.
    @discardableResult
    private func poll(_ condition: String, for seconds: Double, every interval: Double, label: String, step: () async -> Void) async -> Bool {
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
        return met
    }

    /// Counts requestAnimationFrame callbacks (what the compositor delivers), RPG Maker's Graphics.frameCount (engine
    /// updates), and the animation frames in which WebGL drew to the page's canvas, which is each frame an engine such as
    /// Ruffle draws, with their 1% low. The sampler exists only while this step runs.
    private func measureFrameRate(_ seconds: Double) async {
        let sampler = """
        const start = performance.now(), engineStart = window.Graphics ? Graphics.frameCount : 0;
        // A draw call reaches the canvas while no framebuffer is bound for drawing. Bindings are tracked as they change,
        // and a context's binding from before the step is asked for once.
        const restore = [];
        let drew = false;
        const wrap = (proto, name, body) => {
            const original = proto[name];
            restore.push(() => { proto[name] = original; });
            proto[name] = function (...args) { body(this, args); return original.apply(this, args); };
        };
        const bound = new WeakMap();
        for (const proto of [window.WebGLRenderingContext, window.WebGL2RenderingContext].filter(Boolean).map((c) => c.prototype)) {
            wrap(proto, 'bindFramebuffer', (gl, [target, framebuffer]) => {
                if (target === gl.FRAMEBUFFER || target === gl.DRAW_FRAMEBUFFER) bound.set(gl, framebuffer);
            });
            const draw = (gl) => {
                if (!bound.has(gl)) bound.set(gl, gl.getParameter(gl.FRAMEBUFFER_BINDING));
                // The default framebuffer is null, or undefined from engines built with wasm-bindgen.
                if (bound.get(gl) == null) drew = true;
            };
            for (const name of ['drawArrays', 'drawElements', 'drawArraysInstanced', 'drawElementsInstanced', 'blitFramebuffer', 'clear']) {
                if (proto[name]) wrap(proto, name, draw);
            }
        }
        const presents = [];
        let frames = 0, worst = 0, last = start;
        try {
            await new Promise((done) => {
                const tick = (now) => {
                    frames++;
                    if (drew) { presents.push(now); drew = false; }
                    worst = Math.max(worst, now - last);
                    last = now;
                    if (now - start < seconds * 1000) requestAnimationFrame(tick); else done();
                };
                requestAnimationFrame(tick);
            });
        } finally {
            restore.reverse().forEach((undo) => undo());
        }
        const elapsed = (performance.now() - start) / 1000;
        const engine = window.Graphics ? Graphics.frameCount - engineStart : 0;
        const gaps = presents.slice(1).map((t, i) => t - presents[i]).sort((a, b) => a - b);
        const low = gaps.length ? 1000 / gaps[Math.min(gaps.length - 1, Math.floor(gaps.length * 0.99))] : 0;
        return `raf ${(frames / elapsed).toFixed(1)} fps, engine ${(engine / elapsed).toFixed(1)} fps, drawn ${(presents.length / elapsed).toFixed(1)} fps, 1% low ${low.toFixed(1)} fps, worst frame ${worst.toFixed(1)} ms over ${elapsed.toFixed(1)} s`;
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
