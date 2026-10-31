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
    /// The games folders the player added, beside `gamesRoot`.
    public var foldersFile: URL { support.appendingPathComponent("folders.json") }
    /// A game's folder in `gamesRoot`. `LibraryStore.folder(for:)` finds a game in any library folder.
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


/// The game library. Folders on disk decide which games exist: each subfolder of the built-in games folder and of every
/// folder the player added is a game. library.json keeps everything the user or metadata added, so a game that is
/// removed and copied back keeps its artwork, engine override, and lock.
@MainActor
@Observable
public final class LibraryStore {
    public let paths: LibraryPaths
    public let engines: [any EnginePlugin]
    /// Games whose folder is present, or was when its library folder was last reached, most recently played first, then
    /// by title.
    public private(set) var games: [Game] = []
    /// The games folders the player added, in the order they were added. The built-in folder, `paths.gamesRoot`, isn't
    /// among them.
    public private(set) var folders: [LibraryFolder] = []
    /// Why each added folder the last scan couldn't reach is unavailable. Its games stay in `games`.
    public private(set) var folderProblems: [LibraryFolderID: String] = [:]
    private var records: [GameID: Game] = [:]
    /// Folders the player removed. Their games' records still name them.
    private var removed: [LibraryFolder] = []
    /// Where each added folder resolved to. A security scope opened there stays open from then until the player removes
    /// the folder or the app quits, rather than around each scan and launch, because the game page, runtime package
    /// checks, save sync, and Open in Files read game folders too.
    @ObservationIgnored private var resolved: [LibraryFolderID: FolderAccess.Resolved] = [:]
    @ObservationIgnored private var refreshing: Task<Void, Never>?
    /// A deleted game folder goes to the Trash on a Mac. Tests turn it off, so they never fill the Trash of the Mac
    /// they run on.
    @ObservationIgnored var deletesToTrash = Platform.current == .macOS

    /// How long a scan waits for an added folder to answer before calling it unavailable, so an offline share can't
    /// hold up the whole library.
    static let reachTimeout: Duration = .seconds(15)
    /// How long detection may take over an added folder's games. Longer, because a large folder on a slow share can
    /// take a while. Games it doesn't finish keep the engine the last scan found.
    static let probeTimeout: Duration = .seconds(60)

    public init(paths: LibraryPaths = .standard, engines: [any EnginePlugin]) {
        self.paths = paths
        self.engines = engines
        records = (try? Self.load(paths.libraryFile)) ?? [:]
        let saved = (try? Self.loadFolders(paths.foldersFile)) ?? []
        folders = saved.filter { $0.removedAt == nil }
        removed = saved.filter { $0.removedAt != nil }
    }

    public func plugin(for id: EngineID?) -> (any EnginePlugin)? {
        guard let id else { return nil }
        return engines.first { $0.descriptor.id == id }
    }

    public func game(_ id: GameID) -> Game? { records[id] }

    /// Every game the library remembers, including those whose folder is missing.
    public var allGames: [Game] { Array(records.values) }

    /// The folder that holds `folder`'s games: the built-in folder for nil, or where an added folder resolved to. For a
    /// folder the player removed, where it was.
    public func root(of folder: LibraryFolderID?) -> URL? {
        guard let folder else { return paths.gamesRoot }
        return resolved[folder]?.url ?? (folders + removed).first { $0.id == folder }?.url
    }

    /// The game's own folder, in whichever library folder holds it. A game whose library folder was removed is still
    /// found where it was, never in another library folder, and `unavailableReason` says it can't be used.
    public func folder(for id: GameID) -> URL {
        guard let game = records[id] else { return paths.folder(for: id) }
        let root = root(of: game.libraryFolder)
            // A folder ID with no folder at all, which no library writes. Nothing is ever there.
            ?? paths.support.appendingPathComponent("Unknown library folder", isDirectory: true)
        return root.appendingPathComponent(game.folderName ?? id.rawValue, isDirectory: true)
    }

    /// Why the game can't be used, when its library folder is unavailable or was removed.
    public func unavailableReason(_ id: GameID) -> String? {
        guard let folder = records[id]?.libraryFolder else { return nil }
        if let problem = folderProblems[folder] { return problem }
        return folders.contains { $0.id == folder } ? nil : "The library folder that held it was removed."
    }

