import AVFoundation
import CArtemis
import CVNVideo
import Foundation
import QuartzCore

/// The movies a game plays, decoded by vnvideo on threads of their own. A fullscreen movie, which the script waits on,
/// shows over the game. A movie on a layer goes to the core as that layer's texture, frame by frame.
final class ArtemisMovies {
    /// One movie playing. Its clock is its sound when it has sound, so picture and sound stay together, and the time
    /// it has been on screen otherwise.
    private final class Movie {
        let id: String?
        let file: String
        /// The Windows virtual-key codes, mouse buttons included, that skip it.
        let skipKeys: Set<Int32>
        let decoder: MovieDecoder
        let info: vnvideo_info
        var sound: AVAudioSourceNode?
        var elapsed = 0.0
        /// Display times of the frames shown, for the frame rate the log reports.
        var shown: [Double] = []

        init(id: String?, file: String, skipKeys: Set<Int32>, decoder: MovieDecoder) {
            self.id = id
            self.file = file
            self.skipKeys = skipKeys
            self.decoder = decoder
            var info = vnvideo_info()
            vnvideo_get_info(decoder.video, &info)
            self.info = info
        }

        var video: OpaquePointer { decoder.video }
        var time: Double { sound != nil ? vnvideo_audio_time(video) : elapsed }
    }

    private let core: ArtemisCore
    private let files: ArtemisCore.Files
    private let audio: ArtemisAudio
    private let screen: CALayer
    private let log: (String) -> Void
    private var movies: [Movie] = []

    /// `screen` is the layer a fullscreen movie shows in, over the game.
    init(core: ArtemisCore, files: ArtemisCore.Files, audio: ArtemisAudio, screen: CALayer, log: @escaping (String) -> Void) {
        self.core = core
        self.files = files
        self.audio = audio
        self.screen = screen
        self.log = log
    }

    /// Whether a fullscreen movie is playing, which takes the game's input until it ends.
    var playingFullscreen: Bool { movies.contains { $0.id == nil } }

    /// Starts a movie from a `video_play` command. Returns false when it can't play, so the caller ends it at once.
    func play(_ payload: [String: Any]) -> Bool {
        let id = payload["id"] as? String
        guard let file = payload["resolved_file"] as? String ?? payload["file"] as? String else { return false }
        stop(id: id)
        let size = core.api.resources_file_size!(files.resources, file)
        guard size > 0 else {
            log("[W] movie not found: \(file)")
            return false
        }
        let source = MovieSource(read: core.api.resources_read!, resources: files.resources, path: file, owner: files)
        var error = [CChar](repeating: 0, count: 160)
        guard let video = vnvideo_open(MovieSource.read, Unmanaged.passUnretained(source).toOpaque(), UInt64(size),
                                       payload["loop"] as? Bool == true ? 1 : 0, &error, error.count) else {
            log("[W] movie not played: \(file): \(String(decoding: error.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self))")
            return false
        }
        let skipKeys = Set((payload["skip_keys"] as? [Int] ?? []).map(Int32.init))
        let movie = Movie(id: id, file: file, skipKeys: skipKeys, decoder: MovieDecoder(video: video, source: source))
        if movie.info.audio_rate > 0 {
            movie.sound = audio.attachMovieSound(rate: Double(movie.info.audio_rate),
                                                 channels: Int(movie.info.audio_channels), decoder: movie.decoder)
        }
        log("[I] movie \(file): \(movie.info.width)x\(movie.info.height) at \(String(format: "%.2f", movie.info.fps)) fps, "
            + (movie.sound != nil ? "sound \(movie.info.audio_rate) Hz" : "no sound") + (id.map { " on layer \($0)" } ?? "")
            + (id == nil ? ", skipped by keys \(skipKeys.sorted())" : ""))
        movies.append(movie)
        if id == nil {
            screen.contents = nil
            screen.isHidden = false
        }
        return true
    }

    /// Moves the movies on by `milliseconds` and hands out their new frames. Returns the movies that ended, by layer
    /// id, with nil for the fullscreen one, so the core can end its wait or run its handler.
    func advance(_ milliseconds: Double, runtime: OpaquePointer, at timestamp: Double) -> [String?] {
        var ended: [String?] = []
        for movie in movies {
            movie.elapsed += milliseconds / 1000
            let time = movie.time
            var changed: Int32 = 0
            if let pixels = vnvideo_frame(movie.video, time, &changed), changed != 0 {
                movie.shown.append(timestamp)
                if !show(pixels, of: movie, runtime: runtime) {
                    // The game removed the movie's layer.
                    remove(movie, reason: "layer gone")
                    continue
                }
            }
            if let error = vnvideo_error(movie.video) { log("[W] movie \(movie.file): \(String(cString: error))") }
            if vnvideo_ended(movie.video, time) != 0 {
                remove(movie, reason: "ended")
                ended.append(movie.id)
            }
        }
        return ended
    }

