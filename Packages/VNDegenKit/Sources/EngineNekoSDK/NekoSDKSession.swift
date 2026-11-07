import CNekoSDK
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

nonisolated let nekoSDKLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "nekosdk")

/// A NekoSDK game running on VNDegen's runtime in this process. The runtime steps once per display refresh on the main
/// thread and draws into a CPU frame that `FrameView` shows; its sound plays through `PCMAudioOutput`. The game ending
/// itself ends the session through `onExit`.
final class NekoSDKSession: EngineSession {
    let view: PlatformView
    private let frameView = FrameView()
    private let launch: GameLaunch
    private let root: URL
    private var engine: OpaquePointer?
    private var audio: PCMAudioOutput?
    private var displayLink: CADisplayLink?
    private var lastTimestamp: CFTimeInterval?
    #if DEBUG
    private let pacing = UserDefaults.standard.bool(forKey: "frameMonitor") ? FramePacing(tag: "NEKOFRAMES") : nil
    #endif
    private var scrolledNotches = 0.0
    private var scrolledPoints = 0.0
    private var paused = false
    private var stopped = false
    private var stopWaiters: [CheckedContinuation<Void, Never>] = []

    init(_ launch: GameLaunch, root: URL) {
        self.launch = launch
        self.root = root
        view = frameView
        frameView.onReady = { [weak self] in self?.start() }
        frameView.onPointer = { [weak self] event in self?.pointer(event, at: event.location) }
        frameView.onScroll = { [weak self] notches in
            guard let self, let engine else { return }
            // A trackpad reports fractions of a notch, which add up over a slow scroll.
            scrolledNotches += notches
            let whole = Int(scrolledNotches)
            guard whole != 0 else { return }
            scrolledNotches -= Double(whole)
            vnneko_mouse_wheel(engine, Int32(whole))
        }
        frameView.onKey = { [weak self] usage, pressed, _ in self?.hostKey(usage, pressed) }
    }

