import CSiglus
import FontPack
import Foundation
import OSLog
import QuartzCore
import VNCore
#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

nonisolated let siglusLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "siglus")

/// A SiglusEngine game running on siglus_rs in VNDegen's process, drawn by wgpu's Metal backend into `view`, so the
/// hub's menu and touch controls sit over it. The engine runs one frame per display refresh on the main thread. The
/// game starts once the view is on screen, and the game quitting itself ends the session through `onExit`.
final class SiglusSession: EngineSession {
    let view: PlatformView
    private let host = SiglusHostView()
    private let core: SiglusCore
    private let launch: GameLaunch
    private let root: URL
    private var game: OpaquePointer?
    private var screen = CGSize(width: 1280, height: 720)
    private var router = SiglusInputRouter()
    private var displayLink: CADisplayLink?
    private var lastFrame: CFTimeInterval?
    private var paused = false
    private var stopped = false
    /// The game's message box while one is open.
    private var prompt: SiglusPrompt?

    init(_ launch: GameLaunch, root: URL, core: SiglusCore) {
        self.launch = launch
        self.root = root
        self.core = core
        view = host
        host.onReady = { [weak self] in self?.start() }
        host.onResize = { [weak self] in self?.resize() }
        host.onKey = { [weak self] key, pressed, isRepeat in
            guard let self else { return }
            router.hostKey(key, pressed: pressed, isRepeat: isRepeat).forEach(deliver)
        }
        host.onEvent = { [weak self] event in self?.send(event) }
    }

