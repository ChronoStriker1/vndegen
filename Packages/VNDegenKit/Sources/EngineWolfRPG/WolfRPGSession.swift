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
    var failedToStart: Bool { driver.failedToStart }
    var startFailureReason: String? { driver.startFailureReason }
    let view: PlatformView
    private let driver: FrameDriver
    private var frameView: FrameView { driver.view }
    private var engine: OpaquePointer? { driver.engine }
    private let launch: GameLaunch
    private let root: URL
    private var router = WolfRPGInputRouter()
    private var wantedText = false
    private var scrolledNotches = 0.0
    private lazy var wolfCheats = WolfRPGCheats { [weak self] in self?.engine }
    var cheats: (any CheatSession)? { wolfCheats }

    init(_ launch: GameLaunch, root: URL) {
        self.launch = launch
        self.root = root
        let id = launch.game.id.rawValue
        driver = FrameDriver(pacingTag: "WOLFFRAMES", setPaused: { vnwolf_set_paused($0, $1) }, close: { engine in
            if let problem = vnwolf_problem(engine) {
                wolfRPGLog.error("\(id, privacy: .public) stopped: \(String(cString: problem), privacy: .public)")
            }
            vnwolf_stop(engine)
        }, onExit: launch.onExit)
        view = driver.view
        frameView.onReady = { [weak self] in self?.start() }
        frameView.onPointer = { [weak self] event in
            guard let self else { return }
            if event.phase == .began { reopenKeyboardIfTyping() }
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
        guard driver.canStart else { return }
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
            let reason = vnwolf_start_error().map { String(cString: $0) }
            wolfRPGLog.error("\(self.launch.game.id.rawValue, privacy: .public) did not start: \(reason ?? "unknown", privacy: .public)")
            driver.failToStart(reason)
            return
        }
        wolfRPGLog.info("started \(self.launch.game.id.rawValue, privacy: .public)")
        driver.run(started, audio: { vnwolf_audio_render($0, $1, $2) }) { [weak self] engine, ms, link in
            self?.step(engine, ms, link)
        }
        wolfCheats.apply(launch.game.cheats ?? CheatSettings())
        if driver.paused { setPaused(true) }
    }

    private func step(_ engine: OpaquePointer, _ ms: Int, _ link: CADisplayLink) {
        if !vnwolf_step(engine, Int32(min(max(ms, 0), 100))) {
            driver.finish(byGame: true)
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
        driver.pacing?.note(link, presented: changed)
        #endif
    }

    /// A player who hid the keyboard while the game waits for text gets it back by touching the game.
    private func reopenKeyboardIfTyping() {
        guard let engine, vnwolf_wants_text(engine) else { return }
        NotificationCenter.default.post(name: .gameWantsKeyboard, object: self, userInfo: ["wants": true])
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

    // MARK: EngineSession

    func send(_ event: ControllerEvent) {
        // A controller's Confirm and Cancel finish the game's text entry, since neither types anything.
        if let engine, vnwolf_wants_text(engine) {
            let button: VirtualButton? = switch event {
            case let .button(button, true): button
            case let .gamepad(input, true): input.virtualButton
            default: nil
            }
            if button == .confirm { deliver(router.tap(Int32(VNWOLF_KEY_RETURN))) }
            if button == .cancel { deliver(router.tap(Int32(VNWOLF_KEY_ESCAPE))) }
        }
        switch event {
        case .click(_, pressed: true): reopenKeyboardIfTyping()
        case let .pointer(pointer) where pointer.phase == .began: reopenKeyboardIfTyping()
        default: break
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
