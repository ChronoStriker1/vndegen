import CSystem4
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

nonisolated let system4Log = Logger(subsystem: "com.chronostriker1.vndegen", category: "system4")

/// A System 4 game running on VNDegen's runtime (CSystem4) in this process. The runtime runs the game's bytecode on a
/// thread of its own; each display refresh steps it on the main thread and shows its CPU frame through `FrameView`,
/// and its sound plays through `PCMAudioOutput`. The game ending itself ends the session through `onExit`.
final class System4Session: EngineSession {
    let view: PlatformView
    private let frameView = FrameView()
    private let launch: GameLaunch
    private let root: URL
    private var engine: OpaquePointer?
    private var audio: PCMAudioOutput?
    private var displayLink: CADisplayLink?
    private var lastTimestamp: CFTimeInterval?
    private var pacer = System4InputPacer()
    private var msCarry = 0.0  // the part of a millisecond the last step didn't pass on
    private var router = System4InputRouter()
    private var frameSize = CGSize(width: 800, height: 600)
    private var paused = false
    #if DEBUG
    private let pacing = UserDefaults.standard.bool(forKey: "frameMonitor") ? FramePacing(tag: "S4FRAMES") : nil
    #endif
    private var stopped = false
    private var stopWaiters: [CheckedContinuation<Void, Never>] = []

    @MainActor static func make(_ launch: GameLaunch, root: URL) throws -> any EngineSession {
        if let reason = compatibilityNote(root: root) { throw System4Error.unsupported(reason) }
        return System4Session(launch, root: root)
    }

    /// Why VNDegen can't run this System 4 game yet, for the game page, or nil when it can. Reads the game's code
    /// file to see which libraries it is built on.
    nonisolated static func compatibilityNote(root: URL) -> String? {
        root.path.withCString { vns4_unsupported($0).map { String(cString: $0) } }
    }

    /// System 4 games keep their saves in the folder the ini's SaveFolder names, SaveData beside the .ain, on every
    /// platform; the runtime does the same, so a PC backup restores into the folder the game reads.
    nonisolated static func saveLocations(root: URL, dataDirectory: URL) -> [SaveLocation] {
        let name = saveFolderName(root)
        let directory = FileCase.resolve(name, in: root)
        let key = name.lowercased()
        return [SaveLocation(directory: directory) { path in
            // `C:/Games/Game/SaveData/00.asd` maps to `00.asd`: every file under the save folder.
            let parts = path.split(separator: "/").map(String.init)
            guard let at = parts.lastIndex(where: { $0.lowercased() == key }), at + 1 < parts.count else { return nil }
            return parts[(at + 1)...].joined(separator: "/")
        }]
    }

    /// The ini's SaveFolder, or SaveData.
    nonisolated static func saveFolderName(_ root: URL) -> String {
        for ini in ["AliceStart.ini", "System40.ini"] {
            let url = FileCase.resolve(ini, in: root)
            guard let data = try? Data(contentsOf: url) else { continue }
            let text = String(decoding: data, as: UTF8.self)
            for line in text.split(whereSeparator: \.isNewline) {
                let parts = line.split(separator: "=", maxSplits: 1).map { $0.trimmingCharacters(in: .whitespaces) }
                guard parts.count == 2, parts[0] == "SaveFolder" else { continue }
                let value = parts[1].trimmingCharacters(in: CharacterSet(charactersIn: "\""))
                if !value.isEmpty { return value }
            }
        }
        return "SaveData"
    }

