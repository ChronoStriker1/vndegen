import AVFoundation
import CArtemis
import Foundation

/// Plays the music, sound effects, and voices the core asks for in its media commands. Artemis games ship Ogg Vorbis,
/// which stb_vorbis decodes, and sometimes WAV. Each sound plays on a player node of its own, mixed by AVAudioEngine.
final class ArtemisAudio {
    /// A file the game names, as its bytes, or nil when it has none of that name.
    private let read: (String) -> Data?
    /// Called when a sound that doesn't loop ends, with its id, so the core can end a wait on it.
    var onFinished: ((String) -> Void)?

    private let engine = AVAudioEngine()
    private var music: Channel?
    private var sounds: [String: Channel] = [:]
    /// Volumes the game's settings set, by channel: "bgm", "se", "voice".
    private var volumes: [String: Float] = [:]
    private var decoded: [String: AVAudioPCMBuffer] = [:]
    private var decodedOrder: [String] = []
    private var paused = false

    /// Decoded sounds kept for reuse, such as a game's button sounds.
    private static let cacheSize = 48

    init(read: @escaping (String) -> Data?) {
        self.read = read
    }

    deinit { engine.stop() }

    /// One sound playing, with its gain fading toward `target` over the frames left.
    private final class Channel {
        let player = AVAudioPlayerNode()
        let kind: String
        var gain: Float
        var target: Float
        var fadeLeft: Double = 0
        var stopWhenFaded = false

        init(kind: String, gain: Float) {
            self.kind = kind
            self.gain = gain
            target = gain
        }
    }

    /// Runs one media command from the core: its kind, such as `audio_bgm_play`, and its payload.
    func handle(_ kind: String, _ payload: [String: Any]) {
        switch kind {
        case "audio_set_volume":
            if let channel = payload["channel"] as? String, let value = payload["value"] as? Double {
                volumes[channel] = Float(value)
                applyVolumes()
            }
        case "audio_bgm_play", "audio_bgm_crossfade":
            let fade = Self.milliseconds(payload["fade_ms"] ?? payload["time_ms"])
            stopMusic(fade: kind == "audio_bgm_crossfade" ? fade : 0)
            music = play(payload, kind: "bgm", id: nil, fade: fade)
        case "audio_bgm_stop":
            stopMusic(fade: Self.milliseconds(payload["fade_ms"]))
        case "audio_bgm_fade":
            if let music { fade(music, to: Self.gain(payload["gain"]), over: Self.milliseconds(payload["time_ms"])) }
        case "audio_bgm_pan":
            music?.player.pan = Self.pan(payload["pan"])
        case "audio_se_play", "audio_voice_play":
            guard let id = payload["id"] as? String else { return }
            sounds.removeValue(forKey: id).map { stop($0) }
            sounds[id] = play(payload, kind: kind == "audio_se_play" ? "se" : "voice", id: id,
                              fade: Self.milliseconds(payload["fade_ms"]))
        case "audio_se_stop":
            guard let id = payload["id"] as? String, let sound = sounds[id] else { return }
            let fade = Self.milliseconds(payload["fade_ms"])
            if fade > 0 {
                self.fade(sound, to: 0, over: fade)
                sound.stopWhenFaded = true
            } else {
                sounds[id] = nil
                stop(sound)
            }
        case "audio_se_fade":
            if let id = payload["id"] as? String, let sound = sounds[id] {
                fade(sound, to: Self.gain(payload["gain"]), over: Self.milliseconds(payload["time_ms"]))
            }
        case "audio_se_pan":
            if let id = payload["id"] as? String { sounds[id]?.player.pan = Self.pan(payload["pan"]) }
        case "audio_stop_all":
            stopMusic(fade: 0)
            sounds.values.forEach(stop)
            sounds = [:]
        default:
            break
        }
    }

    /// Moves fades on by `milliseconds`, once a frame.
    func advance(_ milliseconds: Double) {
        for channel in [music].compactMap({ $0 }) + Array(sounds.values) where channel.fadeLeft > 0 {
            let step = min(1, milliseconds / channel.fadeLeft)
            channel.gain += (channel.target - channel.gain) * Float(step)
            channel.fadeLeft = max(0, channel.fadeLeft - milliseconds)
            applyVolume(channel)
            if channel.fadeLeft == 0, channel.stopWhenFaded { remove(channel) }
        }
    }

    func setPaused(_ paused: Bool) {
        self.paused = paused
        if paused { engine.pause() } else { startEngine() }
    }

    func stopAll() {
        handle("audio_stop_all", [:])
        engine.stop()
    }

    // MARK: Playing

