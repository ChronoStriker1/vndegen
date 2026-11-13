@preconcurrency import AVFoundation
import OSLog
import Synchronization

nonisolated let frameHostLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "framehost")

/// Plays sound a runtime mixes itself: AVAudioEngine pulls interleaved stereo float samples from `render` on its audio
/// thread. The render function must be safe to call there, as the runtimes' C mixers are. Nonisolated, so the render
/// block it builds isn't tied to the main actor.
nonisolated public final class PCMAudioOutput: @unchecked Sendable {
    /// Fills `frames` interleaved stereo frames at `out`.
    public typealias Render = @Sendable (_ out: UnsafeMutablePointer<Float>, _ frames: Int) -> Void

    let engine = AVAudioEngine()
    private let source: AVAudioSourceNode
    private let state = Mutex(State.idle)
    private var observers: [any NSObjectProtocol] = []

    private enum State {
        case idle, playing, paused, stopped
    }

    public init(sampleRate: Double = 44100, render: @escaping Render) {
        let format = AVAudioFormat(standardFormatWithSampleRate: sampleRate, channels: 2)!
        let scratch = Scratch(frames: 8192)
        source = AVAudioSourceNode(format: format) { _, _, frameCount, buffers -> OSStatus in
            let frames = min(Int(frameCount), scratch.frames)
            render(scratch.samples, frames)
            // The node's format is non-interleaved: one buffer per channel.
            let list = UnsafeMutableAudioBufferListPointer(buffers)
            for (channel, buffer) in list.enumerated() where channel < 2 {
                guard let out = buffer.mData?.assumingMemoryBound(to: Float.self) else { continue }
                for i in 0..<frames { out[i] = scratch.samples[i * 2 + channel] }
                for i in frames..<Int(frameCount) { out[i] = 0 }
            }
            return noErr
        }
        engine.attach(source)
        engine.connect(source, to: engine.mainMixerNode, format: format)
        let center = NotificationCenter.default
        observers.append(center.addObserver(forName: .AVAudioEngineConfigurationChange, object: engine, queue: nil) { [weak self] _ in
            self?.restartIfPlaying()
        })
        #if os(iOS)
        observers.append(center.addObserver(forName: AVAudioSession.interruptionNotification, object: nil, queue: nil) { [weak self] note in
            let type = (note.userInfo?[AVAudioSessionInterruptionTypeKey] as? UInt).flatMap(AVAudioSession.InterruptionType.init)
            if type == .ended { self?.restartIfPlaying() }
        })
        #endif
    }

    deinit {
        observers.forEach(NotificationCenter.default.removeObserver)
    }

    private func restartIfPlaying() {
        guard state.withLock({ $0 == .playing }), !engine.isRunning else { return }
        do {
            try engine.start()
        } catch {
            frameHostLog.error("audio did not restart: \(error.localizedDescription, privacy: .public)")
        }
    }

    public func start() {
        do {
            #if os(iOS)
            try AVAudioSession.sharedInstance().setCategory(.playback, mode: .default)
            try AVAudioSession.sharedInstance().setActive(true)
            #endif
            try engine.start()
            state.withLock { $0 = .playing }
        } catch {
            frameHostLog.error("audio did not start: \(error.localizedDescription, privacy: .public)")
        }
    }

    public func pause() {
        state.withLock { if $0 == .playing { $0 = .paused } }
        engine.pause()
    }

    public func resume() {
        state.withLock { if $0 != .stopped { $0 = .playing } }
        guard !engine.isRunning else { return }
        try? engine.start()
    }

    public func stop() {
        state.withLock { $0 = .stopped }
        engine.stop()
    }
}

/// A buffer the audio thread renders into, allocated once.
nonisolated private final class Scratch: @unchecked Sendable {
    let frames: Int
    let samples: UnsafeMutablePointer<Float>

    init(frames: Int) {
        self.frames = frames
        samples = .allocate(capacity: frames * 2)
        samples.initialize(repeating: 0, count: frames * 2)
    }

    deinit { samples.deallocate() }
}
