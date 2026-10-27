import CNScripter
import FrameHost
import Foundation
import OSLog
import QuartzCore
import VNCore

nonisolated let nScripterLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "nscripter")

/// An NScripter game running on VNDegen's runtime in this process. The runtime steps once per display refresh on the
/// main thread and draws into a CPU frame that `FrameView` shows; its sound plays through `PCMAudioOutput`. The game
/// ending itself ends the session through `onExit`. When the hub ends it instead, the runtime writes the game's system
/// data first.
final class NScripterSession: EngineSession {
    let view: PlatformView
    private let frameView = FrameView()
    private let launch: GameLaunch
    private let root: URL
    private var engine: OpaquePointer?
    private var audio: PCMAudioOutput?
    private var displayLink: CADisplayLink?
    private var lastTimestamp: CFTimeInterval?
    private var router = NScripterInputRouter()
    private var paused = false
    private var stopped = false
    private var stopWaiters: [CheckedContinuation<Void, Never>] = []

    init(_ launch: GameLaunch, root: URL) {
        self.launch = launch
        self.root = root
        view = frameView
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
        guard engine == nil, !stopped else { return }
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
            let reason = vnns_start_error().map { String(cString: $0) } ?? "unknown"
            nScripterLog.error("\(self.launch.game.id.rawValue, privacy: .public) did not start: \(reason, privacy: .public)")
            finish(byGame: true)
            return
        }
        engine = started
        nScripterLog.info("started \(self.launch.game.id.rawValue, privacy: .public)")
        let box = EngineBox(started)
        audio = PCMAudioOutput { out, frames in vnns_audio_render(box.engine, out, Int32(frames)) }
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
        if !vnns_step(engine, Int32(min(max(ms, 0), 100))) {
            finish(byGame: true)
            return
        }
        var w: Int32 = 0, h: Int32 = 0
        var changed = false
        if let pixels = vnns_frame(engine, &w, &h, &changed), changed {
            frameView.present(pixels, width: Int(w), height: Int(h))
        }
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

    private func finish(byGame: Bool) {
        displayLink?.invalidate()
        displayLink = nil
        audio?.stop()
        audio = nil
        if let engine {
            vnns_stop(engine)
            self.engine = nil
        }
        stopped = true
        let waiters = stopWaiters
        stopWaiters = []
        waiters.forEach { $0.resume() }
        if byGame { launch.onExit() }
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
        self.paused = paused
        guard let engine else { return }
        if paused { deliver(router.releaseAll()) }
        vnns_set_paused(engine, paused)
        if paused { audio?.pause() } else { audio?.resume() }
        lastTimestamp = nil
        if !paused { frameView.takeFocus() }
    }

    func terminate() {
        guard !stopped else { return }
        if let engine {
            vnns_request_quit(engine)
            vnns_step(engine, 0)
        }
        finish(byGame: false)
    }

    func waitUntilStopped() async {
        if stopped { return }
        await withCheckedContinuation { stopWaiters.append($0) }
    }
}

/// The engine pointer, handed to the audio thread. vnns_audio_render is safe to call there.
private final class EngineBox: @unchecked Sendable {
    let engine: OpaquePointer
    init(_ engine: OpaquePointer) { self.engine = engine }
}

/// A display link keeps its target, so the session sits behind a weak reference to avoid a cycle.
private final class FrameTarget: NSObject {
    private weak var session: NScripterSession?
    init(_ session: NScripterSession) { self.session = session }
    @objc func step(_ link: CADisplayLink) { session?.step(link) }
}
