import CWolfRPG
import FontPack
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
    #if DEBUG
    private let pacing = UserDefaults.standard.bool(forKey: "frameMonitor") ? FramePacing() : nil
    #endif
    private var router = WolfRPGInputRouter()
    private var wantedText = false
    private var scrolledNotches = 0.0
    private var paused = false
    private var stopped = false
    private var stopWaiters: [CheckedContinuation<Void, Never>] = []
    private lazy var wolfCheats = WolfRPGCheats { [weak self] in self?.engine }
    var cheats: (any CheatSession)? { wolfCheats }

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
            // A trackpad reports fractions of a notch, which add up over a slow scroll.
            scrolledNotches += notches
            let whole = Int(scrolledNotches)
            guard whole != 0 else { return }
            scrolledNotches -= Double(whole)
            deliver([.wheel(whole)])
        }
        frameView.onKey = { [weak self] usage, pressed, characters in
            guard let self else { return }
            deliver(router.hostKey(hidUsage: usage, pressed: pressed))
            if pressed, let characters, !characters.isEmpty, characters.unicodeScalars.allSatisfy({ $0.value >= 0x20 && $0.value < 0xF700 }), let engine {
                vnwolf_text(engine, characters)
            }
        }
    }

    private func start() {
        guard engine == nil, !stopped else { return }
        let logURL = launch.dataDirectory.appendingPathComponent("logs/wolfrpg.txt")
        try? FileManager.default.createDirectory(at: logURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        try? Data().write(to: logURL)
        // MIDI music plays with the player's SoundFont from Settings, or the one the app bundles (docs/midi.md).
        let soundFont = MIDISoundFont.url?.path ?? ""
        let fallback = MIDISoundFont.fallbackURL?.path ?? ""
        let started: OpaquePointer? = root.path.withCString { rootPath in
            logURL.path.withCString { logPath in
                soundFont.withCString { fontPath in
                    fallback.withCString { fallbackPath in
                        var settings = VNWolfLaunch(gameRoot: rootPath, logPath: logPath,
                                                    soundFontPath: soundFont.isEmpty ? nil : fontPath,
                                                    fallbackSoundFontPath: fallback.isEmpty ? nil : fallbackPath)
                        return vnwolf_start(&settings)
                    }
                }
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
        wolfCheats.apply(launch.game.cheats ?? CheatSettings())
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
        // The game asks for typed text: the hub opens its keyboard for as long as the game waits.
        let wantsText = vnwolf_wants_text(engine)
        if wantsText != wantedText {
            wantedText = wantsText
            NotificationCenter.default.post(name: .gameWantsKeyboard, object: self, userInfo: ["wants": wantsText])
        }
        var w: Int32 = 0, h: Int32 = 0
        var changed = false
        if let pixels = vnwolf_frame(engine, &w, &h, &changed), changed {
            frameView.present(pixels, width: Int(w), height: Int(h))
        }
        #if DEBUG
        pacing?.note(tick: now, presented: changed, stepEnd: CACurrentMediaTime(), target: link.targetTimestamp - link.timestamp)
        #endif
    }

    private func deliver(_ inputs: [WolfRPGInput]) {
        guard let engine else { return }
        for input in inputs {
            switch input {
            case let .key(code, down): vnwolf_key(engine, code, down)
            case let .mouseButton(button, down): vnwolf_mouse_button(engine, button, down)
            case let .mouseMove(point): vnwolf_mouse_move(engine, Float(point.x), Float(point.y))
            case let .wheel(notches): vnwolf_mouse_wheel(engine, Int32(notches))
            }
        }
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
        // A controller's Confirm and Cancel finish the game's text entry, since neither types anything.
        if let engine, vnwolf_wants_text(engine) {
            if case let .button(button, true) = event {
                if button == .confirm { deliver(router.tap(Int32(VNWOLF_KEY_RETURN))) }
                if button == .cancel { deliver(router.tap(Int32(VNWOLF_KEY_ESCAPE))) }
            }
        }
        let frame = frameView
        deliver(router.translate(event) { frame.framePoint(fromViewNormalized: $0) })
    }

    func type(_ typed: TypedText) {
        guard let engine else { return }
        switch typed {
        case let .text(text): vnwolf_text(engine, text)
        case .newline: deliver(router.tap(Int32(VNWOLF_KEY_RETURN)))
        case .deleteBackward: deliver(router.tap(Int32(VNWOLF_KEY_BACKSPACE)))
        }
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

#if DEBUG
/// With `-frameMonitor YES`, logs the runtime's pacing every 10 seconds ("WOLFFRAMES" in the app log): display link
/// ticks per second on average and over the slowest 1% of ticks, frames the runtime drew, and the slowest step.
private final class FramePacing {
    private var last: CFTimeInterval?
    private var start: CFTimeInterval = 0
    private var intervals: [Double] = []
    private var presented = 0
    private var worstStep = 0.0

    func note(tick: CFTimeInterval, presented drew: Bool, stepEnd: CFTimeInterval, target: CFTimeInterval) {
        defer { last = tick }
        guard let last else {
            start = tick
            return
        }
        intervals.append(tick - last)
        if drew { presented += 1 }
        worstStep = max(worstStep, stepEnd - tick)
        guard tick - start >= 10 else { return }
        let slowest = intervals.sorted(by: >).prefix(max(1, intervals.count / 100))
        let low = Double(slowest.count) / slowest.reduce(0, +)
        NSLog("WOLFFRAMES seconds=%.1f average=%.1f low1=%.1f drawn=%.1f worstStep=%.1fms target=%.1f", tick - start,
              Double(intervals.count) / (tick - start), low, Double(presented) / (tick - start), worstStep * 1000, 1 / target)
        intervals.removeAll(keepingCapacity: true)
        presented = 0
        worstStep = 0
        start = tick
    }
}
#endif
