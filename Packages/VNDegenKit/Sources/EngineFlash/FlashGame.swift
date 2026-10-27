import Foundation
import Synchronization
import VNCore
import WebRuntime

/// What a Flash game folder holds that VNDegen can start: Windows projectors with their movie appended, SWF movies, and
/// HTML pages that launch movies. Read from the folder once, for detection, the game page, and each launch.
nonisolated struct FlashGame: Equatable, Sendable {
    nonisolated enum Entry: Equatable, Sendable {
        /// A Flash projector, `Game.exe`, with the movie at `movie` in it. The page loads the movie from the projector's
        /// path, so movies it loads and its saves are named as on the PC.
        case projector(path: String, movie: FileSlice)
        /// A SWF movie.
        case movie(path: String)
        /// An HTML page that plays movies with `<embed>` or `<object>`, or links to them, as a collection's menu does.
        case page(path: String)

        /// Relative to the game folder, with its case on disk.
        var path: String {
            switch self {
            case let .projector(path, _), let .movie(path), let .page(path): path
            }
        }
    }

    /// Every entry, best first. The first is what the game starts with unless the player picks another.
    let entries: [Entry]
    /// Every movie and projector in the folder, lower-cased, mapped to its path with its case on disk. Saves are named
    /// after the movie that made them.
    let movies: [String: String]

    var main: Entry { entries[0] }

    /// How deep detection looks. Archived games often keep the projector a few wrapper folders down.
    static let maxDepth = 5
}

nonisolated extension FlashGame {
    init?(folder: URL) {
        self.init(FolderProbe(scanning: folder, maxDepth: Self.maxDepth))
    }

    /// `init(folder:)`, kept while the folder's modification date stays the same. The game page asks for its entries
    /// whenever it draws.
    static func cached(folder: URL) -> FlashGame? {
        let modified = (try? folder.resourceValues(forKeys: [.contentModificationDateKey]))?.contentModificationDate
        if let hit = cache.withLock({ $0[folder.path] }), hit.modified == modified { return hit.game }
        let game = FlashGame(folder: folder)
        cache.withLock { $0[folder.path] = (modified, game) }
        return game
    }

    private static let cache = Mutex<[String: (modified: Date?, game: FlashGame?)]>([:])

    init?(_ probe: FolderProbe) {
        var projectors: [(path: String, movie: FileSlice)] = [], swfs: [String] = [], pages: [(path: String, kind: PageKind)] = []
        var movies: [String: String] = [:]
        for path in probe.entries.sorted() {
            let url = probe.root.appendingPathComponent(path)
            switch (path as NSString).pathExtension.lowercased() {
            case "exe":
                guard let movie = Self.projectorMovie(url) else { continue }
                projectors.append((path, movie))
                movies[path.lowercased()] = path
            case "swf":
                guard Self.isMovie(url) else { continue }
                swfs.append(path)
                movies[path.lowercased()] = path
            case "html", "htm":
                if let kind = Self.pageKind(url) { pages.append((path, kind)) }
            default:
                continue
            }
        }
        guard !projectors.isEmpty || !swfs.isEmpty else { return nil }

        let depth = { (path: String) in path.split(separator: "/").count }
        let size = { (path: String) in
            (try? probe.root.appendingPathComponent(path).resourceValues(forKeys: [.fileSizeKey]))?.fileSize ?? 0
        }
        // A Mac projector keeps its movie in Game.app/Contents/Resources/movie.swf. It is the same movie as the Windows
        // projector beside it, so it comes last.
        let inMacApp = { (path: String) in path.lowercased().contains(".app/contents/") }
        // A collection's menu, which links to its other pages, comes before the pages it links to.
        let launchers = pages.filter { $0.kind != .wrapper }
            .sorted { ($0.kind == .menu ? 0 : 1, depth($0.path), $0.path) < ($1.kind == .menu ? 0 : 1, depth($1.path), $1.path) }
            .map(\.path)
        let rankedProjectors = projectors.sorted { (depth($0.path), $0.path) < (depth($1.path), $1.path) }
        // The game's own movie is the shallowest, and among those the largest. Small ones beside it are loaders and
        // mods, and deeper ones are scenes it loads.
        let rankedMovies = swfs.sorted {
            (inMacApp($0) ? 1 : 0, depth($0), -size($0), $0) < (inMacApp($1) ? 1 : 0, depth($1), -size($1), $1)
        }
        var entries: [Entry] = launchers.prefix(1).map { .page(path: $0) }
        entries += rankedProjectors.map { .projector(path: $0.path, movie: $0.movie) }
        entries += rankedMovies.map { .movie(path: $0) }
        entries += launchers.dropFirst().map { .page(path: $0) }
        self.entries = entries
        self.movies = movies
    }

    /// The entries the game page offers: the automatic one and the others no deeper than it, since deeper movies are
    /// usually the scenes it loads.
    var choices: [Entry] {
        let depth = main.path.split(separator: "/").count
        return entries.filter { $0 == main || $0.path.split(separator: "/").count <= depth }
    }

    /// The movie a Flash projector carries. Projectors end with the movie's length and the marker 0xFA123456, both
    /// little-endian, right after the movie.
    static func projectorMovie(_ url: URL) -> FileSlice? {
        guard let handle = try? FileHandle(forReadingFrom: url) else { return nil }
        defer { try? handle.close() }
        guard let size = try? handle.seekToEnd(), size > 8 + 8 else { return nil }
        try? handle.seek(toOffset: size - 8)
        guard let tail = try? handle.read(upToCount: 8), tail.count == 8 else { return nil }
        let marker = tail.prefix(4).reduce(UInt32(0)) { $0 >> 8 | UInt32($1) << 24 }
        let length = UInt64(tail.suffix(4).reduce(UInt32(0)) { $0 >> 8 | UInt32($1) << 24 })
        guard marker == 0xFA12_3456, length >= 8, length <= size - 8 else { return nil }
        let offset = size - 8 - length
        try? handle.seek(toOffset: offset)
        guard let header = try? handle.read(upToCount: 3), isMovieSignature(header) else { return nil }
        return FileSlice(offset: offset, length: Int64(length))
    }

    static func isMovie(_ url: URL) -> Bool {
        guard let handle = try? FileHandle(forReadingFrom: url) else { return false }
        defer { try? handle.close() }
        return (try? handle.read(upToCount: 3)).map(isMovieSignature) ?? false
    }

    /// FWS is an uncompressed movie, CWS one compressed with zlib, and ZWS one compressed with LZMA.
    static func isMovieSignature(_ bytes: Data) -> Bool {
        ["FWS", "CWS", "ZWS"].contains(String(decoding: bytes.prefix(3), as: UTF8.self))
    }

    nonisolated enum PageKind: Equatable, Sendable {
        /// Plays one movie, as the page a browser game ships with. The movie itself is the better entry.
        case wrapper
        /// Plays or links to several movies.
        case player
        /// Also links to other pages, as a collection's menu does.
        case menu
    }

    /// What an HTML page does with movies. Nil when it names none.
    static func pageKind(_ url: URL) -> PageKind? {
        guard let handle = try? FileHandle(forReadingFrom: url), let data = try? handle.read(upToCount: 256 << 10) else { return nil }
        try? handle.close()
        let text = String(decoding: data, as: UTF8.self).lowercased()
        let swfs = text.ranges(of: ".swf").count
        guard swfs > 0 else { return nil }
        if text.firstMatch(of: /href\s*=\s*["'][^"'#:]+\.html?["']/) != nil { return .menu }
        return swfs > 1 ? .player : .wrapper
    }
}
