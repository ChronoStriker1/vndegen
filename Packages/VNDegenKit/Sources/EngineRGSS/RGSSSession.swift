import CRGSS
import Foundation
import OSLog
import QuartzCore
import VNCore
#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

nonisolated let rgssLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "rgss")

/// An RGSS game running in VNDegen's own process and drawn in `view`, so the hub's menu and touch controls sit over it
/// as they do over a web game. The engine runs one frame per display refresh, at most 60 a second, on the main thread.
/// The game starts once the view is on screen. The game ending itself, by its own quit or an error, ends the session
/// through `onExit`. RGSS games save only when the player saves, so the hub ends a game by stopping it.
final class RGSSSession: EngineSession {
    private enum State {
        /// Waiting for the view to be on screen.
        case waiting
        case running
        case stopped
    }

    let view: PlatformView
    private let host: RGSSHostView
    private let engine: RGSSEngine
    private let launch: GameLaunch
    private let paths: RGSSPaths
    private var state = State.waiting
    private var paused = false
    private var router = RGSSInputRouter()
    private var displayLink: CADisplayLink?
    private var lastFrame: CFTimeInterval?
    /// When each key the game holds went down, and the releases held back for `shortestPress`.
    private var pressedAt: [UInt32: ContinuousClock.Instant] = [:]
    private var heldReleases: [UInt32: Task<Void, Never>] = [:]

    /// RGSS reads its input once a frame and ignores a key unless it is down then, so a quick tap of a touch or
    /// controller button, pressed and released between two frames, would be lost. A release sooner than this after its
    /// press waits until this has passed, three frames at 60 a second.
    static let shortestPress: Duration = .milliseconds(50)

    init(_ launch: GameLaunch, paths: RGSSPaths, engine: RGSSEngine) throws {
        self.launch = launch
        self.engine = engine
        self.paths = paths
        try paths.prepare()
        host = RGSSHostView()
        view = host
        host.onReady = { [weak self] in self?.start() }
        host.onKey = { [weak self] key, pressed, isRepeat in
            guard let self, case .running = state else { return }
            router.hostKey(key, pressed: pressed, isRepeat: isRepeat).forEach(deliver)
        }
        #if canImport(AppKit)
        host.onEvent = { [weak self] event in self?.send(event) }
        #endif
    }

    private func start() {
        guard case .waiting = state else { return }
        let strings = [paths.gameIni.path, paths.system.path, paths.saves.path, paths.log.path].map { strdup($0) }
        defer { strings.forEach { free($0) } }
        #if canImport(UIKit)
        let layer = host.layer
        #else
        guard let layer = host.layer else { return }
        #endif
        var settings = VNRGSSLaunch(gameIni: strings[0], systemDir: strings[1], saveDir: strings[2], logPath: strings[3],
                                    layer: Unmanaged.passUnretained(layer).toOpaque())
        guard engine.start(&settings) == 0 else {
            rgssLog.error("\(self.launch.game.id.rawValue, privacy: .public) did not start: \(self.engine.lastProblem ?? "", privacy: .public)")
            finish()
            return
        }
        rgssLog.info("started \(self.launch.game.id.rawValue, privacy: .public)")
        state = .running
        if paused { engine.setPaused(true) }
        host.takeFocus()
        let target = FrameTarget(self)
        #if canImport(UIKit)
        let link = CADisplayLink(target: target, selector: #selector(FrameTarget.step))
        #else
        let link = host.displayLink(target: target, selector: #selector(FrameTarget.step))
        #endif
        // RGSS games run at 60 frames a second at most, and the core plays one frame's worth of audio per frame.
        link.preferredFrameRateRange = CAFrameRateRange(minimum: 30, maximum: 60, preferred: 60)
        link.add(to: .main, forMode: .common)
        displayLink = link
    }

    fileprivate func step(_ link: CADisplayLink) {
        guard case .running = state else { return }
        let now = link.timestamp
        let seconds = lastFrame.map { now - $0 } ?? 1.0 / 60
        lastFrame = now
        if engine.frame(seconds) != 0 { finish() }
    }

    /// Ends the session, tearing the engine down. When the game ended itself, the hub returns to the library.
    private func finish() {
        let endedByGame = if case .stopped = state { false } else { true }
        displayLink?.invalidate()
        displayLink = nil
        engine.stop()
        let problem = engine.lastProblem
        RGSSPlugin.record(problem: problem, dataDirectory: launch.dataDirectory)
        rgssLog.info("\(self.launch.game.id.rawValue, privacy: .public) stopped\(problem.map { ": \($0)" } ?? "", privacy: .public)")
        state = .stopped
        if endedByGame { launch.onExit() }
    }

    private func deliver(_ input: RGSSInput) {
        guard case let .key(key, down) = input else { return engine.send(input) }
        if down {
            // Pressed again before its held-back release went out, so the game still has the key down.
            if let release = heldReleases.removeValue(forKey: key) { return release.cancel() }
            pressedAt[key] = .now
            return engine.send(input)
        }
        let wait = pressedAt[key].map { $0 + Self.shortestPress - .now } ?? .zero
        guard wait > .zero else {
            pressedAt[key] = nil
            return engine.send(input)
        }
        heldReleases[key] = Task { [weak self] in
            try? await Task.sleep(for: wait)
            guard !Task.isCancelled, let self else { return }
            heldReleases[key] = nil
            pressedAt[key] = nil
            if case .running = state { engine.send(input) }
        }
    }

    // MARK: EngineSession

    /// Touches reach the game through the hub's touch controls, and on macOS the mouse arrives from the view. Controller
    /// buttons arrive here.
    func send(_ event: ControllerEvent) {
        guard case .running = state else { return }
        router.translate(event).forEach(deliver)
    }

    func setPaused(_ paused: Bool) {
        self.paused = paused
        guard case .running = state else { return }
        // The hub's menu takes the keyboard, so the game would never see those keys come up.
        if paused { router.releaseAll().forEach(deliver) }
        engine.setPaused(paused)
        lastFrame = nil
        if !paused { host.takeFocus() }
    }

    func terminate() {
        switch state {
        case .waiting:
            state = .stopped
        case .running:
            state = .stopped
            displayLink?.invalidate()
            displayLink = nil
            engine.stop()
            RGSSPlugin.record(problem: nil, dataDirectory: launch.dataDirectory)
            rgssLog.info("\(self.launch.game.id.rawValue, privacy: .public) stopped by the hub")
        case .stopped:
            break
        }
    }
}

/// A display link keeps its target, so the session sits behind a weak link to avoid a cycle.
private final class FrameTarget: NSObject {
    private weak var session: RGSSSession?

    init(_ session: RGSSSession) {
        self.session = session
    }

    @objc func step(_ link: CADisplayLink) { session?.step(link) }
}
