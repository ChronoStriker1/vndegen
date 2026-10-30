import CWolfRPG
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

nonisolated let wolfRPGLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "wolfrpg")

/// A Wolf RPG Editor game running on VNDegen's runtime in this process. The runtime steps once per display refresh on
/// the main thread and draws into a CPU frame that `FrameView` shows; its sound plays through `PCMAudioOutput`. The
/// game ending itself ends the session through `onExit`.
final class WolfRPGSession: EngineSession {
    let view: PlatformView
    private let frameView = FrameView()
    private let launch: GameLaunch
    private let root: URL
    private var engine: OpaquePointer?
    private var audio: PCMAudioOutput?
    private var displayLink: CADisplayLink?
    private var lastTimestamp: CFTimeInterval?
    private var router = WolfRPGInputRouter()
    private var paused = false
    private var stopped = false
    private var stopWaiters: [CheckedContinuation<Void, Never>] = []

    init(_ launch: GameLaunch, root: URL) {
        self.launch = launch
        self.root = root
        view = frameView
        frameView.onReady = { [weak self] in self?.start() }
        frameView.onKey = { [weak self] usage, pressed, _ in
            guard let self else { return }
            deliver(router.hostKey(hidUsage: usage, pressed: pressed))
        }
    }

    private func start() {
        guard engine == nil, !stopped else { return }
        let logURL = launch.dataDirectory.appendingPathComponent("logs/wolfrpg.txt")
        try? FileManager.default.createDirectory(at: logURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        try? Data().write(to: logURL)
        let started: OpaquePointer? = root.path.withCString { rootPath in
            logURL.path.withCString { logPath in
                var settings = VNWolfLaunch(gameRoot: rootPath, logPath: logPath, soundFontPath: nil)
                return vnwolf_start(&settings)
            }
        }
        guard let started else {
            let reason = vnwolf_start_error().map { String(cString: $0) } ?? "unknown"
            wolfRPGLog.error("\(self.launch.game.id.rawValue, privacy: .public) did not start: \(reason, privacy: .public)")
            finish(byGame: true)
            return
        }
        engine = started
        wolfRPGLog.info("started \(self.launch.game.id.rawValue, privacy: .public)")
        let box = EngineBox(started)
        audio = PCMAudioOutput { out, frames in vnwolf_audio_render(box.engine, out, Int32(frames)) }
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
        if !vnwolf_step(engine, Int32(min(max(ms, 0), 100))) {
            finish(byGame: true)
            return
        }
        var w: Int32 = 0, h: Int32 = 0
        var changed = false
        if let pixels = vnwolf_frame(engine, &w, &h, &changed), changed {
            frameView.present(pixels, width: Int(w), height: Int(h))
        }
    }

    private func deliver(_ keys: [WolfRPGKey]) {
        guard let engine else { return }
        for key in keys { vnwolf_key(engine, key.code, key.down) }
    }

    private func finish(byGame: Bool) {
        displayLink?.invalidate()
        displayLink = nil
        audio?.stop()
        audio = nil
        if let engine {
            if let problem = vnwolf_problem(engine) {
                wolfRPGLog.error("\(self.launch.game.id.rawValue, privacy: .public) stopped: \(String(cString: problem), privacy: .public)")
            }
            vnwolf_stop(engine)
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
        deliver(router.translate(event))
    }

    func setPaused(_ paused: Bool) {
        self.paused = paused
        guard let engine else { return }
        if paused { deliver(router.releaseAll()) }
        vnwolf_set_paused(engine, paused)
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

/// The engine pointer, handed to the audio thread. vnwolf_audio_render is safe to call there.
nonisolated private final class EngineBox: @unchecked Sendable {
    let engine: OpaquePointer
    init(_ engine: OpaquePointer) { self.engine = engine }
}

/// A display link keeps its target, so the session sits behind a weak reference to avoid a cycle.
private final class FrameTarget: NSObject {
    private weak var session: WolfRPGSession?
    init(_ session: WolfRPGSession) { self.session = session }
    @objc func step(_ link: CADisplayLink) { session?.step(link) }
}
