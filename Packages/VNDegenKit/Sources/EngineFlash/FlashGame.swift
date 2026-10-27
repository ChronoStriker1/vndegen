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
    /// Every folder that holds a movie or projector, at any depth, lower-cased, mapped to its path with its case on disk.
    /// A movie may save under one of them rather than under its own path.
    let folders: [String: String]

    var main: Entry { entries[0] }

    /// How deep detection looks. Archived games often keep the projector a few wrapper folders down.
    static let maxDepth = 5
}

nonisolated extension FlashGame {
    init?(folder: URL) {
        self.init(FolderProbe(scanning: folder, maxDepth: Self.maxDepth))
    }

    /// `init(folder:)`, kept for a few seconds while the folder's modification date stays the same, since the game page
    /// asks for its entries whenever it draws. A change deeper in the folder shows once the few seconds have passed.
    static func cached(folder: URL, now: Date = .now) -> FlashGame? {
        let modified = (try? folder.resourceValues(forKeys: [.contentModificationDateKey]))?.contentModificationDate
        if let hit = cache.withLock({ $0[folder.path] }), hit.modified == modified, now.timeIntervalSince(hit.read) < 10 {
            return hit.game
        }
        let game = FlashGame(folder: folder)
        cache.withLock { $0[folder.path] = (modified, now, game) }
        return game
    }

    private static let cache = Mutex<[String: (modified: Date?, read: Date, game: FlashGame?)]>([:])

    init?(_ probe: FolderProbe) {
        var projectors: [(path: String, movie: FileSlice)] = [], swfs: [String] = [], htmlPaths: [String] = []
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
                htmlPaths.append(path)
            default:
                continue
            }
        }
        guard !projectors.isEmpty || !swfs.isEmpty else { return nil }
        // Pages count links to projectors as movies, so they are read once the projectors are known.
        let projectorPaths = Set(projectors.map { $0.path.lowercased() })
        var pages: [String: Page] = [:]
        for path in htmlPaths {
            if let page = Page(probe.root.appendingPathComponent(path), path: path, projectors: projectorPaths) {
                pages[path.lowercased()] = page
            }
        }
        var folders: [String: String] = [:]
        for path in movies.values {
            var parts = path.split(separator: "/").dropLast()
            while !parts.isEmpty {
                folders[parts.joined(separator: "/").lowercased()] = parts.joined(separator: "/")
                parts.removeLast()
            }
        }

        let depth = { (path: String) in path.split(separator: "/").count }
        let size = { (path: String) in
            (try? probe.root.appendingPathComponent(path).resourceValues(forKeys: [.fileSizeKey]))?.fileSize ?? 0
        }
        // A Mac projector keeps its movie in Game.app/Contents/Resources/movie.swf. It is the same movie as the Windows
        // projector beside it, so it comes last.
        let inMacApp = { (path: String) in path.lowercased().contains(".app/contents/") }
        // A collection's menu comes before the pages it links to. Its episode pages often link back to it, so the menu is
        // the page that links to the most pages with movies.
        let launchers = pages.values.compactMap { page in page.kind(among: pages).map { (page, $0) } }
            .filter { $0.1 != .wrapper }
            .map { (path: $0.0.path, menuLinks: $0.0.flashLinks(among: pages)) }
            .sorted { (-$0.menuLinks, depth($0.path), $0.path) < (-$1.menuLinks, depth($1.path), $1.path) }
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
        self.folders = folders
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

    /// What an HTML page names: the movies and projectors it plays or links to, and the pages it links to, as lower-cased
    /// paths in the game folder.
    struct Page: Equatable, Sendable {
        let path: String
        let movies: Set<String>
        let links: Set<String>

        init?(_ url: URL, path: String, projectors: Set<String>) {
            guard let handle = try? FileHandle(forReadingFrom: url), let data = try? handle.read(upToCount: 256 << 10) else { return nil }
            try? handle.close()
            let text = String(decoding: data, as: UTF8.self)
            let folder = path.split(separator: "/").dropLast().map(String.init)
            // A reference is any quoted URL, so <embed src>, <param value>, links, and script strings all count.
            let references = text.matches(of: /["']([^"'<>]+?\.(?i:swf|exe|html?))(?:[?#][^"']*)?["']/).compactMap { match in
                Self.resolve(String(match.1), from: folder)
            }
            self.path = path
            movies = Set(references.filter { $0.hasSuffix(".swf") || projectors.contains($0) })
            links = Set(references.filter { $0.hasSuffix(".html") || $0.hasSuffix(".htm") }.filter { $0 != path.lowercased() })
            guard !movies.isEmpty || !links.isEmpty else { return nil }
        }

        /// A wrapper plays one movie, as a browser game's page does, and the movie itself is the better entry. A player
        /// names several movies. A menu also links to other pages that play movies, as a collection's menu does. Nil for a
        /// page that only links to pages without movies.
        func kind(among pages: [String: Page]) -> PageKind? {
            if flashLinks(among: pages) > 0 { return .menu }
            switch movies.count {
            case 0: return nil
            case 1: return .wrapper
            default: return .player
            }
        }

        /// How many of the pages it links to play movies.
        func flashLinks(among pages: [String: Page]) -> Int {
            links.count { pages[$0].map { !$0.movies.isEmpty } ?? false }
        }

        /// `../flash/a.swf` from `pages/index.html` is `flash/a.swf`. Nil for URLs with a scheme and paths that leave the
        /// game folder.
        static func resolve(_ reference: String, from folder: [String]) -> String? {
            guard !reference.contains(":"), let decoded = reference.removingPercentEncoding ?? Optional(reference) else { return nil }
            var parts = decoded.hasPrefix("/") ? [] : folder
            for part in decoded.split(separator: "/") {
                switch part {
                case ".": continue
                case "..": guard !parts.isEmpty else { return nil }; parts.removeLast()
                default: parts.append(String(part))
                }
            }
            return parts.isEmpty ? nil : parts.joined(separator: "/").lowercased()
        }
    }
}