    init(_ launch: GameLaunch, root: URL) {
        self.launch = launch
        self.root = root
        view = frameView
        frameView.onReady = { [weak self] in self?.start() }
        frameView.onPointer = { [weak self] event in
            guard let self, !paused else { return }
            #if os(macOS)
            // A move with a button the system no longer holds: that button's mouse-up was lost on the way.
            if event.phase == .moved {
                let pressed = NSEvent.pressedMouseButtons
                for button in router.pointerButtons where pressed & (button == 1 ? 1 : 2) == 0 {
                    deliver(router.pointerReleased(button))
                }
            }
            #endif
            deliver(router.pointer(event, at: event.location))
        }
        frameView.onScroll = { [weak self] notches in
            guard let self, !paused else { return }
            let whole = Int(notches.rounded())
            if whole != 0 { deliver([.wheel(whole)]) }
        }
        frameView.onKey = { [weak self] usage, pressed, characters in
            // While paused nothing reaches the router or the game: pausing let go of every key, and a hold recorded
            // now would disagree with the game after resume.
            guard let self, !paused else { return }
            // Printable text goes to the game's text entry too, queued right after its key so it keeps its place.
            // Arrow and function keys type Apple's private-use characters, which are left out with the control
            // characters.
            let text = String(String.UnicodeScalarView((characters ?? "").unicodeScalars.filter {
                switch $0.properties.generalCategory {
                case .control, .privateUse, .format, .surrogate, .unassigned: false
                default: true
                }
            }))
            deliver(router.hostKey(hidUsage: usage, pressed: pressed) + (text.isEmpty ? [] : [.text(text)]))
        }
    }

