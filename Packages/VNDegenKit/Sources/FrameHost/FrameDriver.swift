import Foundation
import QuartzCore

public final class FrameDriver {
    public typealias AudioRender = @Sendable (OpaquePointer, UnsafeMutablePointer<Float>, Int32) -> Void

    public let view = FrameView()
    public private(set) var engine: OpaquePointer?
    public private(set) var paused = false
    public private(set) var stopped = false
    public private(set) var failedToStart = false
    public private(set) var startFailureReason: String?
    public var holdsRuntime: (OpaquePointer) -> Bool = { _ in false }
    public var willFinish: () -> Void = {}
    #if DEBUG
    public let pacing: FramePacing?
    #endif
    private let setRuntimePaused: (OpaquePointer, Bool) -> Void
    private let close: (OpaquePointer) -> Void
    private let onExit: @MainActor () -> Void
    private var audio: PCMAudioOutput?
    private let displayLink = DisplayLinkDriver()
    private var clock = MillisecondClock()
    private var stopWaiters: [CheckedContinuation<Void, Never>] = []

    public init(pacingTag: String, setPaused: @escaping (OpaquePointer, Bool) -> Void, close: @escaping (OpaquePointer) -> Void,
                onExit: @escaping @MainActor () -> Void) {
        setRuntimePaused = setPaused
        self.close = close
        self.onExit = onExit
        #if DEBUG
        pacing = FramePacing(tag: pacingTag)
        #endif
    }

    public var canStart: Bool { engine == nil && !stopped }

    public func run(_ engine: OpaquePointer, sampleRate: Double = 44100, audio render: @escaping AudioRender,
                    step: @escaping (OpaquePointer, Int, CADisplayLink) -> Void) {
        self.engine = engine
        let box = EngineBox(engine)
        audio = PCMAudioOutput(sampleRate: sampleRate) { out, frames in render(box.engine, out, Int32(frames)) }
        audio?.start()
        view.takeFocus()
        displayLink.start(in: view) { [weak self] link in self?.tick(link, step) }
    }

    private func tick(_ link: CADisplayLink, _ step: (OpaquePointer, Int, CADisplayLink) -> Void) {
        guard let engine else { return }
        let ms = clock.tick(at: link.timestamp)
        guard !paused, !holdsRuntime(engine) else { return }
        step(engine, ms, link)
    }

    public func failToStart(_ reason: String?) {
        failedToStart = true
        startFailureReason = reason
        finish(byGame: true)
    }

    public func setPaused(_ paused: Bool, before: (OpaquePointer) -> Void = { _ in }) {
        self.paused = paused
        guard let engine else { return }
        before(engine)
        applyPause(engine)
        if !paused { view.takeFocus() }
    }

    public func holdChanged() {
        guard let engine, !paused else { return }
        applyPause(engine)
    }

    private func applyPause(_ engine: OpaquePointer) {
        let stop = paused || holdsRuntime(engine)
        setRuntimePaused(engine, stop)
        if stop { audio?.pause() } else { audio?.resume() }
        clock.reset()
        #if DEBUG
        pacing?.reset()
        #endif
    }

    public func finish(byGame: Bool) {
        willFinish()
        displayLink.stop()
        audio?.stop()
        audio = nil
        if let engine {
            close(engine)
            self.engine = nil
        }
        stopped = true
        let waiters = stopWaiters
        stopWaiters = []
        waiters.forEach { $0.resume() }
        if byGame { onExit() }
    }

    public func terminate() {
        guard !stopped else { return }
        if engine == nil { failedToStart = true }
        finish(byGame: false)
    }

    public func waitUntilStopped() async {
        if stopped { return }
        await withCheckedContinuation { stopWaiters.append($0) }
    }
}

nonisolated final class EngineBox: @unchecked Sendable {
    let engine: OpaquePointer
    init(_ engine: OpaquePointer) { self.engine = engine }
}
