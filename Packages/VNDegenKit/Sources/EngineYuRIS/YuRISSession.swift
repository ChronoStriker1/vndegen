import CYuRIS
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

nonisolated let yurisLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "yuris")

final class YuRISSession: EngineSession {
    private(set) var failedToStart = false
    private(set) var startFailureReason: String?
    let view: PlatformView
    private let frameView = FrameView()
    private let launch: GameLaunch
    private let root: URL
    private var engine: OpaquePointer?
    private var audio: PCMAudioOutput?
    private var displayLink: CADisplayLink?
    private var clock = MillisecondClock()
    private var router = YuRISInputRouter()
    private var frameSize = CGSize(width: 1280, height: 720)
    private var shownSerial: UInt32 = .max
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
        frameView.onKey = { [weak self] usage, pressed, _ in
            guard let self else { return }
            deliver(router.hostKey(hidUsage: usage, pressed: pressed))
        }
    }

    private func start() {
        guard engine == nil, !stopped else { return }
        let logURL = launch.dataDirectory.appendingPathComponent("logs/yuris.txt")
        try? FileManager.default.createDirectory(at: logURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        try? Data().write(to: logURL)
        let existing = (try? FileManager.default.contentsOfDirectory(atPath: root.path))?.first { $0.lowercased() == "save" }
        let saveURL = root.appending(path: existing ?? "save", directoryHint: .isDirectory)
        let rate = 44100.0
        var problem = [CChar](repeating: 0, count: 512)
        let started: OpaquePointer? = root.path.withCString { rootPath in
            saveURL.path.withCString { savePath in
                logURL.path.withCString { logPath in
                    var config = VNYConfig()
                    config.gameDir = rootPath
                    config.saveDir = savePath
                    config.logPath = logPath
                    config.audioRate = Int32(rate)
                    return vny_open(&config, &problem, problem.count)
                }
            }
        }
        guard let started else {
            let reason = String(decoding: problem.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self)
            yurisLog.error("\(self.launch.game.id.rawValue, privacy: .public) did not start: \(reason, privacy: .public)")
            failedToStart = true
            startFailureReason = reason
            finish(byGame: true)
            return
        }
        engine = started
        yurisLog.info("started \(self.launch.game.id.rawValue, privacy: .public)")
        let box = EngineBox(started)
        audio = PCMAudioOutput(sampleRate: rate) { out, frames in vny_audio(box.engine, out, Int32(frames)) }
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
        let ms = clock.tick(at: link.timestamp)
        guard !paused else { return }
        if !vny_frame(engine, UInt32(min(max(ms, 0), 100))) {
            finish(byGame: true)
            return
        }
        let serial = vny_frame_serial(engine)
        guard serial != shownSerial else { return }
        shownSerial = serial
        var w: Int32 = 0, h: Int32 = 0
        if let pixels = vny_pixels(engine, &w, &h), w > 0, h > 0 {
            frameSize = CGSize(width: Int(w), height: Int(h))
            frameView.present(pixels, width: Int(w), height: Int(h))
        }
    }

    private func deliver(_ inputs: [YuRISInput]) {
        guard let engine else { return }
        for input in inputs {
            switch input {
            case let .key(code, down): vny_key(engine, code, down)
            case let .mouseButton(button, down): vny_mouse_button(engine, button, down)
            case let .mouseMove(p):
                vny_mouse_move(engine, Int32(p.x * frameSize.width), Int32(p.y * frameSize.height))
            case let .wheel(n): vny_wheel(engine, Int32(n))
            }
        }
    }

    private func finish(byGame: Bool) {
        displayLink?.invalidate()
        displayLink = nil
        audio?.stop()
        audio = nil
        if let engine {
            if let problem = vny_problem(engine) {
                yurisLog.error("\(self.launch.game.id.rawValue, privacy: .public) stopped: \(String(cString: problem), privacy: .public)")
            }
            vny_close(engine)
            self.engine = nil
        }
        stopped = true
        let waiters = stopWaiters
        stopWaiters = []
        waiters.forEach { $0.resume() }
        if byGame { launch.onExit() }
    }

    func send(_ event: ControllerEvent) {
        let frame = frameView
        deliver(router.translate(event) { frame.framePoint(fromViewNormalized: $0) })
    }

    func setPaused(_ paused: Bool) {
        self.paused = paused
        guard let engine else { return }
        if paused { deliver(router.releaseAll()) }
        vny_set_paused(engine, paused)
        if paused { audio?.pause() } else { audio?.resume() }
        clock.reset()
        if !paused { frameView.takeFocus() }
    }

    func terminate() {
        guard !stopped else { return }
        if engine == nil { failedToStart = true }
        finish(byGame: false)
    }

    func waitUntilStopped() async {
        if stopped { return }
        await withCheckedContinuation { stopWaiters.append($0) }
    }
}

nonisolated private final class EngineBox: @unchecked Sendable {
    let engine: OpaquePointer
    init(_ engine: OpaquePointer) { self.engine = engine }
}

private final class FrameTarget: NSObject {
    private weak var session: YuRISSession?
    init(_ session: YuRISSession) { self.session = session }
    @objc func step(_ link: CADisplayLink) { session?.step(link) }
}