    private func start() {
        guard game == nil, !stopped else { return }
        SiglusEngineLock.inUse = true
        let logURL = launch.dataDirectory.appending(path: "logs/siglus.txt")
        try? FileManager.default.createDirectory(at: logURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        try? Data().write(to: logURL)
        core.prepare(log: logURL, font: FontPack.resolve("MS PGothic")?.file)
        #if canImport(UIKit)
        let kind = Int32(VNSIGLUS_VIEW_UIKIT)
        #else
        let kind = Int32(VNSIGLUS_VIEW_APPKIT)
        #endif
        let size = host.pixelSize
        var error: UnsafeMutablePointer<CChar>?
        let created = core.api.siglus_create!(kind, Unmanaged.passUnretained(host).toOpaque(), size.width, size.height,
                                             size.scale, root.path, &error)
        guard let created else { return fail(core.take(error)) }
        game = created
        var width: UInt32 = 0, height: UInt32 = 0
        core.api.siglus_screen_size!(created, &width, &height)
        screen = CGSize(width: Int(width), height: Int(height))
        core.api.siglus_set_message_box!(created, { context, request, kind, title, message in
            guard let context else { return }
            let session = Unmanaged<SiglusSession>.fromOpaque(context).takeUnretainedValue()
            let title = title.map { String(cString: $0) } ?? "", message = message.map { String(cString: $0) } ?? ""
            MainActor.assumeIsolated { session.showMessageBox(request, kind: kind, title: title, message: message) }
        }, Unmanaged.passUnretained(self).toOpaque())
        siglusLog.info("started \(self.launch.game.id.rawValue, privacy: .public), \(width)x\(height)")
        if paused { core.api.siglus_set_paused!(created, 1) }
        host.takeFocus()
        let target = FrameTarget(self)
        #if canImport(UIKit)
        let link = CADisplayLink(target: target, selector: #selector(FrameTarget.step))
        #else
        let link = host.displayLink(target: target, selector: #selector(FrameTarget.step))
        #endif
        link.add(to: .main, forMode: .common)
        displayLink = link
    }

    private func fail(_ reason: String) {
        siglusLog.error("\(self.launch.game.id.rawValue, privacy: .public): \(reason, privacy: .public)")
        finish()
        launch.onExit()
    }

    private func resize() {
        guard let game else { return }
        let size = host.pixelSize
        core.api.siglus_resize!(game, size.width, size.height, size.scale)
    }

    fileprivate func step() {
        guard let game, let link = displayLink, !paused else { return }
        let now = link.timestamp
        let elapsed = lastFrame.map { min(max((now - $0) * 1000, 1), 100) } ?? 16
        lastFrame = now
        switch Int(core.api.siglus_step!(game, UInt32(elapsed.rounded()))) {
        case VNSIGLUS_RUNNING: break
        case VNSIGLUS_ENDED:
            finish()
            launch.onExit()
        default:
            fail("the engine stopped; see logs/siglus.txt")
        }
    }

    // MARK: Message boxes

    /// The game's own message box, which the script waits on until it is answered.
    private func showMessageBox(_ request: UInt64, kind: Int32, title: String, message: String) {
        let buttons = SiglusPrompt.buttons(kind)
        let answer: @MainActor (Int) -> Void = { [weak self] index in
            guard let self else { return }
            prompt = nil
            if let game { core.api.siglus_answer!(game, request, Int64(index)) }
        }
        #if canImport(UIKit)
        let alert = UIAlertController(title: title, message: message, preferredStyle: .alert)
        for (index, button) in buttons.enumerated() {
            let style: UIAlertAction.Style = index == buttons.count - 1 && buttons.count > 1 ? .cancel : .default
            alert.addAction(UIAlertAction(title: button, style: style) { _ in answer(index) })
        }
        prompt = SiglusPrompt(buttons: buttons.count) { [weak alert] index in
            alert?.dismiss(animated: true)
            answer(index)
        }
        var presenter = host.window?.rootViewController
        while let shown = presenter?.presentedViewController { presenter = shown }
        presenter?.present(alert, animated: true)
        #else
        let alert = NSAlert()
        alert.messageText = title.isEmpty ? message : title
        if !title.isEmpty { alert.informativeText = message }
        buttons.forEach { alert.addButton(withTitle: $0) }
        guard let window = host.window else {
            return answer(alert.runModal().rawValue - NSApplication.ModalResponse.alertFirstButtonReturn.rawValue)
        }
        prompt = SiglusPrompt(buttons: buttons.count) { [weak window, weak alert] index in
            guard let window, let sheet = alert?.window else { return }
            window.endSheet(sheet, returnCode: NSApplication.ModalResponse(rawValue: NSApplication.ModalResponse.alertFirstButtonReturn.rawValue + index))
        }
        alert.beginSheetModal(for: window) { response in
            MainActor.assumeIsolated { answer(response.rawValue - NSApplication.ModalResponse.alertFirstButtonReturn.rawValue) }
        }
        #endif
    }

    // MARK: Input

    private func deliver(_ input: SiglusInput) {
        guard let game else { return }
        switch input {
        case let .key(key, down, isRepeat):
            core.api.siglus_key!(game, key, down ? 1 : 0, isRepeat ? 1 : 0)
        case let .button(button, down):
            core.api.siglus_button!(game, button, down ? 1 : 0)
        case let .pointer(location):
            let point = Self.screenPoint(location, view: host.bounds.size, screen: screen)
            core.api.siglus_pointer!(game, point.x, point.y)
        case let .wheel(notches):
            core.api.siglus_wheel!(game, notches)
        }
    }

    /// A point normalized over the view, as a pixel of the game's screen. The screen fills the view's middle, with bars
    /// beside or above it.
    nonisolated static func screenPoint(_ location: CGPoint, view: CGSize, screen: CGSize) -> CGPoint {
        guard view.width > 0, view.height > 0, screen.width > 0, screen.height > 0 else { return .zero }
        let scale = min(view.width / screen.width, view.height / screen.height)
        let left = (view.width - screen.width * scale) / 2, top = (view.height - screen.height * scale) / 2
        return CGPoint(x: (location.x * view.width - left) / scale, y: (location.y * view.height - top) / scale)
    }

    // MARK: EngineSession

    func send(_ event: ControllerEvent) {
        guard game != nil, !paused else { return }
        if let prompt {
            prompt.handle(event)
            return
        }
        router.translate(event).forEach(deliver)
    }

    func type(_ typed: TypedText) {
        guard let game, core.api.siglus_wants_text!(game) == 1 else {
            for key in KeyboardKey.keys(typing: typed) {
                send(.key(key, pressed: true))
                send(.key(key, pressed: false))
            }
            return
        }
        switch typed {
        case let .text(text): core.api.siglus_text!(game, text)
        case .deleteBackward:
            core.api.siglus_key!(game, VirtualKey.backspace, 1, 0)
            core.api.siglus_key!(game, VirtualKey.backspace, 0, 0)
        case .newline:
            core.api.siglus_key!(game, VirtualKey.return, 1, 0)
            core.api.siglus_key!(game, VirtualKey.return, 0, 0)
        }
    }

    func setPaused(_ paused: Bool) {
        self.paused = paused
        // The hub's menu takes the keyboard, so the game would never see those keys come up.
        if paused { router.releaseAll().forEach(deliver) }
        lastFrame = nil
        if let game { core.api.siglus_set_paused!(game, paused ? 1 : 0) }
        if !paused { host.takeFocus() }
    }

    func terminate() {
        finish()
    }

    private func finish() {
        guard !stopped else { return }
        stopped = true
        displayLink?.invalidate()
        displayLink = nil
        if let game {
            core.api.siglus_set_message_box!(game, nil, nil)
            core.api.siglus_destroy!(game)
            self.game = nil
        }
        SiglusEngineLock.inUse = false
        siglusLog.info("\(self.launch.game.id.rawValue, privacy: .public) stopped")
    }
}

/// A message box the game shows, which a controller answers too: Confirm, Return, or Space presses its first button,
/// and Cancel, Escape, or E its last when it has more than one.
struct SiglusPrompt {
    let buttons: Int
    let close: (_ index: Int) -> Void

    /// The buttons of each kind of message box, in the order the engine counts them.
    static func buttons(_ kind: Int32) -> [String] {
        switch kind {
        case 1: ["OK", "Cancel"]
        case 2: ["Yes", "No"]
        case 3: ["Yes", "No", "Cancel"]
        default: ["OK"]
        }
    }

    func handle(_ event: ControllerEvent) {
        // In the "Keyboard and mouse" layout A and B arrive as Space and E, so the keys answer too.
        switch event {
        case .button(.confirm, pressed: true): close(0)
        case let .key(key, pressed: true) where key.keyCode == 13 || key.keyCode == 32: close(0)
        case .button(.cancel, pressed: true) where buttons > 1: close(buttons - 1)
        case let .key(key, pressed: true) where (key.keyCode == 27 || key.keyCode == 69) && buttons > 1: close(buttons - 1)
        default: break
        }
    }
}

/// A display link keeps its target, so the session sits behind a weak link to avoid a cycle.
private final class FrameTarget: NSObject {
    private weak var session: SiglusSession?

    init(_ session: SiglusSession) {
        self.session = session
    }

    @objc func step() { session?.step() }
}
