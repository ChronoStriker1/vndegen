import Foundation
import VNCore
import WebRuntime

extension FlashPlugin {
    /// Flash shared objects, as `.sol` files under `SharedObjects/` in the app's data directory for the game, named by
    /// the movie's path in the game folder. On the PC, Flash Player keeps the same files under
    /// `#SharedObjects/<id>/localhost/` in `%APPDATA%/Macromedia/Flash Player`, named by the movie's whole path there.
    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        let movies = FlashGame(folder: folder)?.movies ?? [:]
        return [SaveLocation(directory: Self.savesDirectory(dataDirectory)) { Self.savePath($0, movies: movies) }]
    }

    /// The player picks which entry starts the game, as the game page's advanced option.
    public func variants(for game: Game, folder: URL, dataDirectory: URL) -> EngineVariants? {
        guard let flash = FlashGame.cached(folder: folder), flash.choices.count > 1 else { return nil }
        return EngineVariants(
            title: "Start with",
            options: flash.choices.map { .init(id: $0.path, title: $0.path) },
            selected: Self.chosenEntry(dataDirectory: dataDirectory),
            automatic: flash.main.path)
    }

    public func selectVariant(_ id: String?, for game: Game, dataDirectory: URL) throws {
        let file = Self.settingsFile(dataDirectory)
        guard let id else {
            do { try FileManager.default.removeItem(at: file) } catch CocoaError.fileNoSuchFile {}
            return
        }
        try FileManager.default.createDirectory(at: dataDirectory, withIntermediateDirectories: true)
        try JSONEncoder().encode(Settings(entry: id)).write(to: file, options: .atomic)
    }

    /// The entry the player picked, while the game still has it, or the automatic one.
    static func entry(for game: FlashGame, dataDirectory: URL) -> FlashGame.Entry {
        let chosen = chosenEntry(dataDirectory: dataDirectory)
        return game.entries.first { $0.path == chosen } ?? game.main
    }

    static func savesDirectory(_ dataDirectory: URL) -> URL {
        dataDirectory.appendingPathComponent("SharedObjects", isDirectory: true)
    }

    static func storage(_ dataDirectory: URL) -> SharedObjectStore {
        SharedObjectStore(directory: savesDirectory(dataDirectory),
                          others: WebStorage(directory: dataDirectory.appendingPathComponent("localStorage", isDirectory: true)))
    }

    /// Maps a save's path from a backup to its path under `SharedObjects/` here:
    /// - Flash Player's `.../#SharedObjects/<id>/localhost/Games/Test Game 01/game.swf/save.sol` is `game.swf/save.sol`,
    ///   from the longest end of the movie's PC path that is a movie in the game folder. A save made for a local path of
    ///   `/`, `.../localhost/save.sol`, is `save.sol`. Movies run from a network drive keep theirs under `#localWithNet`.
    /// - Ruffle on the PC keeps the same layout under `.../SharedObjects/localhost/`.
    /// - VNDegen on another device keeps it under `.../SharedObjects/`.
    /// Nil for any other file, and for a save of a movie this game doesn't have.
    static func savePath(_ path: String, movies: [String: String]) -> String? {
        let parts = path.split(separator: "/").map(String.init)
        guard let file = parts.last, file.lowercased().hasSuffix(".sol") else { return nil }
        let isHost = { (name: String) in ["localhost", "#localwithnet"].contains(name.lowercased()) }
        if let marker = parts.lastIndex(where: { $0.lowercased() == "#sharedobjects" }), marker + 2 < parts.count - 1, isHost(parts[marker + 2]) {
            return movieSave(Array(parts[(marker + 3)...]), movies: movies)
        }
        guard let marker = parts.lastIndex(of: "SharedObjects"), marker < parts.count - 1 else { return nil }
        if marker + 1 < parts.count - 1, isHost(parts[marker + 1]) {
            return movieSave(Array(parts[(marker + 2)...]), movies: movies)
        }
        let relative = parts[(marker + 1)...]
        guard relative.allSatisfy({ !$0.hasPrefix(".") }) else { return nil }
        return relative.joined(separator: "/")
    }

    /// `["Games", "Test Game 01", "game.swf", "save.sol"]` is `game.swf/save.sol` when the game has `game.swf`. Parts
    /// after the movie, from a shared object name with slashes, stay as they are.
    private static func movieSave(_ parts: [String], movies: [String: String]) -> String? {
        guard parts.allSatisfy({ !$0.isEmpty && $0 != "." && $0 != ".." }) else { return nil }
        if parts.count == 1 { return parts[0] }
        let folders = parts.dropLast()
        for end in stride(from: folders.count, to: 0, by: -1) {
            for start in 0 ..< end {
                guard let movie = movies[folders[start ..< end].joined(separator: "/").lowercased()] else { continue }
                return ([movie] + folders[end...] + [parts.last!]).joined(separator: "/")
            }
        }
        return nil
    }

    private struct Settings: Codable {
        var entry: String
    }

    private static func settingsFile(_ dataDirectory: URL) -> URL {
        dataDirectory.appendingPathComponent("flash.json")
    }

    private static func chosenEntry(dataDirectory: URL) -> String? {
        guard let data = try? Data(contentsOf: settingsFile(dataDirectory)) else { return nil }
        return (try? JSONDecoder().decode(Settings.self, from: data))?.entry
    }
}
