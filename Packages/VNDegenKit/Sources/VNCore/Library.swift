import Foundation
import Observation

/// Where the library lives on disk.
public struct LibraryPaths: Sendable {
    /// Folder the user copies games into, one subfolder per game.
    public let gamesRoot: URL
    /// App-owned state: library.json, artwork, per-game engine data.
    public let support: URL

    public init(gamesRoot: URL, support: URL) {
        self.gamesRoot = gamesRoot
        self.support = support
    }

    /// iOS: Documents/Games, visible in the Files app. macOS: ~/Games/VNDegen, outside the folders iCloud Drive can sync.
    public static var standard: LibraryPaths {
        let fm = FileManager.default
        let appSupport = fm.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        #if os(macOS)
        let games = fm.homeDirectoryForCurrentUser.appendingPathComponent("Games/VNDegen", isDirectory: true)
        #else
        let games = fm.urls(for: .documentDirectory, in: .userDomainMask)[0].appendingPathComponent("Games", isDirectory: true)
        #endif
        return LibraryPaths(gamesRoot: games, support: appSupport.appendingPathComponent("VNDegen", isDirectory: true))
    }

    public var libraryFile: URL { support.appendingPathComponent("library.json") }
    public func folder(for id: GameID) -> URL { gamesRoot.appendingPathComponent(id.rawValue, isDirectory: true) }
    public func artworkDirectory(for id: GameID) -> URL {
        support.appendingPathComponent("Artwork/\(id.rawValue)", isDirectory: true)
    }
    public func dataDirectory(for id: GameID) -> URL {
        support.appendingPathComponent("GameData/\(id.rawValue)", isDirectory: true)
    }
    /// Runtime packages such as RPG Maker RTPs, one folder each, shared by every game.
    public var runtimesDirectory: URL { support.appendingPathComponent("Runtimes", isDirectory: true) }
}

/// The game library. Folders on disk decide which games exist. library.json keeps everything the user or metadata added,
/// so a game that is removed and copied back keeps its artwork, engine override, and lock.
@MainActor
@Observable
public final class LibraryStore {
    public let paths: LibraryPaths
    public let engines: [any EnginePlugin]
    /// Games whose folder is present, most recently played first, then by title.
    public private(set) var games: [Game] = []
    private var records: [GameID: Game] = [:]

    public init(paths: LibraryPaths = .standard, engines: [any EnginePlugin]) {
        self.paths = paths
        self.engines = engines
        records = (try? Self.load(paths.libraryFile)) ?? [:]
    }

    public func plugin(for id: EngineID?) -> (any EnginePlugin)? {
        guard let id else { return nil }
        return engines.first { $0.descriptor.id == id }
    }

    public func game(_ id: GameID) -> Game? { records[id] }

    /// Every game the library remembers, including those whose folder is missing.
    public var allGames: [Game] { Array(records.values) }

    /// Rescans the games folder and re-runs detection on every present game.
    public func refresh() async {
        let paths = paths, engines = engines
        let scanned: [(GameID, EngineMatch?)] = await Task.detached(priority: .userInitiated) {
            try? FileManager.default.createDirectory(at: paths.gamesRoot, withIntermediateDirectories: true)
            let names = (try? FileManager.default.contentsOfDirectory(atPath: paths.gamesRoot.path)) ?? []
            return names.filter { name in
                var isDir: ObjCBool = false
                return !name.hasPrefix(".")
                    && FileManager.default.fileExists(atPath: paths.gamesRoot.appendingPathComponent(name).path, isDirectory: &isDir)
                    && isDir.boolValue
            }.map { name in
                let probe = FolderProbe(scanning: paths.gamesRoot.appendingPathComponent(name, isDirectory: true))
                return (GameID(name), EngineDetector.detect(probe, using: engines))
            }
        }.value

        for (id, match) in scanned {
            records[id, default: Game(id: id)].detected = match
        }
        let present = Set(scanned.map(\.0))
        games = records.values.filter { present.contains($0.id) }.sorted(by: Self.libraryOrder)
        save()
    }

    public func update(_ id: GameID, _ mutate: (inout Game) -> Void) {
        guard var game = records[id] else { return }
        mutate(&game)
        records[id] = game
        if let index = games.firstIndex(where: { $0.id == id }) {
            games[index] = game
            games.sort(by: Self.libraryOrder)
        }
        save()
    }

    public func artworkURL(_ slot: ArtworkSlot, for id: GameID) -> URL? {
        guard let name = records[id]?.artwork[slot] else { return nil }
        return paths.artworkDirectory(for: id).appendingPathComponent(name)
    }

    /// Stores image bytes for a slot, replacing what was there.
    public func setArtwork(_ data: Data, fileExtension: String, slot: ArtworkSlot, for id: GameID) throws {
        let dir = paths.artworkDirectory(for: id)
        try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        // A fresh name per write so image caches keyed on URL see the change.
        let name = "\(slot.rawValue)-\(UUID().uuidString.prefix(8)).\(fileExtension)"
        try data.write(to: dir.appendingPathComponent(name), options: .atomic)
        if let old = records[id]?.artwork[slot] {
            try? FileManager.default.removeItem(at: dir.appendingPathComponent(old))
        }
        update(id) { $0.artwork[slot] = name }
    }

    public func makeLaunch(_ id: GameID, onExit: @escaping @MainActor () -> Void) throws -> GameLaunch? {
        guard let game = records[id] else { return nil }
        let data = paths.dataDirectory(for: id)
        try FileManager.default.createDirectory(at: data, withIntermediateDirectories: true)
        return GameLaunch(game: game, folder: paths.folder(for: id), dataDirectory: data, onExit: onExit)
    }

    private static func libraryOrder(_ a: Game, _ b: Game) -> Bool {
        switch (a.lastPlayedAt, b.lastPlayedAt) {
        case let (x?, y?) where x != y: return x > y
        case (_?, nil): return true
        case (nil, _?): return false
        default: return a.title.localizedStandardCompare(b.title) == .orderedAscending
        }
    }

    private func save() {
        do {
            try FileManager.default.createDirectory(at: paths.support, withIntermediateDirectories: true)
            let encoder = JSONEncoder()
            encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
            encoder.dateEncodingStrategy = .iso8601
            try encoder.encode(Array(records.values)).write(to: paths.libraryFile, options: .atomic)
        } catch {
            assertionFailure("library save failed: \(error)")
        }
    }

    private static func load(_ url: URL) throws -> [GameID: Game] {
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        let games = try decoder.decode([Game].self, from: Data(contentsOf: url))
        return Dictionary(games.map { ($0.id, $0) }, uniquingKeysWith: { a, _ in a })
    }
}