    /// The game whose folder is `name` in `folder`, nil for the built-in folder.
    public func gameID(named name: String, in folder: LibraryFolderID?) -> GameID? {
        records.values.first { $0.libraryFolder == folder && ($0.folderName ?? $0.id.rawValue) == name }?.id
    }

    /// Rescans every library folder and re-runs detection on every present game. Rescans run one after another.
    public func refresh() async {
        let previous = refreshing
        let task = Task { await previous?.value; await scan() }
        refreshing = task
        await task.value
    }

    /// Adds a games folder the player picked. On iPhone and iPad, `url` is the picker's security-scoped URL.
    public func addFolder(_ url: URL) async throws {
        let candidate = Self.realPath(url)
        for existing in [paths.gamesRoot] + folders.map(\.url) {
            let other = Self.realPath(existing)
            if candidate == other || candidate.hasPrefix(other + "/") || other.hasPrefix(candidate + "/") {
                throw LibraryFolderError(message: existing == paths.gamesRoot
                    ? "That folder is VNDegen's own games folder, or overlaps it."
                    : "That folder is already in the library, or overlaps \(existing.lastPathComponent).")
            }
        }
        let opened = FolderAccess.start(url)
        let bookmark: Data
        do {
            bookmark = try FolderAccess.bookmark(for: url)
        } catch {
            if opened { FolderAccess.stop(url) }
            throw error
        }
        // A folder removed before comes back as itself, so its games find their records by name in it.
        let earlier = removed.firstIndex { Self.realPath($0.url) == candidate }
        var folder = LibraryFolder(url: url, bookmark: bookmark)
        if let earlier {
            folder = LibraryFolder(id: removed[earlier].id, url: url, bookmark: bookmark, lastSeen: removed[earlier].lastSeen)
            removed.remove(at: earlier)
        }
        folders.append(folder)
        resolved[folder.id] = FolderAccess.Resolved(url: url, scoped: opened)
        saveFolders()
        await refresh()
    }

    /// Takes an added folder out of the library. Its games leave the library and its files stay where they are. Their
    /// records stay too, so adding the folder back brings back their artwork, play time, and settings.
    public func removeFolder(_ id: LibraryFolderID) async {
        if var folder = folders.first(where: { $0.id == id }) {
            folder.removedAt = .now
            removed.append(folder)
        }
        folders.removeAll { $0.id == id }
        folderProblems[id] = nil
        if let access = resolved.removeValue(forKey: id), access.scoped { FolderAccess.stop(access.url) }
        saveFolders()
        await refresh()
    }

    /// Deletes the game's folder: to the Trash on a Mac, for good on iPhone and iPad. Nothing outside that folder is
    /// touched, so the saves and artwork VNDegen keeps for the game stay, and its record stays as for any game whose
    /// folder disappeared, so putting the folder back restores it. Refuses a folder that can't be reached, or that isn't
    /// a real folder directly inside its library folder.
    public func deleteFolder(of id: GameID) async throws {
        guard let game = records[id], games.contains(where: { $0.id == id }) else {
            throw LibraryFolderError(message: "That game isn't in the library.")
        }
        if let problem = unavailableReason(id) {
            throw LibraryFolderError(message: "\(game.title)'s folder can't be reached. \(problem)")
        }
        guard let root = root(of: game.libraryFolder) else { throw LibraryFolderError(message: "\(game.title)'s library folder was removed.") }
        let folder = folder(for: id), toTrash = deletesToTrash
        try await Task.detached { try FolderAccess.delete(folder, inside: root, toTrash: toTrash) }.value
        await refresh()
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
        return GameLaunch(game: game, folder: folder(for: id), dataDirectory: data, onExit: onExit)
    }

    // MARK: Scanning

    /// One library folder's game folders, as a scan found them.
    struct Listing {
        let folder: LibraryFolderID?
        let root: URL
        let names: [String]
        /// False when the folder couldn't be reached, so `names` are the ones its last scan found.
        let available: Bool
        /// Shown in the ID of a game whose folder name another library folder already uses.
        let label: String
    }

