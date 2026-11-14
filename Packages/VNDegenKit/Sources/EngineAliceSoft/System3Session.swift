import CSystem3
import FontPack
import FrameHost
import Foundation
import OSLog
import QuartzCore
import SwiftUI
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
    var failedToStart: Bool { driver.failedToStart }
    var startFailureReason: String? { driver.startFailureReason }
    let view: PlatformView
    private let driver: FrameDriver
    private var frameView: FrameView { driver.view }
    private var engine: OpaquePointer? { driver.engine }
    private let launch: GameLaunch
    private let root: URL
    private var router = System3InputRouter()
    private var frameSize = CGSize(width: 640, height: 480)
    private var shownSerial: UInt32 = .max
    private var numberBox: System3NumberBox?
    private var numberView: PlatformView?
    #if canImport(UIKit)
    private var numberController: UIHostingController<System3NumberBoxView>?
    #endif
    private var pointerLocation: CGPoint?
    private lazy var system3Cheats = System3Cheats { [weak self] in self?.engine }
    var cheats: (any CheatSession)? { system3Cheats }
    private static let textSkipItem = "textSkip"

    init(_ launch: GameLaunch, root: URL) {
        self.launch = launch
        self.root = root
        let id = launch.game.id.rawValue
        driver = FrameDriver(pacingTag: "S3FRAMES", setPaused: { vns3_set_paused($0, $1) }, close: { engine in
            if let problem = vns3_problem(engine) {
                system3Log.error("\(id, privacy: .public) stopped: \(String(cString: problem), privacy: .public)")
            }
            vns3_close(engine)
        }, onExit: launch.onExit)
        view = driver.view
        driver.willFinish = { [weak self] in self?.closeNumberBox() }
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
        guard driver.canStart else { return }
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
            driver.failToStart(reason)
            return
        }
        system3Log.info("started \(self.launch.game.id.rawValue, privacy: .public)")
        driver.run(started, sampleRate: rate, audio: { vns3_audio($0, $1, $2) }) { [weak self] engine, ms, link in
            self?.step(engine, ms, link)
        }
        system3Cheats.apply(launch.game.cheats ?? CheatSettings())
        if driver.paused { setPaused(true) }
    }

    private func step(_ engine: OpaquePointer, _ ms: Int, _ link: CADisplayLink) {
        if !vns3_frame(engine, UInt32(min(max(ms, 0), 100))) {
            driver.finish(byGame: true)
            return
        }
        let serial = vns3_frame_serial(engine)
        #if DEBUG
        driver.pacing?.note(link, presented: serial != shownSerial)
        #endif
        if numberBox == nil { showNumberBoxIfAsked() }
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
        if let numberBox {
            // The box takes the input while it's up. A right click is the game's cancel, and the box's too.
            for input in inputs {
                switch input {
                case let .key(code, down: true): numberBox.press(virtualKey: code)
                case .mouseButton(mouseRight, down: true): numberBox.press(.cancel)
                default: break
                }
            }
            return
        }
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

    // MARK: Number box

    private func showNumberBoxIfAsked() {
        guard let engine else { return }
        var value: Int32 = 0, minimum: Int32 = 0, maximum: Int32 = 0
        guard vns3_number_request(engine, &value, &minimum, &maximum) else { return }
        // The box takes the input, so the game sees every key and mouse button it holds come up, as when the menu opens.
        deliver(router.releaseToNumberBox())
        let entry = System3NumberEntry(value: Int(value), minimum: Int(minimum), maximum: Int(maximum))
        let title = String(cString: vns3_number_title(engine))
        let box = System3NumberBox(title: title.isEmpty ? "Enter a number" : title, entry: entry) { [weak self] answer in
            self?.answerNumberBox(answer)
        }
        numberBox = box
        let content = System3NumberBoxView(box: box)
        #if canImport(UIKit)
        let controller = UIHostingController(rootView: content)
        controller.view.backgroundColor = .clear
        numberController = controller
        let shown: PlatformView = controller.view
        shown.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        #else
        let shown: PlatformView = NSHostingView(rootView: content)
        shown.autoresizingMask = [.width, .height]
        #endif
        shown.frame = frameView.bounds
        frameView.addSubview(shown)
        numberView = shown
        frameView.takeFocus()
    }

    private func closeNumberBox() {
        numberBox = nil
        numberView?.removeFromSuperview()
        numberView = nil
        #if canImport(UIKit)
        numberController = nil
        #endif
    }

    private func answerNumberBox(_ answer: Int?) {
        closeNumberBox()
        guard let engine else { return }
        vns3_number_answer(engine, Int32(answer ?? 0), answer != nil)
        frameView.takeFocus()
    }

    /// Input for the box that doesn't go through the router. A touch mouse's tap, or a button bound to a click, presses
    /// the control under the pointer when it lifts, and a secondary one cancels, as a right click does.
    private func handledByNumberBox(_ event: ControllerEvent, _ box: System3NumberBox) -> Bool {
        switch event {
        case let .pointer(pointer):
            pointerLocation = pointer.location
            guard pointer.phase == .ended else { return true }
            if pointer.button == .secondary { box.press(.cancel) }
            else if let control = box.control(at: pointer.location) { box.press(control) }
            return true
        case let .click(button, pressed):
            guard button == .primary else { return false }
            if !pressed, let location = pointerLocation, let control = box.control(at: location) { box.press(control) }
            return true
        case let .key(key, pressed):
            if pressed { box.press(boundKey: key.keyCode) }
            return true
        default:
            return false
        }
    }

    // MARK: EngineSession

    func send(_ event: ControllerEvent) {
        if case let .pointer(pointer) = event { pointerLocation = pointer.location }
        if let numberBox, handledByNumberBox(event, numberBox) { return }
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

    func type(_ typed: TypedText) {
        if let numberBox {
            numberBox.type(typed)
            return
        }
        for key in KeyboardKey.keys(typing: typed) {
            send(.key(key, pressed: true))
            send(.key(key, pressed: false))
        }
    }

    /// The PC engines keep this in the window's menu bar: 文字送り on System 3.9, Skip until choice in the English
    /// System 3.0 exes. It turns itself off at the next choice or left click, so the menu asks the runtime each time.
    func gameMenu() async -> [GameMenuItem] {
        guard let engine, !driver.stopped else { return [] }
        return [GameMenuItem(id: Self.textSkipItem, title: "Skip until choice", isChecked: vns3_text_skip(engine))]
    }

    func chooseGameMenuItem(_ id: GameMenuItem.ID) {
        guard let engine, !driver.stopped, id == Self.textSkipItem else { return }
        vns3_set_text_skip(engine, !vns3_text_skip(engine))
    }

    func waitUntilStopped() async {
        await driver.waitUntilStopped()
    }
}
