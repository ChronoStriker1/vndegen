import CPJADV
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

nonisolated let pjadvLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "pjadv")

final class PJADVSession: EngineSession {
    var failedToStart: Bool { driver.failedToStart }
    var startFailureReason: String? { driver.startFailureReason }
    let view: PlatformView
    private let driver: FrameDriver
    private var frameView: FrameView { driver.view }
    private var engine: OpaquePointer? { driver.engine }
    private let launch: GameLaunch
    private let root: URL
    private let saveDirectory: URL
    private var router = PJADVInputRouter()
    private var frameSize = CGSize(width: 1920, height: 1080)
    private var shownSerial: UInt32 = .max

    init(_ launch: GameLaunch, root: URL, saveDirectory: URL) {
        self.launch = launch
        self.root = root
        self.saveDirectory = saveDirectory
        let id = launch.game.id.rawValue
        driver = FrameDriver(pacingTag: "PJFRAMES", setPaused: { vnpjadv_set_paused($0, $1) }, close: { engine in
            if let problem = vnpjadv_problem(engine) {
                pjadvLog.error("\(id, privacy: .public) stopped: \(String(cString: problem), privacy: .public)")
            }
            vnpjadv_close(engine)
        }, onExit: launch.onExit)
        view = driver.view
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
        guard driver.canStart else { return }
        let logURL = launch.dataDirectory.appendingPathComponent("logs/pjadv.txt")
        try? FileManager.default.createDirectory(at: logURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        try? Data().write(to: logURL)
        try? FileManager.default.createDirectory(at: saveDirectory, withIntermediateDirectories: true)
        let saveURL = saveDirectory
        let rate = 44100.0
        var problem = [CChar](repeating: 0, count: 512)
        let started: OpaquePointer? = root.path.withCString { rootPath in
            saveURL.path.withCString { savePath in
                logURL.path.withCString { logPath in
                    var config = VNPJADVConfig()
                    config.gameDir = rootPath
                    config.saveDir = savePath
                    config.logPath = logPath
                    config.audioRate = Int32(rate)
                    config.language = PJADVLanguage.choose().rawValue
                    config.seed = UInt32.random(in: 1...UInt32.max)
                    return vnpjadv_open(&config, &problem, problem.count)
                }
            }
        }
        guard let started else {
            let reason = String(decoding: problem.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self)
            pjadvLog.error("\(self.launch.game.id.rawValue, privacy: .public) did not start: \(reason, privacy: .public)")
            driver.failToStart(reason)
            return
        }
        pjadvLog.info("started \(self.launch.game.id.rawValue, privacy: .public)")
        driver.run(started, sampleRate: rate, audio: { vnpjadv_audio($0, $1, $2) }) { [weak self] engine, ms, link in
            self?.step(engine, ms, link)
        }
        if driver.paused { setPaused(true) }
    }

    private func step(_ engine: OpaquePointer, _ ms: Int, _ link: CADisplayLink) {
        if !vnpjadv_frame(engine, UInt32(min(max(ms, 0), 100))) {
            driver.finish(byGame: true)
            return
        }
        let serial = vnpjadv_frame_serial(engine)
        #if DEBUG
        driver.pacing?.note(link, presented: serial != shownSerial)
        #endif
        guard serial != shownSerial else { return }
        shownSerial = serial
        var w: Int32 = 0, h: Int32 = 0
        if let pixels = vnpjadv_pixels(engine, &w, &h), w > 0, h > 0 {
            frameSize = CGSize(width: Int(w), height: Int(h))
            frameView.present(pixels, width: Int(w), height: Int(h))
        }
    }

    private func deliver(_ inputs: [PJADVInput]) {
        guard let engine else { return }
        for input in inputs {
            switch input {
            case let .key(code, down): vnpjadv_key(engine, code, down)
            case let .mouseButton(button, down): vnpjadv_mouse_button(engine, button, down)
            case let .mouseMove(p):
                vnpjadv_mouse_move(engine, Int32(p.x * frameSize.width), Int32(p.y * frameSize.height))
            case let .wheel(n): vnpjadv_wheel(engine, Int32(n))
            }
        }
    }

    func send(_ event: ControllerEvent) {
        let frame = frameView
        deliver(router.translate(event) { frame.framePoint(fromViewNormalized: $0) })
    }

    func setPaused(_ paused: Bool) {
        driver.setPaused(paused) { _ in
            if paused { deliver(router.releaseAll()) }
        }
    }

    func terminate() {
        driver.terminate()
    }

    func waitUntilStopped() async {
        await driver.waitUntilStopped()
    }
}
