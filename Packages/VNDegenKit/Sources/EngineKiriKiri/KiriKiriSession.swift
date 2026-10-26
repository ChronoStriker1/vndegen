import CKiriKiri
import Foundation
import OSLog
import QuartzCore
import VNCore
#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

nonisolated let kiriKiriLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "kirikiri")

/// A KiriKiri game running in VNDegen's own process and drawn in `view`, so the hub's menu and touch controls sit over
/// it as they do over a web game. The engine runs one frame per display refresh on the main thread. The game starts
/// once the view is on screen, at the size the game will fill. The game quitting itself ends the session through
/// `onExit`. When the hub ends the session instead, the game closes through its normal path, which writes its system
/// saves, and `waitUntilStopped()` returns once it has.
final class KiriKiriSession: EngineSession {
    private enum State {
        /// Waiting for the view to be on screen.
        case waiting
        case running
        /// The hub ended the session and the game is closing, until `deadline` at most.
        case quitting(deadline: ContinuousClock.Instant)
        case stopped
    }

    let view: PlatformView
    private let host: KiriKiriHostView
    private let engine: KiriKiriEngine
    private let launch: GameLaunch
    private let paths: KiriKiriPaths
    private var state = State.waiting
    /// A native modal loop inside a frame, such as a game's own dialog, can fire the display link or the quit timer
    /// again before the frame returns. The engine ignores a nested frame, and nothing may stop it under the running one.
    private var inFrame = false
    private var paused = false
    private var router = KiriKiriInputRouter()
    private var displayLink: CADisplayLink?
    private var quitTimer: Timer?
    private var stopWaiters: [CheckedContinuation<Void, Never>] = []
    /// When each key the game holds went down, and the releases held back for `shortestPress`.
    private var pressedAt: [Int32: ContinuousClock.Instant] = [:]
    private var heldReleases: [Int32: Task<Void, Never>] = [:]

    /// Longest the game gets to close after the hub ends it. The engine itself ends a game after 3 seconds when the
    /// game is still waiting on a quit dialog of its own.
    static let quitTimeout: Duration = .seconds(8)
    /// KAG handles a key on a later frame and ignores it unless the key is still down then, so a quick tap of a touch
    /// or controller button would be lost. A release sooner than this after its press waits until this has passed.
    static let shortestPress: Duration = .milliseconds(50)

    init(_ launch: GameLaunch, layout: KiriKiriLayout, engine: KiriKiriEngine) throws {
        self.launch = launch
        self.engine = engine
        paths = KiriKiriPaths(folder: launch.folder, dataDirectory: launch.dataDirectory, layout: layout)
        try paths.prepare()
        host = KiriKiriHostView()
        view = host
        host.onReady = { [weak self] in self?.start() }
        host.onKey = { [weak self] scancode, pressed, isRepeat in
            guard let self, case .running = state else { return }
            router.hostKey(scancode, pressed: pressed, isRepeat: isRepeat).forEach(deliver)
        }
        host.onEvent = { [weak self] event in self?.send(event) }
    }

