import CSystem3
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

nonisolated let system3Log = Logger(subsystem: "com.chronostriker1.vndegen", category: "system3")

/// A System 3 game running on VNDegen's runtime (CSystem3) in this process. The runtime steps once per display refresh
/// on the main thread and draws into a CPU frame that `FrameView` shows; its sound plays through `PCMAudioOutput`.
final class System3Session: EngineSession {
    let view: PlatformView
    private let frameView = FrameView()
    private let launch: GameLaunch
    private let root: URL
    private var engine: OpaquePointer?
    private var audio: PCMAudioOutput?
    private var displayLink: CADisplayLink?
    private var lastTimestamp: CFTimeInterval?
    private var router = System3InputRouter()
    private var frameSize = CGSize(width: 640, height: 480)
    private var shownSerial: UInt32 = .max
    private var paused = false
    private var stopped = false
    private var stopWaiters: [CheckedContinuation<Void, Never>] = []

    init(_ launch: GameLaunch, root: URL) {
        self.launch = launch
        self.root = root
        view = frameView
        frameView.filter = .nearest
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
        frameView.onKey = { [weak self] usage, pressed, _ in
            guard let self else { return }
            deliver(router.hostKey(hidUsage: usage, pressed: pressed))
        }
    }

    private func start() {
        guard engine == nil, !stopped else { return }
        let logURL = launch.dataDirectory.appendingPathComponent("logs/system3.txt")
        try? FileManager.default.createDirectory(at: logURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        try? Data().write(to: logURL)
        // MIDI music plays with the player's SoundFont from Settings, or the one the app bundles (docs/midi.md).
        let soundFont = MIDISoundFont.url?.path ?? ""
        let fallback = MIDISoundFont.fallbackURL?.path ?? ""
        let rate = 44100.0
        var problem = [CChar](repeating: 0, count: 512)
        let started: OpaquePointer? = root.path.withCString { rootPath in
            logURL.path.withCString { logPath in
                soundFont.withCString { fontPath in
                    fallback.withCString { fallbackPath in
                        var config = VNS3Config()
                        config.gameDir = rootPath
                        config.saveDir = rootPath
                        config.logPath = logPath
                        config.audioRate = Int32(rate)
                        config.soundFontPath = soundFont.isEmpty ? nil : fontPath
                        config.fallbackSoundFontPath = fallback.isEmpty ? nil : fallbackPath
                        return vns3_open(&config, &problem, problem.count)
                    }
                }
            }
        }
        guard let started else {
            let reason = String(decoding: problem.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self)
            system3Log.error("\(self.launch.game.id.rawValue, privacy: .public) did not start: \(reason, privacy: .public)")
            finish(byGame: true)
            return
        }
        engine = started
        system3Log.info("started \(self.launch.game.id.rawValue, privacy: .public)")
        let box = EngineBox(started)
        audio = PCMAudioOutput(sampleRate: rate) { out, frames in vns3_audio(box.engine, out, Int32(frames)) }
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
        if !vns3_frame(engine, UInt32(min(max(ms, 0), 100))) {
            finish(byGame: true)
            return
        }
        let serial = vns3_frame_serial(engine)
        guard serial != shownSerial else { return }
        shownSerial = serial
        var w: Int32 = 0, h: Int32 = 0
        if let pixels = vns3_pixels(engine, &w, &h), w > 0, h > 0 {
            frameSize = CGSize(width: Int(w), height: Int(h))
            frameView.present(pixels, width: Int(w), height: Int(h))
        }
    }

    private func deliver(_ inputs: [System3Input]) {
        guard let engine else { return }
        for input in inputs {
            switch input {
            case let .key(code, down): vns3_key(engine, code, down)
            case let .mouseButton(button, down): vns3_mouse_button(engine, button, down)
            case let .mouseMove(p):
                vns3_mouse_move(engine, Int32(p.x * frameSize.width), Int32(p.y * frameSize.height))
            case let .wheel(n): vns3_wheel(engine, Int32(n))
            }
        }
    }

    private func finish(byGame: Bool) {
        displayLink?.invalidate()
        displayLink = nil
        audio?.stop()
        audio = nil
        if let engine {
            if let problem = vns3_problem(engine) {
                system3Log.error("\(self.launch.game.id.rawValue, privacy: .public) stopped: \(String(cString: problem), privacy: .public)")
            }
            vns3_close(engine)
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
        let frame = frameView
        deliver(router.translate(event) { frame.framePoint(fromViewNormalized: $0) })
    }

    func setPaused(_ paused: Bool) {
        self.paused = paused
        guard let engine else { return }
        if paused { deliver(router.releaseAll()) }
        vns3_set_paused(engine, paused)
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

/// The engine pointer, handed to the audio thread. vns3_audio is safe to call there.
nonisolated private final class EngineBox: @unchecked Sendable {
    let engine: OpaquePointer
    init(_ engine: OpaquePointer) { self.engine = engine }
}

/// A display link keeps its target, so the session sits behind a weak reference to avoid a cycle.
private final class FrameTarget: NSObject {
    private weak var session: System3Session?
    init(_ session: System3Session) { self.session = session }
    @objc func step(_ link: CADisplayLink) { session?.step(link) }
}
