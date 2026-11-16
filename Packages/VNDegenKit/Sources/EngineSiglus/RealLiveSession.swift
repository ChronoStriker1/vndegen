import CSiglus
import FontPack
import FrameHost
import Foundation
import QuartzCore
import VNCore

/// A RealLive game running on siglus_rs in VNDegen's process. The engine steps once per display refresh on the main
/// thread and composites each frame on the CPU, which `FrameView` shows. Its sound plays through the engine's own Core
/// Audio output. The game ending itself ends the session through `onExit`.
final class RealLiveSession: EngineSession {
    let view: PlatformView
    private let frameView = FrameView()
    private let core: SiglusCore
    private let launch: GameLaunch
    private let root: URL
    private var game: OpaquePointer?
    private var frame = CGSize(width: 640, height: 480)
    private var router = SiglusInputRouter()
    private var displayLink: CADisplayLink?
    private var lastFrame: CFTimeInterval?
    private var paused = false
    private var stopped = false

    init(_ launch: GameLaunch, root: URL, core: SiglusCore) {
        self.launch = launch
        self.root = root
        self.core = core
        view = frameView
        frameView.onReady = { [weak self] in self?.start() }
        frameView.onKey = { [weak self] usage, pressed, _ in
            guard let self, let key = VirtualKey.forUsage(usage) else { return }
            router.hostKey(key, pressed: pressed, isRepeat: false).forEach(deliver)
        }
        // FrameView's own pointer events are normalized over the frame already.
        frameView.onPointer = { [weak self] event in
            guard let self else { return }
            router.translate(.pointer(event)).forEach(deliver)
        }
        frameView.onScroll = { [weak self] notches in
            let whole = Int32(notches.rounded())
            if whole != 0 { self?.deliver(.wheel(whole)) }
        }
    }

    private func start() {
        guard game == nil, !stopped else { return }
        SiglusEngineLock.inUse = true
        let logURL = launch.dataDirectory.appending(path: "logs/reallive.txt")
        let saves = RealLivePlugin.saveDirectory(launch.dataDirectory)
        try? FileManager.default.createDirectory(at: logURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        try? FileManager.default.createDirectory(at: saves, withIntermediateDirectories: true)
        try? Data().write(to: logURL)
        core.prepare(log: logURL, font: FontPack.resolve("MS PGothic")?.file)
        var error: UnsafeMutablePointer<CChar>?
        guard let opened = core.api.reallive_open!(root.path, saves.path, &error) else {
            siglusLog.error("\(self.launch.game.id.rawValue, privacy: .public): \(self.core.take(error), privacy: .public)")
            return finish(byGame: true)
        }
        game = opened
        siglusLog.info("started RealLive \(self.launch.game.id.rawValue, privacy: .public)")
        if paused { core.api.reallive_set_paused!(opened, 1) }
        frameView.takeFocus()
        let target = FrameTarget(self)
        #if canImport(UIKit)
        let link = CADisplayLink(target: target, selector: #selector(FrameTarget.step))
        #else
        let link = frameView.displayLink(target: target, selector: #selector(FrameTarget.step))
        #endif
        link.add(to: .main, forMode: .common)
        displayLink = link
    }

    fileprivate func step() {
        guard let game, let link = displayLink, !paused else { return }
        let now = link.timestamp
        let elapsed = lastFrame.map { min(max((now - $0) * 1000, 0), 100) } ?? 16
        lastFrame = now
        guard Int(core.api.reallive_step!(game, UInt32(elapsed.rounded()))) == VNSIGLUS_RUNNING else {
            return finish(byGame: true)
        }
        var width: UInt32 = 0, height: UInt32 = 0
        guard let pixels = core.api.reallive_frame!(game, &width, &height), width > 0, height > 0 else { return }
        frame = CGSize(width: Int(width), height: Int(height))
        frameView.present(pixels, width: Int(width), height: Int(height))
    }

    private func deliver(_ input: SiglusInput) {
        guard let game else { return }
        switch input {
        case let .key(key, down, _):
            core.api.reallive_key!(game, key, down ? 1 : 0)
        case let .button(button, down):
            core.api.reallive_button!(game, button, down ? 1 : 0)
        case let .pointer(location):
            core.api.reallive_pointer!(game, Int32((location.x * frame.width).rounded(.down)),
                                       Int32((location.y * frame.height).rounded(.down)))
        case let .wheel(notches):
            core.api.reallive_wheel!(game, notches)
        }
    }

    private func finish(byGame: Bool) {
        guard !stopped else { return }
        stopped = true
        displayLink?.invalidate()
        displayLink = nil
        if let game {
            core.api.reallive_close!(game)
            self.game = nil
        }
        SiglusEngineLock.inUse = false
        siglusLog.info("RealLive \(self.launch.game.id.rawValue, privacy: .public) stopped")
        if byGame { launch.onExit() }
    }

    // MARK: EngineSession

    func send(_ event: ControllerEvent) {
        guard game != nil, !paused else { return }
        // The hub's pointer events are normalized over the view, which has bars beside or above the frame.
        if case let .pointer(pointer) = event {
            let location = frameView.framePoint(fromViewNormalized: pointer.location)
            router.translate(.pointer(PointerEvent(phase: pointer.phase, location: location, button: pointer.button)))
                .forEach(deliver)
            return
        }
        router.translate(event).forEach(deliver)
    }

    func type(_ typed: TypedText) {
        guard let game, case let .text(text) = typed else {
            for key in KeyboardKey.keys(typing: typed) {
                send(.key(key, pressed: true))
                send(.key(key, pressed: false))
            }
            return
        }
        core.api.reallive_text!(game, text)
    }

    func setPaused(_ paused: Bool) {
        self.paused = paused
        if paused { router.releaseAll().forEach(deliver) }
        lastFrame = nil
        if let game { core.api.reallive_set_paused!(game, paused ? 1 : 0) }
        if !paused { frameView.takeFocus() }
    }

    func terminate() {
        finish(byGame: false)
    }
}

/// A display link keeps its target, so the session sits behind a weak link to avoid a cycle.
private final class FrameTarget: NSObject {
    private weak var session: RealLiveSession?

    init(_ session: RealLiveSession) {
        self.session = session
    }

    @objc func step() { session?.step() }
}