    private func scan() async {
        let gamesRoot = paths.gamesRoot
        let builtIn = await Task.detached {
            try? FileManager.default.createDirectory(at: gamesRoot, withIntermediateDirectories: true)
            return (try? FolderAccess.gameFolders(in: gamesRoot)) ?? []
        }.value
        var reached: [LibraryFolderID: FolderAccess.Reach] = [:]
        await withTaskGroup(of: (LibraryFolderID, FolderAccess.Reach).self) { group in
            for folder in folders { group.addTask { (folder.id, await self.reach(folder)) } }
            for await (id, reach) in group { reached[id] = reach }
        }

        var listings = [Listing(folder: nil, root: gamesRoot, names: builtIn, available: true, label: "VNDegen")]
        for index in folders.indices {
            let folder = folders[index]
            guard let reach = reached[folder.id] else { continue }
            if let names = reach.names, let url = reach.url {
                folders[index].url = url
                if let refreshed = reach.refreshed { folders[index].bookmark = refreshed }
                folders[index].lastSeen = names.sorted()
                folderProblems[folder.id] = nil
                listings.append(Listing(folder: folder.id, root: url, names: names, available: true, label: folder.name))
            } else {
                folderProblems[folder.id] = reach.problem ?? "The folder can't be reached."
                listings.append(Listing(folder: folder.id, root: root(of: folder.id) ?? folder.url, names: folder.lastSeen,
                                        available: false, label: folder.name))
            }
        }

        let placed = Self.place(listings, in: &records)
        let engines = engines
        // Each added folder's games are probed on their own, within `probeTimeout`, so a share that lists its games
        // and then stalls on their files holds up nothing else. The built-in folder is the app's own.
        let groups = Dictionary(grouping: placed.filter(\.available), by: \.folder)
        let matches = await withTaskGroup(of: [(GameID, EngineMatch?)]?.self) { group in
            for (folder, games) in groups {
                let probes = games.map { ($0.id, $0.url) }
                group.addTask {
                    let work = Task.detached {
                        probes.map { id, url in (id, EngineDetector.detect(FolderProbe(scanning: url), using: engines)) }
                    }
                    return folder == nil ? await work.value : await Self.value(of: work, within: Self.probeTimeout)
                }
            }
            var all: [(GameID, EngineMatch?)] = []
            for await found in group { all += found ?? [] }
            return all
        }
        for (id, match) in matches { records[id]?.detected = match }
        games = placed.compactMap { records[$0.id] }.sorted(by: Self.libraryOrder)
        save()
        saveFolders()
    }

    /// A game a scan placed in the library.
    struct Placed {
        let id: GameID
        let folder: LibraryFolderID?
        let url: URL
        let available: Bool
    }

    /// Gives every game folder in `listings` its record, so IDs stay unique across library folders and stay the same
    /// from scan to scan. A folder keeps the record that last held it. A folder new to the library takes the record whose
    /// ID is its name when no present folder holds that record, which is how a game moved between library folders keeps
    /// its artwork and play time. Otherwise, when another library folder has a game of the same name, its ID adds its
    /// library folder's name. IDs are compared without case, because they name folders for artwork and saves, and a
    /// Mac volume usually treats "Game" and "game" as one. Earlier listings win, and the built-in folder comes first.
    static func place(_ listings: [Listing], in records: inout [GameID: Game]) -> [Placed] {
        struct Key: Hashable { let folder: LibraryFolderID?; let name: String }
        var held: [Key: GameID] = [:]
        for game in records.values { held[Key(folder: game.libraryFolder, name: game.folderName ?? game.id.rawValue)] = game.id }

        var placed: [Placed] = []
        var claimed = Set<GameID>()
        var unplaced: [(Listing, String)] = []
        for listing in listings {
            for name in listing.names {
                if let id = held[Key(folder: listing.folder, name: name)], !claimed.contains(id) {
                    claimed.insert(id)
                    placed.append(Placed(id: id, folder: listing.folder, url: listing.root.appendingPathComponent(name, isDirectory: true),
                                         available: listing.available))
                } else if listing.available {
                    unplaced.append((listing, name))
                }
            }
        }
        // Every ID, folded, mapped to the IDs in the library that fold to it.
        var folded: [String: Set<GameID>] = [:]
        for id in records.keys { folded[id.rawValue.lowercased(), default: []].insert(id) }
        for (listing, name) in unplaced {
            // A record can be taken over when nothing present holds it and it was made for a folder of this name. No
            // other record may share its ID in another case.
            func free(_ id: GameID) -> Bool {
                guard !claimed.contains(where: { $0.rawValue.lowercased() == id.rawValue.lowercased() }) else { return false }
                guard folded[id.rawValue.lowercased(), default: []].subtracting([id]).isEmpty else { return false }
                guard let record = records[id] else { return true }
                return (record.folderName ?? record.id.rawValue) == name
            }
            var id = GameID(name)
            if !free(id) {
                let base = "\(name) (\(listing.label))"
                id = GameID(base)
                var number = 2
                while !free(id) {
                    id = GameID("\(base) \(number)")
                    number += 1
                }
            }
            var game = records[id] ?? Game(id: id, title: name)
            game.libraryFolder = listing.folder
            game.folderName = id.rawValue == name ? nil : name
            records[id] = game
            folded[id.rawValue.lowercased(), default: []].insert(id)
            claimed.insert(id)
            placed.append(Placed(id: id, folder: listing.folder, url: listing.root.appendingPathComponent(name, isDirectory: true),
                                 available: true))
        }
        return placed
    }

