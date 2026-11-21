import CANIM
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

nonisolated let animLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "anim")

final class ANIMSession: EngineSession {
    var failedToStart: Bool { driver.failedToStart }
    var startFailureReason: String? { driver.startFailureReason }
    let view: PlatformView
    private let driver: FrameDriver
    private var frameView: FrameView { driver.view }
    private var engine: OpaquePointer? { driver.engine }
    private let launch: GameLaunch
    private let root: URL
    private let saveDirectory: URL
    private var router = ANIMInputRouter()
    private var frameSize = CGSize(width: 1280, height: 720)
    private var shownSerial: UInt32 = .max
    private var showingPage = false

    init(_ launch: GameLaunch, root: URL, saveDirectory: URL) {
        self.launch = launch
        self.root = root
        self.saveDirectory = saveDirectory
        let id = launch.game.id.rawValue
        driver = FrameDriver(pacingTag: "ANFRAMES", setPaused: { vnanim_set_paused($0, $1) }, close: { engine in
            if let problem = vnanim_problem(engine) {
                animLog.error("\(id, privacy: .public) stopped: \(String(cString: problem), privacy: .public)")
            }
            vnanim_close(engine)
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
        let logURL = launch.dataDirectory.appendingPathComponent("logs/anim.txt")
        try? FileManager.default.createDirectory(at: logURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        try? Data().write(to: logURL)
        try? FileManager.default.createDirectory(at: saveDirectory, withIntermediateDirectories: true)
        let saveURL = saveDirectory
        let rate = 44100.0
        var problem = [CChar](repeating: 0, count: 512)
        let started: OpaquePointer? = root.path.withCString { rootPath in
            saveURL.path.withCString { savePath in
                logURL.path.withCString { logPath in
                    var config = VNANIMConfig()
                    config.gameDir = rootPath
                    config.saveDir = savePath
                    config.logPath = logPath
                    config.audioRate = Int32(rate)
                    return vnanim_open(&config, &problem, problem.count)
                }
            }
        }
        guard let started else {
            let reason = String(decoding: problem.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self)
            animLog.error("\(self.launch.game.id.rawValue, privacy: .public) did not start: \(reason, privacy: .public)")
            driver.failToStart(reason)
            return
        }
        animLog.info("started \(self.launch.game.id.rawValue, privacy: .public)")
        driver.run(started, sampleRate: rate, audio: { vnanim_audio($0, $1, $2) }) { [weak self] engine, ms, link in
            self?.step(engine, ms, link)
        }
        if driver.paused { setPaused(true) }
    }

    private func step(_ engine: OpaquePointer, _ ms: Int, _ link: CADisplayLink) {
        if !vnanim_frame(engine, UInt32(min(max(ms, 0), 100))) {
            driver.finish(byGame: true)
            return
        }
        if let page = vnanim_take_page(engine), !showingPage { showPage(URL(fileURLWithPath: String(cString: page))) }
        let serial = vnanim_frame_serial(engine)
        #if DEBUG
        driver.pacing?.note(link, presented: serial != shownSerial)
        #endif
        guard serial != shownSerial else { return }
        shownSerial = serial
        var w: Int32 = 0, h: Int32 = 0
        if let pixels = vnanim_pixels(engine, &w, &h), w > 0, h > 0 {
            frameSize = CGSize(width: Int(w), height: Int(h))
            frameView.present(pixels, width: Int(w), height: Int(h))
        }
    }

    private func deliver(_ inputs: [ANIMInput]) {
        guard let engine else { return }
        for input in inputs {
            switch input {
            case let .key(code, down): vnanim_key(engine, code, down)
            case let .mouseButton(button, down): vnanim_mouse_button(engine, button, down)
            case let .mouseMove(p):
                vnanim_mouse_move(engine, Int32(p.x * frameSize.width), Int32(p.y * frameSize.height))
            case let .wheel(n): vnanim_wheel(engine, Int32(n))
            }
        }
    }

    func send(_ event: ControllerEvent) {
        let frame = frameView
        deliver(router.translate(event) { frame.framePoint(fromViewNormalized: $0) })
    }

    /// The game's own help page, which the PC opens in a browser, in a sheet over the game. It reads only the game's
    /// folder and never the network; the game waits while it's open.
    private func showPage(_ page: URL) {
        showingPage = true
        setPaused(true)
        PageSheet.show(page, title: "Help", over: view) { [weak self] in
            guard let self else { return }
            showingPage = false
            setPaused(false)
        }
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