    private func start() {
        guard case .waiting = state else { return }
        let strings = [paths.gameRoot.path, paths.projectData, paths.log.path, paths.appData.path, paths.documents.path].map { strdup($0) }
        defer { strings.forEach { free($0) } }
        #if canImport(UIKit)
        let hostView: UnsafeMutableRawPointer? = nil
        #else
        let hostView: UnsafeMutableRawPointer? = Unmanaged.passUnretained(host).toOpaque()
        #endif
        var settings = VNKKLaunch(gameRoot: strings[0], projectData: strings[1], logPath: strings[2], appDataDir: strings[3],
                                  personalDir: strings[4], hostView: hostView)
        guard engine.start(&settings) == 0 else {
            kiriKiriLog.error("\(self.launch.game.id.rawValue, privacy: .public) did not start, see \(self.paths.log.path, privacy: .public)")
            engine.stop()
            stopped()
            launch.onExit()
            return
        }
        kiriKiriLog.info("started \(self.launch.game.id.rawValue, privacy: .public)")
        state = .running
        #if canImport(UIKit)
        if let sdlView = engine.view() { host.embed(Unmanaged<UIView>.fromOpaque(sdlView).takeUnretainedValue()) }
        #endif
        if paused { engine.setPaused(true) }
        host.takeFocus()
        let target = FrameTarget(self)
        #if canImport(UIKit)
        let link = CADisplayLink(target: target, selector: #selector(FrameTarget.step))
        #else
        let link = host.displayLink(target: target, selector: #selector(FrameTarget.step))
        #endif
        link.add(to: .main, forMode: .common)
        displayLink = link
    }

    func step() {
        guard !inFrame else { return }
        inFrame = true
        defer { inFrame = false }
        switch state {
        case .running:
            if engine.iterate() != 0 { finish() }
        case let .quitting(deadline):
            if engine.iterate() != 0 || ContinuousClock.now > deadline { finish() }
        case .waiting, .stopped:
            break
        }
    }

    private func finish() {
        let quitByGame = if case .running = state { true } else { false }
        displayLink?.invalidate()
        displayLink = nil
        quitTimer?.invalidate()
        quitTimer = nil
        engine.stop()
        kiriKiriLog.info("\(self.launch.game.id.rawValue, privacy: .public) stopped\(quitByGame ? " by the game" : "", privacy: .public)")
        stopped()
        if quitByGame { launch.onExit() }
    }

    private func stopped() {
        state = .stopped
        let waiters = stopWaiters
        stopWaiters = []
        waiters.forEach { $0.resume() }
    }

    private func deliver(_ input: KiriKiriInput) {
        guard case let .key(scancode, down) = input else { return engine.send(input) }
        if down {
            // Pressed again before its held-back release went out, so the game still has the key down.
            if let release = heldReleases.removeValue(forKey: scancode) { return release.cancel() }
            pressedAt[scancode] = .now
            return engine.send(input)
        }
        let wait = pressedAt[scancode].map { $0 + Self.shortestPress - .now } ?? .zero
        guard wait > .zero else {
            pressedAt[scancode] = nil
            return engine.send(input)
        }
        heldReleases[scancode] = Task { [weak self] in
            try? await Task.sleep(for: wait)
            guard !Task.isCancelled, let self else { return }
            heldReleases[scancode] = nil
            pressedAt[scancode] = nil
            switch state {
            case .running, .quitting: engine.send(input)
            case .waiting, .stopped: break
            }
        }
    }

    // MARK: EngineSession

    /// Touches on iOS, and the mouse and keyboard on macOS, reach the game through the view itself. Controller buttons
    /// and the touch mouse arrive here.
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
        if !paused { host.takeFocus() }
    }

    func terminate() {
        switch state {
        case .waiting:
            stopped()
        case .running:
            engine.requestQuit()
            state = .quitting(deadline: .now + Self.quitTimeout)
            // The hub takes the view off screen, which would stop a display link tied to it. The timer keeps the
            // session alive until the game has closed.
            displayLink?.invalidate()
            displayLink = nil
            let timer = Timer(timeInterval: 1.0 / 60, repeats: true) { _ in
                MainActor.assumeIsolated { self.step() }
            }
            RunLoop.main.add(timer, forMode: .common)
            quitTimer = timer
        case .quitting, .stopped:
            break
        }
    }

    func waitUntilStopped() async {
        if case .stopped = state { return }
        await withCheckedContinuation { stopWaiters.append($0) }
    }
}

/// A display link keeps its target, so the session sits behind a weak link to avoid a cycle.
private final class FrameTarget: NSObject {
    private weak var session: KiriKiriSession?

    init(_ session: KiriKiriSession) {
        self.session = session
    }

    @objc func step() { session?.step() }
}

/// The files a launch hands the engine. The game's data directory holds the engine's log and stands in for the
/// Windows AppData and Documents folders.
nonisolated struct KiriKiriPaths {
    var gameRoot: URL
    /// The project as the engine takes it. Kirikiri tells a folder from an archive by its closing slash.
    var projectData: String
    var log: URL
    var appData: URL
    var documents: URL

    init(folder: URL, dataDirectory: URL, layout: KiriKiriLayout) {
        gameRoot = folder.appendingPathComponent(layout.root, isDirectory: true)
        switch layout.data {
        case let .archive(path), let .embedded(path): projectData = folder.appendingPathComponent(path).path
        case let .folder(path): projectData = folder.appendingPathComponent(path, isDirectory: true).path + "/"
        }
        log = dataDirectory.appendingPathComponent("logs/kirikiri.txt")
        appData = KiriKiriPlugin.appDataDirectory(dataDirectory)
        documents = KiriKiriPlugin.documentsDirectory(dataDirectory)
    }

    /// Creates the folders and starts a fresh log for this launch.
    func prepare() throws {
        let fileManager = FileManager.default
        for folder in [log.deletingLastPathComponent(), appData, documents] {
            try fileManager.createDirectory(at: folder, withIntermediateDirectories: true)
        }
        try Data().write(to: log)
    }
}
