import CNScripter
import FrameHost
import Foundation
import OSLog
import QuartzCore
import VNCore
#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

nonisolated let nScripterLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "nscripter")

/// An NScripter game running on VNDegen's runtime in this process. The runtime steps once per display refresh on the
/// main thread and draws into a CPU frame that `FrameView` shows; its sound plays through `PCMAudioOutput`. The game
/// ending itself ends the session through `onExit`. When the hub ends it instead, the runtime writes the game's system
/// data first.
final class NScripterSession: EngineSession {
    var failedToStart: Bool { driver.failedToStart }
    var startFailureReason: String? { driver.startFailureReason }
    let view: PlatformView
    private let driver: FrameDriver
    private var frameView: FrameView { driver.view }
    private var engine: OpaquePointer? { driver.engine }
    private let launch: GameLaunch
    private let root: URL
    private var router = NScripterInputRouter()
    private var lifeCycleObservers: [NSObjectProtocol] = []

    init(_ launch: GameLaunch, root: URL) {
        self.launch = launch
        self.root = root
        let id = launch.game.id.rawValue
        driver = FrameDriver(pacingTag: "NSFRAMES", setPaused: { engine, paused in
            vnns_set_paused(engine, paused)
            // A paused app may be killed without warning too.
            if paused { vnns_save_system_data(engine) }
        }, close: { engine in
            if let problem = vnns_problem(engine) {
                nScripterLog.error("\(id, privacy: .public) stopped: \(String(cString: problem), privacy: .public)")
            }
            vnns_stop(engine)
        }, onExit: launch.onExit)
        view = driver.view
        driver.willFinish = { [weak self] in self?.stopObservingLifeCycle() }
        frameView.onReady = { [weak self] in self?.start() }
        frameView.onPointer = { [weak self] event in
            guard let self else { return }
            deliver(router.pointer(event, at: event.location))
        }
        frameView.onScroll = { [weak self] notches in
            guard let self else { return }
            let whole = Int(notches.rounded())
            if whole != 0 { deliver([.wheel(whole)]) }
        }
        frameView.onKey = { [weak self] usage, pressed, characters in
            guard let self else { return }
            deliver(router.hostKey(hidUsage: usage, pressed: pressed))
            if pressed, let characters, !characters.isEmpty, characters.unicodeScalars.allSatisfy({ $0.value >= 0x20 && $0.value < 0xF700 }) {
                deliver([.text(characters)])
            }
        }
    }

    private func start() {
        guard driver.canStart else { return }
        let logURL = launch.dataDirectory.appendingPathComponent("logs/nscripter.txt")
        try? FileManager.default.createDirectory(at: logURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        try? Data().write(to: logURL)
        let started: OpaquePointer? = root.path.withCString { rootPath in
            logURL.path.withCString { logPath in
                var settings = VNNSLaunch(gameRoot: rootPath, logPath: logPath, englishWordWrap: NScripterSession.wantsWordWrap(root))
                return vnns_start(&settings)
            }
        }
        guard let started else {
            let reason = vnns_start_error().map { String(cString: $0) }
            nScripterLog.error("\(self.launch.game.id.rawValue, privacy: .public) did not start: \(reason ?? "unknown", privacy: .public)")
            driver.failToStart(reason)
            return
        }
        nScripterLog.info("started \(self.launch.game.id.rawValue, privacy: .public)")
        driver.run(started, audio: { vnns_audio_render($0, $1, $2) }) { [weak self] engine, ms, link in
            self?.step(engine, ms, link)
        }
        if driver.paused { setPaused(true) }
        observeLifeCycle()
    }

    /// iOS may end an app in the background without warning, and a player may force-quit it. The PC engine writes
    /// global variables and the read-text record only at saves and on exit, so they are written whenever the app
    /// leaves the foreground too.
    private func observeLifeCycle() {
        #if canImport(UIKit)
        let names = [UIApplication.willResignActiveNotification, UIApplication.didEnterBackgroundNotification,
                     UIApplication.willTerminateNotification]
        #else
        let names = [NSApplication.willResignActiveNotification, NSApplication.willTerminateNotification]
        #endif
        lifeCycleObservers = names.map { name in
            NotificationCenter.default.addObserver(forName: name, object: nil, queue: .main) { [weak self] _ in
                MainActor.assumeIsolated { self?.saveSystemData() }
            }
        }
    }

    private func stopObservingLifeCycle() {
        lifeCycleObservers.forEach(NotificationCenter.default.removeObserver)
        lifeCycleObservers = []
    }

    private func saveSystemData() {
        if let engine { vnns_save_system_data(engine) }
    }

    private func step(_ engine: OpaquePointer, _ ms: Int, _ link: CADisplayLink) {
        if !vnns_step(engine, Int32(min(max(ms, 0), 100))) {
            driver.finish(byGame: true)
            return
        }
        var w: Int32 = 0, h: Int32 = 0
        var changed = false
        if let pixels = vnns_frame(engine, &w, &h, &changed), changed {
            frameView.present(pixels, width: Int(w), height: Int(h))
        }
        #if DEBUG
        driver.pacing?.note(link, presented: changed)
        #endif
    }

    private func deliver(_ inputs: [NScripterInput]) {
        guard let engine else { return }
        for input in inputs {
            switch input {
            case let .key(code, down): vnns_key(engine, code, down)
            case let .mouseButton(button, down): vnns_mouse_button(engine, button, down)
            case let .mouseMove(p): vnns_mouse_move(engine, Float(p.x), Float(p.y))
            case let .wheel(n): vnns_wheel(engine, Int32(n))
            case let .text(s): vnns_text(engine, s)
            }
        }
    }

    /// Whether the game's English text breaks between words. See docs/nscripter.md for the trigger.
    nonisolated static func wantsWordWrap(_ root: URL) -> Bool { false }

    // MARK: EngineSession

    func send(_ event: ControllerEvent) {
        let frame = frameView
        deliver(router.translate(event) { frame.framePoint(fromViewNormalized: $0) })
    }

    func type(_ typed: TypedText) {
        switch typed {
        case let .text(text): deliver([.text(text)])
        case .newline: deliver([.key(Int32(VNNS_KEY_RETURN), down: true), .key(Int32(VNNS_KEY_RETURN), down: false)])
        case .deleteBackward:
            deliver([.key(Int32(VNNS_KEY_BACKSPACE), down: true), .key(Int32(VNNS_KEY_BACKSPACE), down: false)])
        }
    }

    func setPaused(_ paused: Bool) {
        driver.setPaused(paused) { _ in
            if paused { deliver(router.releaseAll()) }
        }
    }

    func terminate() {
        if !driver.stopped, let engine {
            vnns_request_quit(engine)
            vnns_step(engine, 0)
        }
        driver.terminate()
    }

    func gameMenu() async -> [GameMenuItem] {
        guard let engine, !driver.stopped, let json = vnns_menu(engine) else { return [] }
        return NScripterMenu.items(json: String(cString: json))
    }

    func chooseGameMenuItem(_ id: GameMenuItem.ID) {
        guard let engine, !driver.stopped else { return }
        if !vnns_menu_click(engine, id) {
            nScripterLog.error("\(self.launch.game.id.rawValue, privacy: .public) has no menu item \(id, privacy: .public)")
        }
    }

    func waitUntilStopped() async {
        await driver.waitUntilStopped()
    }
}
