import CRGSS
import FontPack
import FrameHost
import Foundation
import OSLog
import QuartzCore
import VNCore

nonisolated let rgssLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "rgss")

/// An RGSS game running on VNDegen's runtime in this process. The game's scripts run on the runtime's own thread; each
/// display refresh lets it run for the time that passed and shows its latest frame in `FrameView`. Its sound plays
/// through `PCMAudioOutput`. The game ending itself, by its own quit or an error, ends the session through `onExit`.
/// RGSS games save only when the player saves, so the hub ends a game by stopping it.
final class RGSSSession: EngineSession {
    let view: PlatformView
    private let frameView = FrameView()
    private let launch: GameLaunch
    private let paths: RGSSPaths
    private var running = false
    private var stopped = false
    private var paused = false
    private var audio: PCMAudioOutput?
    private var router = RGSSInputRouter()
    /// Hardware keys down, to tell a key's repeats from its first press.
    private var keyboardHeld: Set<UInt32> = []
    private var displayLink: CADisplayLink?
    private var lastFrame: CFTimeInterval?
    #if DEBUG
    private let pacing = UserDefaults.standard.bool(forKey: "frameMonitor") ? FramePacing() : nil
    #endif
    /// When each key the game holds went down, and the releases held back for `shortestPress`.
    private var pressedAt: [UInt32: ContinuousClock.Instant] = [:]
    private var heldReleases: [UInt32: Task<Void, Never>] = [:]

    /// RGSS reads its input once a frame. The runtime keeps a key pressed and released between two frames down for one
    /// frame, but some games check a key on two frames in a row, so a quick tap of a touch or controller button is held
    /// at least this long, three frames at 60 a second.
    static let shortestPress: Duration = .milliseconds(50)

    init(_ launch: GameLaunch, paths: RGSSPaths) throws {
        self.launch = launch
        self.paths = paths
        try paths.prepare()
        view = frameView
        frameView.filter = .nearest
        frameView.onReady = { [weak self] in self?.start() }
        frameView.onKey = { [weak self] usage, pressed, _ in
            guard let self, running, let key = RetroKey.hidUsage(usage) else { return }
            // A key held down repeats on macOS; RGSS repeats held keys itself.
            let isRepeat = pressed && !keyboardHeld.insert(key).inserted
            if !pressed { keyboardHeld.remove(key) }
            router.hostKey(key, pressed: pressed, isRepeat: isRepeat).forEach(deliver)
        }
        frameView.onPointer = { [weak self] event in
            guard let self, running else { return }
            router.pointer(event, at: event.location).forEach(deliver)
        }
        frameView.onScroll = { [weak self] notches in
            guard let self, running else { return }
            deliver(.wheel(notches))
        }
    }