    private func start() {
        guard engine == nil, !stopped else { return }
        let logURL = launch.dataDirectory.appendingPathComponent("logs/nekosdk.txt")
        try? FileManager.default.createDirectory(at: logURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        try? Data().write(to: logURL)
        // The movie decoder is vnffmovie in the KiriKiri framework the app embeds. Without it the game runs with its
        // movies left out.
        let movies = vnneko_find_movies()
        if movies == nil {
            let missing = vnneko_start_error().map { String(cString: $0) } ?? "unknown"
            nekoSDKLog.notice("movies left out: the KiriKiri framework's decoder isn't loaded (\(missing, privacy: .public))")
        }
        // The Windows font pack is already set up app-wide (FontPack), so the runtime is given no folder of its own.
        let started: OpaquePointer? = root.path.withCString { rootPath in
            logURL.path.withCString { logPath in
                launch.dataDirectory.path.withCString { dataPath in
                    var settings = VNNekoLaunch(gameRoot: rootPath, dataDir: dataPath, logPath: logPath, fontDir: nil, movies: movies)
                    return vnneko_start(&settings)
                }
            }
        }
        guard let started else {
            let reason = vnneko_start_error().map { String(cString: $0) } ?? "unknown"
            nekoSDKLog.error("\(self.launch.game.id.rawValue, privacy: .public) did not start: \(reason, privacy: .public)")
            finish(byGame: true)
            return
        }
        engine = started
        nekoSDKLog.info("started \(self.launch.game.id.rawValue, privacy: .public)")
        let box = EngineBox(started)
        audio = PCMAudioOutput { out, frames in vnneko_audio_render(box.engine, out, Int32(frames)) }
        audio?.start()
        if paused { setPaused(true) }
        frameView.takeFocus()
        #if canImport(UIKit)
        let link = CADisplayLink(target: FrameTarget(self), selector: #selector(FrameTarget.step(_:)))
        #else
        let link = frameView.displayLink(target: FrameTarget(self), selector: #selector(FrameTarget.step(_:)))
        #endif
        link.add(to: .main, forMode: .common)
        displayLink = link
    }

    fileprivate func step(_ link: CADisplayLink) {
        guard let engine else { return }
        let now = link.timestamp
        let ms = lastTimestamp.map { Int(((now - $0) * 1000).rounded()) } ?? 16
        lastTimestamp = now
        guard !paused else { return }
        #if DEBUG
        let began = CACurrentMediaTime()
        #endif
        if !vnneko_step(engine, Int32(min(max(ms, 0), 100))) {
            finish(byGame: true)
            return
        }
        var w: Int32 = 0, h: Int32 = 0
        var changed = false
        if let pixels = vnneko_frame(engine, &w, &h, &changed), changed {
            frameView.present(pixels, width: Int(w), height: Int(h))
        }
        #if DEBUG
        let ended = CACurrentMediaTime()
        pacing?.note(tick: now, presented: changed, stepEnd: ended, target: link.targetTimestamp - link.timestamp, workTime: ended - began,
                     progress: "text=\(vnneko_debug_text_count(engine))")
        #endif
    }

    private func pointer(_ event: PointerEvent, at location: CGPoint) {
        guard let engine else { return }
        vnneko_mouse_move(engine, Float(location.x), Float(location.y))
        let button: Int32 = event.button == .primary ? 0 : 1
        switch event.phase {
        case .began: vnneko_mouse_button(engine, button, true)
        case .ended, .cancelled: vnneko_mouse_button(engine, button, false)
        case .moved: break
        }
    }

    private func finish(byGame: Bool) {
        displayLink?.invalidate()
        displayLink = nil
        audio?.stop()
        audio = nil
        if let engine {
            if let problem = vnneko_problem(engine) {
                nekoSDKLog.error("\(self.launch.game.id.rawValue, privacy: .public) stopped: \(String(cString: problem), privacy: .public)")
            }
            vnneko_stop(engine)
            self.engine = nil
        }
        stopped = true
        let waiters = stopWaiters
        stopWaiters = []
        waiters.forEach { $0.resume() }
        if byGame { launch.onExit() }
    }

    // MARK: EngineSession

    func send(_ event: ControllerEvent) {
        guard let engine else { return }
        switch event {
        case let .button(button, pressed):
            control(button, pressed)
        case let .gamepad(input, pressed):
            if let button = input.virtualButton { control(button, pressed) }
        case let .click(button, pressed):
            vnneko_mouse_button(engine, button == .primary ? 0 : 1, pressed)
        case let .pointer(pointer):
            self.pointer(pointer, at: frameView.framePoint(fromViewNormalized: pointer.location))
        case let .scroll(dy):
            scrolledPoints += dy
            let notches = Int(scrolledPoints / 40)
            guard notches != 0 else { return }
            scrolledPoints -= Double(notches) * 40
            // Content scrolling down is the wheel turned toward the player.
            vnneko_mouse_wheel(engine, Int32(-notches))
        case .key:
            break
        }
    }

    /// Confirm clicks where the pointer is, Cancel is the right button, Dash skips while held, the direction pad moves
    /// between a menu's controls, and page up and down turn the wheel, which opens and scrolls the backlog.
    private func control(_ button: VirtualButton, _ pressed: Bool) {
        guard let engine else { return }
        switch button {
        case .confirm: vnneko_mouse_button(engine, 0, pressed)
        case .cancel, .menu: vnneko_mouse_button(engine, 1, pressed)
        case .dash: vnneko_set_skip(engine, pressed)
        case .up: if pressed { _ = vnneko_move_focus(engine, 0, -1) }
        case .down: if pressed { _ = vnneko_move_focus(engine, 0, 1) }
        case .left: if pressed { _ = vnneko_move_focus(engine, -1, 0) }
        case .right: if pressed { _ = vnneko_move_focus(engine, 1, 0) }
        case .pageUp: if pressed { vnneko_mouse_wheel(engine, 1) }
        case .pageDown: if pressed { vnneko_mouse_wheel(engine, -1) }
        case .hub: break
        }
    }

    /// A hardware keyboard: Return and Space click, Escape is the right button, Ctrl skips while held, and the arrows move.
    private func hostKey(_ usage: Int, _ pressed: Bool) {
        switch usage {
        case 40, 88, 44: control(.confirm, pressed)
        case 41: control(.cancel, pressed)
        case 224, 228: control(.dash, pressed)
        case 79: control(.right, pressed)
        case 80: control(.left, pressed)
        case 81: control(.down, pressed)
        case 82: control(.up, pressed)
        case 75: control(.pageUp, pressed)
        case 78: control(.pageDown, pressed)
        default: break
        }
    }

    func setPaused(_ paused: Bool) {
        self.paused = paused
        guard let engine else { return }
        vnneko_set_paused(engine, paused)
        if paused { audio?.pause() } else { audio?.resume() }
        lastTimestamp = nil
        if !paused { frameView.takeFocus() }
    }

    func terminate() {
        guard !stopped else { return }
        finish(byGame: false)
    }

    func waitUntilStopped() async {
        if stopped { return }
        await withCheckedContinuation { stopWaiters.append($0) }
    }
}

/// The engine pointer, handed to the audio thread. vnneko_audio_render is safe to call there.
nonisolated private final class EngineBox: @unchecked Sendable {
    let engine: OpaquePointer
    init(_ engine: OpaquePointer) { self.engine = engine }
}

/// A display link keeps its target, so the session sits behind a weak reference to avoid a cycle.
private final class FrameTarget: NSObject {
    private weak var session: NekoSDKSession?
    init(_ session: NekoSDKSession) { self.session = session }
    @objc func step(_ link: CADisplayLink) { session?.step(link) }
}