    /// Reaches an added folder within `reachTimeout`, keeping the security scope that reaching it opened.
    private func reach(_ folder: LibraryFolder) async -> FolderAccess.Reach {
        let current = resolved[folder.id]
        let work = Task.detached { FolderAccess.reach(folder, current: current) }
        guard let reach = await Self.value(of: work, within: Self.reachTimeout) else {
            // The folder answers after the scan gave up on it. The next scan reaches it again, so close what this one opened.
            Task.detached {
                let late = await work.value
                if late.opened, let url = late.url { FolderAccess.stop(url) }
            }
            return FolderAccess.Reach(problem: "The folder didn't answer. The drive or share that holds it may be offline.")
        }
        guard folders.contains(where: { $0.id == folder.id }) else {
            // Removed while the scan reached it.
            if reach.opened, let url = reach.url { FolderAccess.stop(url) }
            return reach
        }
        if reach.names != nil, let url = reach.url, reach.opened || url != current?.url {
            if let current, current.scoped { FolderAccess.stop(current.url) }
            resolved[folder.id] = FolderAccess.Resolved(url: url, scoped: reach.opened)
        }
        return reach
    }

    /// `work`'s value, or nil once `timeout` passes. A task group can't do this, because it waits for every child and
    /// awaiting a task's value ignores cancellation.
    static func value<T: Sendable>(of work: Task<T, Never>, within timeout: Duration) async -> T? {
        let (results, continuation) = AsyncStream<T?>.makeStream()
        let timer = Task {
            try? await Task.sleep(for: timeout)
            continuation.yield(nil)
            continuation.finish()
        }
        Task {
            continuation.yield(await work.value)
            continuation.finish()
        }
        for await result in results {
            timer.cancel()
            return result
        }
        return nil
    }

    private static func realPath(_ url: URL) -> String { url.standardizedFileURL.resolvingSymlinksInPath().path }

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
            try Self.encoder.encode(Array(records.values)).write(to: paths.libraryFile, options: .atomic)
        } catch {
            assertionFailure("library save failed: \(error)")
        }
    }

    private func saveFolders() {
        do {
            try FileManager.default.createDirectory(at: paths.support, withIntermediateDirectories: true)
            try Self.encoder.encode(folders + removed).write(to: paths.foldersFile, options: .atomic)
        } catch {
            assertionFailure("library folders save failed: \(error)")
        }
    }

    private static var encoder: JSONEncoder {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        encoder.dateEncodingStrategy = .iso8601
        return encoder
    }

    private static var decoder: JSONDecoder {
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        return decoder
    }

    private static func load(_ url: URL) throws -> [GameID: Game] {
        let games = try decoder.decode([Game].self, from: Data(contentsOf: url))
        return Dictionary(games.map { ($0.id, $0) }, uniquingKeysWith: { a, _ in a })
    }

    private static func loadFolders(_ url: URL) throws -> [LibraryFolder] {
        try decoder.decode([LibraryFolder].self, from: Data(contentsOf: url))
    }
}

/// Why a library folder change or a game folder deletion was refused, in words the player reads.
public struct LibraryFolderError: LocalizedError, Sendable {
    public let message: String
    public init(message: String) { self.message = message }
    public var errorDescription: String? { message }
}