    private func start() {
        guard engine == nil, !stopped else { return }
        let logURL = launch.dataDirectory.appendingPathComponent("logs/system4.txt")
        try? FileManager.default.createDirectory(at: logURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        try? Data().write(to: logURL)
        let saveDir = FileCase.resolve(Self.saveFolderName(root), in: root)
        try? FileManager.default.createDirectory(at: saveDir, withIntermediateDirectories: true)
        // The Windows font pack is already set up app-wide (FontPack), so the runtime is given no folder of its own.
        let started: OpaquePointer? = root.path.withCString { rootPath in
            saveDir.path.withCString { savePath in
                logURL.path.withCString { logPath in
                    var settings = VNS4Launch(gameRoot: rootPath, saveDir: savePath, logPath: logPath, fontsDir: nil)
                    return vns4_start(&settings)
                }
            }
        }
        guard let started else {
            let reason = vns4_start_error().map { String(cString: $0) } ?? "unknown"
            system4Log.error("\(self.launch.game.id.rawValue, privacy: .public) did not start: \(reason, privacy: .public)")
            finish(byGame: true)
            return
        }
        engine = started
        system4Log.info("started \(self.launch.game.id.rawValue, privacy: .public)")
        let box = System4EngineBox(started)
        audio = PCMAudioOutput { out, frames in vns4_audio_render(box.engine, out, Int32(frames)) }
        audio?.start()
        if paused { setPaused(true) }
        frameView.takeFocus()
        #if canImport(UIKit)
        let link = CADisplayLink(target: System4FrameTarget(self), selector: #selector(System4FrameTarget.step(_:)))
        #else
        let link = frameView.displayLink(target: System4FrameTarget(self), selector: #selector(System4FrameTarget.step(_:)))
        #endif
        link.add(to: .main, forMode: .common)
        displayLink = link
    }

    fileprivate func step(_ link: CADisplayLink) {
        guard let engine else { return }
        let now = link.timestamp
        // Whole milliseconds, carrying the fraction: rounding each 16.67 ms refresh to 17 ran the game 2% fast.
        let elapsed = lastTimestamp.map { (now - $0) * 1000 + msCarry } ?? 16
        let ms = Int(elapsed.rounded(.down))
        msCarry = elapsed - Double(ms)
        lastTimestamp = now
        guard !paused else { return }
        apply(pacer.poll(now: CACurrentMediaTime(), done: Int(vns4_frames_done(engine))))
        // The runtime caps the step at 100 ms and logs the time it drops.
        if !vns4_step(engine, Int32(min(max(ms, 0), 60_000))) {
            finish(byGame: true)
            return
        }
        var w: Int32 = 0, h: Int32 = 0
        if let pixels = vns4_frame(engine, &w, &h), w > 0, h > 0 {
            frameSize = CGSize(width: Int(w), height: Int(h))
            frameView.present(pixels, width: Int(w), height: Int(h))
        }
        #if DEBUG
        pacing?.note(tick: now, presented: true, stepEnd: CACurrentMediaTime(), target: link.targetTimestamp - link.timestamp)
        #endif
    }

    /// Hands inputs to the runtime as System4InputPacer allows; the rest follow on later frames.
    private func deliver(_ inputs: [System4Input]) {
        guard !inputs.isEmpty, let engine else { return }
        apply(pacer.add(inputs, now: CACurrentMediaTime(), done: Int(vns4_frames_done(engine))))
    }

    private func apply(_ inputs: [System4Input]) {
        guard let engine else { return }
        for input in inputs {
            switch input {
            case let .key(vk, down): vns4_key(engine, vk, down)
            case let .mouseMove(p):
                vns4_mouse_move(engine, Int32((p.x * frameSize.width).rounded(.down)), Int32((p.y * frameSize.height).rounded(.down)))
            case let .wheel(n): vns4_wheel(engine, Int32(n))
            case let .text(t): t.withCString { vns4_text(engine, $0) }
            }
        }
    }

    private func finish(byGame: Bool) {
        displayLink?.invalidate()
        displayLink = nil
        audio?.stop()
        audio = nil
        if let engine {
            if let problem = vns4_problem(engine) {
                system4Log.error("\(self.launch.game.id.rawValue, privacy: .public) stopped: \(String(cString: problem), privacy: .public)")
            }
            vns4_stop(engine)
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
        guard !paused else { return }
        let frame = frameView
        deliver(router.translate(event) { frame.framePoint(fromViewNormalized: $0) })
    }

    func setPaused(_ paused: Bool) {
        let wasPaused = self.paused
        self.paused = paused
        guard let engine else { return }
        if paused {
            // Input still waiting is dropped, and every key and button the game holds lets go now.
            _ = router.releaseAll()
            apply(pacer.clear(now: CACurrentMediaTime(), done: Int(vns4_frames_done(engine))))
        }
        if !paused, wasPaused { pacer.resumed(now: CACurrentMediaTime(), done: Int(vns4_frames_done(engine))) }
        vns4_set_paused(engine, paused)
        if paused { audio?.pause() } else { audio?.resume() }
        lastTimestamp = nil
        msCarry = 0
        #if DEBUG
        pacing?.reset()
        #endif
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

/// The engine pointer, handed to the audio thread. vns4_audio_render is safe to call there.
nonisolated private final class System4EngineBox: @unchecked Sendable {
    let engine: OpaquePointer
    init(_ engine: OpaquePointer) { self.engine = engine }
}

/// A display link keeps its target, so the session sits behind a weak reference to avoid a cycle.
private final class System4FrameTarget: NSObject {
    private weak var session: System4Session?
    init(_ session: System4Session) { self.session = session }
    @objc func step(_ link: CADisplayLink) { session?.step(link) }
}

/// Input for the runtime, in vns4.h's terms: Windows virtual-key codes (the mouse buttons are VK_LBUTTON 1 and
/// VK_RBUTTON 2) and points normalized over the frame.
nonisolated enum System4Error: Error, LocalizedError {
    case unsupported(String)

    var errorDescription: String? {
        switch self {
        case let .unsupported(reason): reason
        }
    }
}

nonisolated enum System4Input: Equatable, Sendable {
    case key(Int32, down: Bool)
    case mouseMove(CGPoint)
    case wheel(Int)
    case text(String)  // typed characters for the game's text entry, in order with the keys around them
}

/// Turns VNDegen's controller events and the session view's own keyboard into keys the game reads. Several buttons
/// can hold the same key, so a key goes down with the first press and up with the last release.
/// Paces input for the games, which read the keyboard and mouse once per game frame and only take a click on a button
/// the pointer has rested on. A touch moves the pointer, presses and lets go within a few milliseconds, which they
/// would miss. Inputs apply in the order they came, with these waits, counted in frames the game has finished
/// (vns4_frames_done). Input set while the count is D is surely read by the time it reaches D + 2, since frame D + 1
/// may have read the input before it changed.
/// - a mouse button press waits until the game has finished three frames with the pointer where the press is (Rance
///   02's title took none at one), or half a second has passed since the pointer moved;
/// - after a key or button changes, nothing more applies until the game has read it, so it sees every state and a
///   press its own position; a game that stops finishing frames (a load, or a loop waiting for a key to come up)
///   holds this back at most half a second, so no release waits on it for long;
/// - and the next key or button change also waits 100 ms after a press and 50 ms after a release (Sengoku Rance missed
///   a 17 ms tap).
/// Consecutive pointer moves collapse to the last, so a fast drag can't build a backlog in front of its release.
nonisolated struct System4InputPacer {
    static let hoverFrames = 3
    static let pressHold = 0.1
    static let releaseHold = 0.05

    private var queue: [System4Input] = []
    private var movedAt = Int.min / 2   // frames done when the pointer moved to where it is
    private var movedTime = -Double.infinity  // and when
    private var position: CGPoint?
    private var keyGate = 0.0           // no key or button change applies before this time
    private var changedAt = Int.min / 2 // frames done when the last key or button changed
    private var changedTime = -Double.infinity  // and when
    private var releasedAtPause = false  // at the last clear(), a change was still unread or pause let go of something
    static let readWaitLimit = 0.5
    private(set) var down: Set<Int32> = []  // keys and buttons the runtime holds

    /// Takes newly arrived inputs; returns those the runtime may have now. `done` is vns4_frames_done.
    mutating func add(_ inputs: [System4Input], now: Double, done: Int) -> [System4Input] {
        queue += inputs
        return drain(now, done: done)
    }

    /// Called before each game step; returns the inputs now due.
    mutating func poll(now: Double, done: Int) -> [System4Input] {
        drain(now, done: done)
    }

    /// Drops everything waiting and returns the releases of every key and button the runtime holds, as when the game
    /// pauses. Nothing applies after them until the game has read them.
    mutating func clear(now: Double, done: Int) -> [System4Input] {
        queue = []
        defer { down = [] }
        // A change the game hadn't read yet (a key let go just before the pause) must still be read after resume.
        if done < changedAt + 2 { releasedAtPause = true }
        if !down.isEmpty {
            changedAt = done
            changedTime = now
            keyGate = now + Self.releaseHold
            releasedAtPause = true
        }
        return down.sorted().map { .key($0, down: false) }
    }

    /// Called on resume: the game couldn't read anything while paused, so what pause let go of must still be read
    /// before anything else applies, however long the pause was. A pause that let go of nothing leaves pacing as it was.
    mutating func resumed(now: Double, done: Int) {
        guard releasedAtPause else { return }
        releasedAtPause = false
        changedAt = done
        changedTime = now
        keyGate = max(keyGate, now + Self.releaseHold)
    }

    private mutating func drain(_ now: Double, done: Int) -> [System4Input] {
        var out: [System4Input] = []
        var move: CGPoint?
        func flushMove() {
            if let m = move { out.append(.mouseMove(m)) }
            move = nil
        }
        loop: while let next = queue.first, done >= changedAt + 2 || now >= changedTime + Self.readWaitLimit {
            switch next {
            case let .mouseMove(p):
                if p != position {
                    position = p
                    movedAt = done
                    movedTime = now
                }
                move = p
            case .wheel, .text:
                flushMove()
                out.append(next)
            case let .key(vk, isDown):
                if now < keyGate { break loop }
                if isDown, vk == 1 || vk == 2, done < movedAt + 1 + Self.hoverFrames,
                   now < movedTime + Self.readWaitLimit { break loop }
                flushMove()
                out.append(next)
                if isDown { down.insert(vk) } else { down.remove(vk) }
                keyGate = now + (isDown ? Self.pressHold : Self.releaseHold)
                changedAt = done
                changedTime = now
            }
            queue.removeFirst()
        }
        flushMove()
        return out
    }
}

nonisolated struct System4InputRouter {
    static let pointsPerNotch = 40.0
    private var held: [Int32: Int] = [:]
    private var hostKeysDown: Set<Int> = []
    private var pointerDown: Set<Int32> = []  // mouse buttons a pointer holds: 1 left, 2 right
    private var scrolled = 0.0

    mutating func translate(_ event: ControllerEvent, frame: (CGPoint) -> CGPoint) -> [System4Input] {
        switch event {
        case let .button(button, pressed):
            return control(button, pressed)
        case let .gamepad(input, pressed):
            guard let button = input.virtualButton else { return [] }
            return control(button, pressed)
        case let .key(key, pressed):
            return press(Int32(key.keyCode), pressed)
        case let .click(button, pressed):
            return press(button == .primary ? 1 : 2, pressed)
        case let .pointer(pointer):
            return self.pointer(pointer, at: frame(pointer.location))
        case let .scroll(dy):
            scrolled += dy
            let notches = Int(scrolled / Self.pointsPerNotch)
            guard notches != 0 else { return [] }
            scrolled -= Double(notches) * Self.pointsPerNotch
            // Content scrolling down is the wheel turned toward the player, which opens no backlog.
            return [.wheel(-notches)]
        }
    }

    /// A pointer event whose location is already normalized over the frame, from the session view itself.
    mutating func pointer(_ pointer: PointerEvent, at location: CGPoint) -> [System4Input] {
        let button: Int32 = pointer.button == .primary ? 1 : 2
        switch pointer.phase {
        case .began:
            // A press of a button the router thinks is still down means its release was lost (AppKit has dropped a
            // mouse-up); without letting go first, the game would see one endless press and no click from here on.
            let stale = pointerDown.insert(button).inserted ? [] : press(button, false)
            return [.mouseMove(location)] + stale + press(button, true)
        case .moved: return [.mouseMove(location)]
        case .ended, .cancelled:
            guard pointerDown.remove(button) != nil else { return [.mouseMove(location)] }
            return [.mouseMove(location)] + press(button, false)
        }
    }

    /// The pointer buttons held as far as the router knows, to check against the system's own state.
    var pointerButtons: Set<Int32> { pointerDown }

    /// Lets go of a pointer button whose release never arrived.
    mutating func pointerReleased(_ button: Int32) -> [System4Input] {
        guard pointerDown.remove(button) != nil else { return [] }
        return press(button, false)
    }

    /// The controls System 4 games share: Confirm is Return (it advances text and picks), Cancel and Menu right click
    /// (which hides the message window or closes a menu), the arrows move, page up and down turn the wheel (the
    /// backlog), and Dash holds Control (skip).
    private mutating func control(_ button: VirtualButton, _ pressed: Bool) -> [System4Input] {
        switch button {
        case .confirm: press(13, pressed)
        case .cancel, .menu: press(2, pressed)
        case .up: press(38, pressed)
        case .down: press(40, pressed)
        case .left: press(37, pressed)
        case .right: press(39, pressed)
        case .pageUp: pressed ? [.wheel(1)] : []
        case .pageDown: pressed ? [.wheel(-1)] : []
        case .dash: press(17, pressed)
        case .hub: []
        }
    }

    /// A key the session view read from a hardware keyboard, by USB HID usage. A held key's repeats are ignored.
    mutating func hostKey(hidUsage: Int, pressed: Bool) -> [System4Input] {
        guard let vk = Self.windowsKey(hidUsage: hidUsage) else { return [] }
        if pressed {
            guard hostKeysDown.insert(hidUsage).inserted else { return [] }
        } else {
            guard hostKeysDown.remove(hidUsage) != nil else { return [] }
        }
        return press(vk, pressed)
    }

    mutating func releaseAll() -> [System4Input] {
        let out = held.keys.map { System4Input.key($0, down: false) }
        held = [:]
        hostKeysDown = []
        pointerDown = []
        return out
    }

    private mutating func press(_ vk: Int32, _ pressed: Bool) -> [System4Input] {
        let count = held[vk, default: 0]
        if pressed {
            held[vk] = count + 1
            guard count == 0 else { return [] }
        } else {
            guard count > 0 else { return [] }
            held[vk] = count == 1 ? nil : count - 1
            guard count == 1 else { return [] }
        }
        return [.key(vk, down: pressed)]
    }

    /// Windows virtual-key codes from USB HID usages.
    static func windowsKey(hidUsage u: Int) -> Int32? {
        switch u {
        case 4...29: Int32(65 + u - 4)  // A to Z
        case 30...38: Int32(49 + u - 30)  // 1 to 9
        case 39: 48  // 0
        case 40, 88: 13  // Return
        case 41: 27  // Escape
        case 42: 8  // Backspace
        case 43: 9  // Tab
        case 44: 32  // Space
        case 58...69: Int32(112 + u - 58)  // F1 to F12
        case 75: 33  // Page Up
        case 78: 34  // Page Down
        case 79: 39  // right
        case 80: 37  // left
        case 81: 40  // down
        case 82: 38  // up
        case 224, 228: 17  // Control
        case 225, 229: 16  // Shift
        default: nil
        }
    }
}