    private func start() {
        guard !running, !stopped else { return }
        // MIDI music plays with the player's SoundFont from Settings, or the one the app bundles (docs/midi.md).
        let soundFont = MIDISoundFont.url?.path
        let fallback = MIDISoundFont.fallbackURL?.path
        let strings = [paths.gameIni.path, paths.saves.path, paths.log.path, soundFont, fallback].map { $0.flatMap { strdup($0) } }
        let rtps = paths.runtimes.map { strdup($0.path) }
        defer {
            strings.forEach { free($0) }
            rtps.forEach { free($0) }
        }
        let result = rtps.map { UnsafePointer($0) }.withUnsafeBufferPointer { rtpList in
            var settings = VNRGSSLaunch(gameIni: UnsafePointer(strings[0]), rtps: rtpList.baseAddress,
                                        rtpCount: Int32(rtpList.count), saveDir: UnsafePointer(strings[1]),
                                        logPath: UnsafePointer(strings[2]), soundFont: UnsafePointer(strings[3]),
                                        fallbackSoundFont: UnsafePointer(strings[4]))
            return vnrgss_start(&settings)
        }
        guard result == 0 else {
            rgssLog.error("\(self.launch.game.id.rawValue, privacy: .public) did not start: \(Self.problem ?? "", privacy: .public)")
            running = true
            finish(byGame: true)
            return
        }
        rgssLog.info("started \(self.launch.game.id.rawValue, privacy: .public)")
        running = true
        audio = PCMAudioOutput { out, frames in vnrgss_audio_render(out, Int32(frames)) }
        audio?.start()
        if paused { setPaused(true) }
        frameView.takeFocus()
        let target = FrameTarget(self)
        #if canImport(UIKit)
        let link = CADisplayLink(target: target, selector: #selector(FrameTarget.step))
        #else
        let link = frameView.displayLink(target: target, selector: #selector(FrameTarget.step))
        #endif
        link.add(to: .main, forMode: .common)
        displayLink = link
    }

    fileprivate func step(_ link: CADisplayLink) {
        guard running else { return }
        let now = link.timestamp
        let seconds = lastFrame.map { now - $0 } ?? 1.0 / 60
        lastFrame = now
        if vnrgss_step(seconds) != 0 {
            finish(byGame: true)
            return
        }
        var width: Int32 = 0, height: Int32 = 0
        var changed = false
        if let pixels = vnrgss_frame(&width, &height, &changed), changed {
            frameView.present(pixels, width: Int(width), height: Int(height))
        }
        #if DEBUG
        pacing?.note(tick: now, presented: changed, stepEnd: CACurrentMediaTime())
        #endif
    }

    private static var problem: String? { vnrgss_problem().map { String(cString: $0) } }

    /// Ends the session, tearing the runtime down. When the game ended itself, the hub returns to the library.
    private func finish(byGame: Bool) {
        guard running else { return }
        running = false
        stopped = true
        displayLink?.invalidate()
        displayLink = nil
        heldReleases.values.forEach { $0.cancel() }
        heldReleases = [:]
        vnrgss_stop()
        audio?.stop()
        audio = nil
        let problem = byGame ? Self.problem : nil
        RGSSPlugin.record(problem: problem, dataDirectory: launch.dataDirectory)
        rgssLog.info("\(self.launch.game.id.rawValue, privacy: .public) stopped\(problem.map { ": \($0)" } ?? "", privacy: .public)")
        if byGame { launch.onExit() }
    }

    private func deliver(_ input: RGSSInput) {
        guard running else { return }
        guard case let .key(key, down) = input else { return send(input) }
        if down {
            // Pressed again before its held-back release went out, so the game still has the key down.
            if let release = heldReleases.removeValue(forKey: key) { return release.cancel() }
            pressedAt[key] = .now
            return send(input)
        }
        let wait = pressedAt[key].map { $0 + Self.shortestPress - .now } ?? .zero
        guard wait > .zero else {
            pressedAt[key] = nil
            return send(input)
        }
        heldReleases[key] = Task { [weak self] in
            try? await Task.sleep(for: wait)
            guard !Task.isCancelled, let self else { return }
            heldReleases[key] = nil
            pressedAt[key] = nil
            send(input)
        }
    }

    private func send(_ input: RGSSInput) {
        guard running else { return }
        switch input {
        case let .key(code, down): vnrgss_key(code, down)
        case let .mouseMove(to): vnrgss_mouse_move(Float(to.x), Float(to.y))
        case let .mouseButton(button, down): vnrgss_mouse_button(button, down)
        case let .wheel(notches): vnrgss_wheel(Float(notches))
        }
    }

    // MARK: EngineSession

    /// Controller and touch gamepad buttons arrive here, and so do the hub's touch mouse and keyboard.
    func send(_ event: ControllerEvent) {
        let frame = frameView
        router.translate(event) { frame.framePoint(fromViewNormalized: $0) }.forEach(deliver)
    }

    func setPaused(_ paused: Bool) {
        self.paused = paused
        guard running else { return }
        // The hub's menu takes the keyboard, so the game would never see those keys come up.
        if paused {
            router.releaseAll().forEach(deliver)
            keyboardHeld = []
        }
        vnrgss_set_paused(paused)
        if paused { audio?.pause() } else { audio?.resume() }
        lastFrame = nil
        if !paused { frameView.takeFocus() }
    }

    func terminate() {
        if running {
            finish(byGame: false)
            RGSSPlugin.record(problem: nil, dataDirectory: launch.dataDirectory)
        }
        stopped = true
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

#if DEBUG
/// With `-frameMonitor YES`, logs the game's frame rate every 10 seconds ("RGSSFRAMES" in the app log): frames the
/// game drew per second on average and over the slowest 1% of them, display link ticks per second, and the slowest step.
private final class FramePacing {
    private var start: CFTimeInterval?
    private var lastPresented: CFTimeInterval?
    private var gaps: [Double] = []
    private var ticks = 0
    private var worstStep = 0.0

    func note(tick: CFTimeInterval, presented: Bool, stepEnd: CFTimeInterval) {
        guard let start else {
            self.start = tick
            return
        }
        ticks += 1
        worstStep = max(worstStep, stepEnd - tick)
        if presented {
            if let lastPresented { gaps.append(tick - lastPresented) }
            lastPresented = tick
        }
        let elapsed = tick - start
        guard elapsed >= 10 else { return }
        let slowest = gaps.sorted(by: >).prefix(max(1, gaps.count / 100))
        let low = slowest.isEmpty ? 0 : Double(slowest.count) / slowest.reduce(0, +)
        NSLog("RGSSFRAMES seconds=%.1f average=%.1f low1=%.1f ticks=%.1f worstStep=%.1fms target=%d", elapsed,
              Double(gaps.count) / elapsed, low, Double(ticks) / elapsed, worstStep * 1000, Int(vnrgss_frame_rate()))
        self.start = tick
        gaps.removeAll(keepingCapacity: true)
        ticks = 0
        worstStep = 0
    }
}
#endif