    private func play(_ payload: [String: Any], kind: String, id: String?, fade: Double) -> Channel? {
        guard let file = payload["resolved_file"] as? String ?? payload["file"] as? String,
              let buffer = buffer(for: file) else { return nil }
        let loops = payload["loop"] as? Bool ?? false
        let target = Self.gain(payload["gain"])
        let channel = Channel(kind: kind, gain: fade > 0 ? 0 : target)
        if fade > 0 { self.fade(channel, to: target, over: fade) }
        engine.attach(channel.player)
        engine.connect(channel.player, to: engine.mainMixerNode, format: buffer.format)
        channel.player.pan = Self.pan(payload["pan"])
        applyVolume(channel)
        // An A-B loop plays its first part once, then loops the second.
        let loopPart = (payload["resolved_loop_file"] as? String ?? payload["loop_file"] as? String).flatMap(self.buffer(for:))
        if let loopPart, loopPart.format == buffer.format {
            channel.player.scheduleBuffer(buffer)
            channel.player.scheduleBuffer(loopPart, at: nil, options: .loops)
        } else if loops {
            channel.player.scheduleBuffer(buffer, at: nil, options: .loops)
        } else {
            channel.player.scheduleBuffer(buffer, completionCallbackType: .dataPlayedBack) { [weak self, weak channel] _ in
                DispatchQueue.main.async {
                    guard let self, let channel else { return }
                    MainActor.assumeIsolated { self.ended(channel, id: id) }
                }
            }
        }
        startEngine()
        channel.player.play()
        return channel
    }

    private func ended(_ channel: Channel, id: String?) {
        guard let id, sounds[id] === channel else { return }
        sounds[id] = nil
        engine.detach(channel.player)
        onFinished?(id)
    }

    private func startEngine() {
        guard !paused, !engine.isRunning else { return }
        try? engine.start()
    }

    private func stopMusic(fade: Double) {
        guard let music else { return }
        self.music = nil
        if fade > 0 {
            self.fade(music, to: 0, over: fade)
            music.stopWhenFaded = true
            sounds["\u{0}music-fading-\(ObjectIdentifier(music).hashValue)"] = music
        } else {
            stop(music)
        }
    }

    private func stop(_ channel: Channel) {
        channel.player.stop()
        engine.detach(channel.player)
    }

    private func remove(_ channel: Channel) {
        if let key = sounds.first(where: { $0.value === channel })?.key { sounds[key] = nil }
        stop(channel)
    }

    private func fade(_ channel: Channel, to gain: Float, over milliseconds: Double) {
        channel.target = gain
        channel.fadeLeft = milliseconds
        if milliseconds <= 0 {
            channel.gain = gain
            applyVolume(channel)
        }
    }

    private func applyVolumes() {
        ([music].compactMap { $0 } + Array(sounds.values)).forEach(applyVolume)
    }

    private func applyVolume(_ channel: Channel) {
        channel.player.volume = channel.gain * (volumes[channel.kind] ?? 1)
    }

    // MARK: Decoding

    private func buffer(for name: String) -> AVAudioPCMBuffer? {
        if let cached = decoded[name] { return cached }
        let candidates = [name, name + ".ogg", name + ".wav"]
        guard let data = candidates.lazy.compactMap(read).first,
              let buffer = Self.vorbis(data) ?? Self.pcm(data) else { return nil }
        decoded[name] = buffer
        decodedOrder.append(name)
        if decodedOrder.count > Self.cacheSize { decoded[decodedOrder.removeFirst()] = nil }
        return buffer
    }

    /// Ogg Vorbis, through stb_vorbis.
    private static func vorbis(_ data: Data) -> AVAudioPCMBuffer? {
        var channels: Int32 = 0, rate: Int32 = 0
        var samples: UnsafeMutablePointer<Int16>?
        let frames = data.withUnsafeBytes { bytes in
            stb_vorbis_decode_memory(bytes.baseAddress?.assumingMemoryBound(to: UInt8.self), Int32(bytes.count),
                                     &channels, &rate, &samples)
        }
        guard frames > 0, let samples, channels > 0 else { return nil }
        defer { free(samples) }
        guard let format = AVAudioFormat(standardFormatWithSampleRate: Double(rate), channels: AVAudioChannelCount(channels)),
              let buffer = AVAudioPCMBuffer(pcmFormat: format, frameCapacity: AVAudioFrameCount(frames)),
              let out = buffer.floatChannelData else { return nil }
        buffer.frameLength = AVAudioFrameCount(frames)
        let count = Int(channels)
        for frame in 0..<Int(frames) {
            for channel in 0..<count {
                out[channel][frame] = Float(samples[frame * count + channel]) / 32768
            }
        }
        return buffer
    }

    /// Any other format AVFoundation reads, such as WAV, through a temporary file.
    private static func pcm(_ data: Data) -> AVAudioPCMBuffer? {
        let url = FileManager.default.temporaryDirectory.appending(path: "artemis-\(UUID().uuidString)")
        guard (try? data.write(to: url)) != nil else { return nil }
        defer { try? FileManager.default.removeItem(at: url) }
        guard let file = try? AVAudioFile(forReading: url),
              let buffer = AVAudioPCMBuffer(pcmFormat: file.processingFormat, frameCapacity: AVAudioFrameCount(file.length)),
              (try? file.read(into: buffer)) != nil else { return nil }
        return buffer
    }

    // MARK: Payload values

    /// Artemis gains run from 0 to 1000.
    private static func gain(_ value: Any?) -> Float {
        guard let gain = value as? Double else { return 1 }
        return Float(max(0, min(1000, gain))) / 1000
    }

    /// Artemis pans run from -1000, left, to 1000.
    private static func pan(_ value: Any?) -> Float {
        guard let pan = value as? Double else { return 0 }
        return Float(max(-1000, min(1000, pan))) / 1000
    }

    private static func milliseconds(_ value: Any?) -> Double {
        max(0, value as? Double ?? 0)
    }
}