    /// Ends the fullscreen movie early when `key`, a virtual-key code or mouse button, is one that skips it on the PC.
    /// Returns whether it was skipped.
    func skip(key: Int32) -> Bool {
        guard let movie = movies.first(where: { $0.id == nil }), movie.skipKeys.contains(key) else { return false }
        remove(movie, reason: "skipped")
        return true
    }

    func stopAll() {
        movies.forEach { remove($0, reason: "stopped") }
    }

    private func stop(id: String?) {
        movies.filter { $0.id == id }.forEach { remove($0, reason: "replaced") }
    }

    private func remove(_ movie: Movie, reason: String) {
        movies.removeAll { $0 === movie }
        if let sound = movie.sound { audio.detachMovieSound(sound) }
        if movie.id == nil {
            screen.isHidden = true
            screen.contents = nil
        }
        let sound = movie.sound != nil ? String(format: ", %.2f s of sound played", vnvideo_audio_time(movie.video)) : ""
        log("[I] movie \(movie.file) \(reason)" + Self.frameRate(movie.shown) + sound)
    }

    /// The average and 1% low frame rate of the frames shown, from their display times.
    private static func frameRate(_ times: [Double]) -> String {
        guard times.count > 2, let first = times.first, let last = times.last, last > first else { return "" }
        let gaps = zip(times.dropFirst(), times).map { $0 - $1 }.sorted(by: >)
        let worst = gaps.prefix(max(1, gaps.count / 100))
        let low = Double(worst.count) / worst.reduce(0, +)
        return String(format: ", %d frames over %.2f s, %.2f fps average, %.2f fps 1%% low", times.count, last - first,
                      Double(gaps.count) / (last - first), low)
    }

    private func show(_ pixels: UnsafePointer<UInt8>, of movie: Movie, runtime: OpaquePointer) -> Bool {
        let width = Int(movie.info.width), height = Int(movie.info.height), length = width * height * 4
        guard let id = movie.id else {
            screen.contents = Self.image(pixels, width: width, height: height, length: length)
            return true
        }
        return id.withCString {
            core.api.runtime_upload_video_layer_frame!(runtime, $0, UInt32(width), UInt32(height), pixels, length)
        } != 0
    }

    private static func image(_ pixels: UnsafePointer<UInt8>, width: Int, height: Int, length: Int) -> CGImage? {
        guard let provider = CGDataProvider(data: Data(bytes: pixels, count: length) as CFData) else { return nil }
        return CGImage(width: width, height: height, bitsPerComponent: 8, bitsPerPixel: 32, bytesPerRow: width * 4,
                       space: CGColorSpace(name: CGColorSpace.sRGB)!, bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.noneSkipLast.rawValue),
                       provider: provider, decode: nil, shouldInterpolate: true, intent: .defaultIntent)
    }
}

/// A movie file vnvideo reads on its decode thread, through the core's file lookup, which locks around each read.
nonisolated final class MovieSource: @unchecked Sendable {
    private let readFile: ArtResourcesReadFn
    private let resources: OpaquePointer
    private let path: [CChar]
    /// Keeps the mounted files alive while the decode thread reads them.
    private let files: AnyObject

    init(read: ArtResourcesReadFn, resources: OpaquePointer, path: String, owner: AnyObject) {
        readFile = read
        self.resources = resources
        self.path = Array(path.utf8CString)
        files = owner
    }

    static let read: vnvideo_read_fn = { user, offset, buffer, length in
        guard let user else { return -1 }
        let source = Unmanaged<MovieSource>.fromOpaque(user).takeUnretainedValue()
        return source.path.withUnsafeBufferPointer {
            source.readFile(source.resources, $0.baseAddress, offset, buffer?.assumingMemoryBound(to: UInt8.self), length)
        }
    }
}

/// A movie's decoder, closed when the last of its users lets go: the movie, and the render block of its sound, which
/// the audio thread can still be running when the movie ends.
nonisolated final class MovieDecoder: @unchecked Sendable {
    let video: OpaquePointer
    /// The file the decode thread reads, kept until the decoder closes.
    private let source: MovieSource

    init(video: OpaquePointer, source: MovieSource) {
        self.video = video
        self.source = source
    }

    deinit { vnvideo_close(video) }
}
