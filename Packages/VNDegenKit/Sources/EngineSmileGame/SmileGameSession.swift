import CSmileGame
import FontPack
import FrameHost
import Foundation
import OSLog
import QuartzCore
import VNCore

nonisolated let smileGameLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "smilegame")

final class SmileGameSession: EngineSession {
    var failedToStart: Bool { driver.failedToStart }
    var startFailureReason: String? { driver.startFailureReason }
    let view: PlatformView
    private let driver: FrameDriver
    private var frameView: FrameView { driver.view }
    private var engine: OpaquePointer? { driver.engine }
    private let launch: GameLaunch
    private let root: URL
    private let saveDirectory: URL
    private var router = SmileGameInputRouter()
    private var shownSerial: UInt32 = .max
    private var owedMs = 0
    static let shortestFrameMs = 15

    init(_ launch: GameLaunch, root: URL) {
        self.launch = launch
        self.root = root
        saveDirectory = SmileGameLayout.saveDirectory(in: root)
        let id = launch.game.id.rawValue
        driver = FrameDriver(pacingTag: "SGFRAMES", setPaused: { _, _ in }, close: { engine in
            if let problem = vnsmilegame_problem(engine) {
                smileGameLog.error("\(id, privacy: .public) stopped: \(String(cString: problem), privacy: .public)")
            }
            vnsmilegame_close(engine)
        }, onExit: launch.onExit)
        view = driver.view
        frameView.onReady = { [weak self] in self?.start() }
        frameView.onKey = { [weak self] usage, pressed, _ in
            guard let self else { return }
            deliver(router.hostKey(hidUsage: usage, pressed: pressed))
        }
    }

    private func start() {
        guard driver.canStart else { return }
        let logURL = launch.dataDirectory.appendingPathComponent("logs/smilegame.txt")
        try? FileManager.default.createDirectory(at: logURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        let font = FontPack.resolve("MS PGothic")?.file.path
        let rate = 44100.0
        var problem = [CChar](repeating: 0, count: 512)
        let strings = [root.path, saveDirectory.path, logURL.path, font].map { $0.flatMap { strdup($0) } }
        defer { strings.forEach { free($0) } }
        var config = VNSmileGameConfig()
        config.gameDir = UnsafePointer(strings[0])
        config.saveDir = UnsafePointer(strings[1])
        config.logPath = UnsafePointer(strings[2])
        config.fallbackFont = UnsafePointer(strings[3])
        config.audioRate = Int32(rate)
        guard let started = vnsmilegame_open(&config, &problem, problem.count) else {
            let reason = String(decoding: problem.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self)
            smileGameLog.error("\(self.launch.game.id.rawValue, privacy: .public) did not start: \(reason, privacy: .public)")
            driver.failToStart(reason)
            return
        }
        smileGameLog.info("started \(self.launch.game.id.rawValue, privacy: .public)")
        driver.run(started, sampleRate: rate, audio: { vnsmilegame_audio($0, $1, $2) }) { [weak self] engine, ms, link in
            self?.step(engine, ms, link)
        }
        if driver.paused { setPaused(true) }
    }

    private func step(_ engine: OpaquePointer, _ ms: Int, _ link: CADisplayLink) {
        #if DEBUG
        let began = CACurrentMediaTime()
        #endif
        owedMs = min(owedMs + max(ms, 0), 100)
        var presented = false
        if owedMs >= Self.shortestFrameMs {
            if !vnsmilegame_frame(engine, UInt32(owedMs)) {
                driver.finish(byGame: true)
                return
            }
            #if DEBUG
            gameSeconds += CACurrentMediaTime() - began
            gameFrames += 1
            #endif
            owedMs = 0
            presented = present(engine)
        }
        #if DEBUG
        driver.pacing?.note(link, presented: presented, began: began, progress: progress(engine))
        #endif
    }

    #if DEBUG
    private var gameSeconds = 0.0
    private var gameFrames = 0

    private func progress(_ engine: OpaquePointer) -> String {
        let scene = vnsmilegame_scene(engine).map { String(cString: $0) } ?? ""
        let line = String(format: "%@ runtime=%.1fms", scene, gameSeconds / Double(max(gameFrames, 1)) * 1000)
        gameSeconds = 0
        gameFrames = 0
        return line
    }
    #endif

    private func present(_ engine: OpaquePointer) -> Bool {
        let serial = vnsmilegame_frame_serial(engine)
        let presented = serial != shownSerial
        if presented {
            shownSerial = serial
            var w: Int32 = 0, h: Int32 = 0
            if let pixels = vnsmilegame_pixels(engine, &w, &h), w > 0, h > 0 {
                frameView.present(pixels, width: Int(w), height: Int(h))
            }
        }
        return presented
    }

    private func deliver(_ keys: [SmileGameKey]) {
        guard let engine else { return }
        for key in keys { vnsmilegame_key(engine, key.code, key.down) }
    }

    func send(_ event: ControllerEvent) {
        deliver(router.translate(event))
    }

    func gameMenu() async -> [GameMenuItem] {
        guard engine != nil, !driver.stopped else { return [] }
        let slots = SmileGameSlots.list(in: saveDirectory)
        return [
            GameMenuItem(id: "save", title: "Save", kind: .submenu(slots.map { slot in
                GameMenuItem(id: SmileGameSlots.saveID(slot.index), title: slot.title)
            })),
            GameMenuItem(id: "load", title: "Load", kind: .submenu(slots.map { slot in
                GameMenuItem(id: SmileGameSlots.loadID(slot.index), title: slot.title, isEnabled: slot.written != nil)
            })),
        ]
    }

    func chooseGameMenuItem(_ id: GameMenuItem.ID) {
        guard let engine, !driver.stopped, let choice = SmileGameSlots.choice(id) else { return }
        deliver(router.releaseAll())
        switch choice {
        case let .save(slot):
            let ok = vnsmilegame_save(engine, Int32(slot))
            smileGameLog.info("save slot \(slot + 1) \(ok ? "written" : "failed", privacy: .public)")
        case let .load(slot):
            let ok = vnsmilegame_load(engine, Int32(slot))
            smileGameLog.info("load slot \(slot + 1) \(ok ? "done" : "failed", privacy: .public)")
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
