import Foundation
import Rsync

/// The Ludusavi store on the server, and VNDegen's partial copy of it (the mirror) that `LudusaviStore` reads and writes.
/// rsync keeps the two in step: mapping files and the backups a restore needs come down, and new backups go up.
struct RemoteStore: Sendable {
    /// The store folder on the server, without a trailing slash. Relative paths start in the login folder.
    let path: String
    let mirror: URL

    init(path: String, mirror: URL) {
        var path = path.trimmingCharacters(in: .whitespaces)
        // The shell doesn't expand a quoted `~`, and rsync starts in the home folder anyway.
        if path == "~" { path = "." } else if path.hasPrefix("~/") { path.removeFirst(2) }
        while path.count > 1, path.hasSuffix("/") { path.removeLast() }
        self.path = path.isEmpty ? "." : path
        self.mirror = mirror
    }

    var local: LudusaviStore { LudusaviStore(root: mirror) }

    func remote(_ folder: String? = nil) -> String {
        folder.map { "\(path)/\($0)/" } ?? "\(path)/"
    }

    // MARK: Index

    /// Brings every game's mapping.yaml down. Only changed files move. Game folders gone from the server leave the
    /// mirror too.
    func refreshIndex(_ client: RsyncClient) async throws -> StoreIndex {
        try FileManager.default.createDirectory(at: mirror, withIntermediateDirectories: true)
        let report = try await client.pull(remote(), into: mirror, filters: [.include("/*/"), .include("/*/mapping.yaml"), .exclude("*")],
                                           owners: true)
        let remoteFolders = Set(report.entries.filter { $0.path.hasSuffix("/" + LudusaviStore.mappingFile) }.map { String($0.path.dropLast(13)) })
        for folder in (try? FileManager.default.contentsOfDirectory(atPath: mirror.path)) ?? [] where !remoteFolders.contains(folder) {
            try? FileManager.default.removeItem(at: mirror.appendingPathComponent(folder))
        }
        return StoreIndex(mirror: local, folders: remoteFolders.sorted(), root: report.entries.first { $0.path == "." })
    }

    /// Brings one game's mapping.yaml into the mirror, or removes the mirror's copy when the server has none. Returns
    /// what `readGame` does.
    @discardableResult
    func refreshGame(_ client: RsyncClient, folder: String) async throws -> GameState {
        let state = try await readGame(client, folder: folder)
        let url = mirror.appendingPathComponent(folder, isDirectory: true).appendingPathComponent(LudusaviStore.mappingFile)
        if let mapping = state.mapping {
            try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
            try mapping.write(to: url, options: .atomic)
        } else {
            try? FileManager.default.removeItem(at: url)
        }
        return state
    }

    /// The server's mapping.yaml of the game, read whole every time, and its entries for the store folder, the game
    /// folder, and the mapping, which give new files their owner. Leaves the mirror alone. rsync's quick check would skip
    /// a mapping.yaml that another device rewrote at the same size within the same second, so this pulls into an empty
    /// folder instead.
    func readGame(_ client: RsyncClient, folder: String) async throws -> GameState {
        try await readGame(client, folder: folder, arrived: nil)
    }

    /// `readGame`, also handing the state to `arrived` as soon as the mapping's bytes are in and checked, before the
    /// server's closing statistics and exit.
    private func readGame(_ client: RsyncClient, folder: String, arrived: (@Sendable (GameState) async throws -> Void)?) async throws -> GameState {
        let scratch = mirror.deletingLastPathComponent().appendingPathComponent("Incoming-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: scratch, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: scratch) }
        let name = RsyncClient.escapedPattern(folder)
        let state = { @Sendable (listed: [RsyncEntry]) throws -> GameState in
            let entries = GameEntries(root: listed.first { $0.path == "." }, folder: listed.first { $0.path == folder },
                                      mapping: listed.first { $0.path == "\(folder)/\(LudusaviStore.mappingFile)" })
            let url = scratch.appendingPathComponent(folder, isDirectory: true).appendingPathComponent(LudusaviStore.mappingFile)
            return GameState(entries: entries, mapping: entries.mapping == nil ? nil : try Data(contentsOf: url))
        }
        var onArrival: (@Sendable ([RsyncEntry]) async throws -> Void)?
        if let arrived { onArrival = { listed in try await arrived(try state(listed)) } }
        let report = try await client.pull(remote(), into: scratch, filters: [.include("/\(name)/"), .include("/\(name)/mapping.yaml"), .exclude("*")],
                                           owners: true, arrived: onArrival)
        return try state(report.entries)
    }

    /// Whether the server's mapping.yaml of the game is `expected`, byte for byte, nil meaning none. Answers as soon as
    /// the file's bytes are in and checked, without waiting for the server's closing statistics and exit. `rest` is that
    /// remainder of the read; it fails when the server does not end cleanly.
    func mappingIs(_ expected: Data?, _ client: RsyncClient, folder: String) async throws -> (same: Bool, rest: Task<Void, any Error>) {
        let (answers, answer) = AsyncThrowingStream<Bool, any Error>.makeStream()
        let rest = Task {
            do {
                _ = try await readGame(client, folder: folder) { answer.yield($0.mapping == expected) }
                answer.finish()
            } catch {
                answer.finish(throwing: error)
                throw error
            }
        }
        for try await same in answers { return (same, rest) }
        throw RsyncError.connectionClosed(nil)
    }

    struct GameEntries: Sendable {
        var root: RsyncEntry?
        var folder: RsyncEntry?
        var mapping: RsyncEntry?
    }

    /// A game on the server as one read saw it.
    struct GameState: Sendable {
        var entries: GameEntries
        /// mapping.yaml's bytes. Nil when the server has none.
        var mapping: Data?
    }

    /// Brings down the backup folders a restore of `backup` (nil for the latest) reads: its full backup and, for a
    /// differential, that one. Folders already in the mirror move only where they differ.
    func fetch(_ client: RsyncClient, folder: String, backup: String?) async throws {
        let mapping = try local.mapping(folder)
        var names: [String] = []
        for full in mapping.backups {
            if backup == nil || full.name == backup { names = [full.name] + (backup == nil ? full.children.suffix(1).map(\.name) : []) }
            if let diff = full.children.first(where: { $0.name == backup }) { names = [full.name, diff.name] }
        }
        guard !names.isEmpty else { throw SaveSyncError.noBackups }
        let filters = names.flatMap { name -> [RsyncFilter] in
            // A full backup named `.` keeps its files in drive folders at the top of the game folder.
            name == "." ? [.include("/drive-*/***")] : [.include("/\(RsyncClient.escapedPattern(name))/***")]
        }
        try await client.pull(remote(folder), into: mirror.appendingPathComponent(folder, isDirectory: true), filters: filters + [.exclude("*")])
    }

    // MARK: Sending

    /// Sends the mirror's backup folder `backup` of the game up. New folders and files take the game folder's owner,
    /// group, and mode, or the store folder's for a new game, so the PC's own access to the share keeps working.
    func push(_ client: RsyncClient, folder: String, backup: String, like entries: GameEntries) async throws {
        var paths = [backup]
        for relative in try Self.files(under: mirror.appendingPathComponent(folder).appendingPathComponent(backup)).keys {
            let parts = relative.split(separator: "/").map(String.init)
            for depth in 1 ..< parts.count { paths.append(([backup] + parts.prefix(depth)).joined(separator: "/")) }
            paths.append(backup + "/" + relative)
        }
        try await upload(client, folder: folder, paths: Array(Set(paths)), like: entries)
    }

    /// Throws unless the server's copy of backup folder `backup` holds exactly the mirror's files, at the same sizes.
    /// mapping.yaml goes up only after this, so it never names files the server lacks. Only a differential backup may
    /// hold no files, when every file is the full backup's or was removed.
    func verify(_ client: RsyncClient, folder: String, backup: String, differential: Bool) async throws {
        let local = try Self.files(under: mirror.appendingPathComponent(folder).appendingPathComponent(backup))
        let listed = try await client.list(remote(folder), filters: [.include("/\(RsyncClient.escapedPattern(backup))/***"), .exclude("*")])
        let prefix = backup + "/"
        let remote = Dictionary(uniqueKeysWithValues: listed.filter { $0.kind == .file && $0.path.hasPrefix(prefix) }
            .map { (String($0.path.dropFirst(prefix.count)), $0.size) })
        let hasFolder = listed.contains { $0.kind == .directory && $0.path == backup }
        guard hasFolder, remote == local, !local.isEmpty || differential else { throw SaveSyncError.incompleteUpload(backup) }
    }

    /// Every file under `root`, by relative path, with its size. Hidden ones too: backups keep paths such as
    /// `drive-0/home/deck/.local/share/...`.
    static func files(under root: URL) throws -> [String: Int64] {
        let base = root.standardizedFileURL.resolvingSymlinksInPath().pathComponents.count
        guard let walker = FileManager.default.enumerator(at: root, includingPropertiesForKeys: [.isRegularFileKey, .fileSizeKey]) else {
            throw SaveSyncError.unreadable(root.path)
        }
        var files: [String: Int64] = [:]
        for case let url as URL in walker {
            let values = try url.resourceValues(forKeys: [.isRegularFileKey, .fileSizeKey])
            guard values.isRegularFile == true else { continue }
            let relative = url.standardizedFileURL.resolvingSymlinksInPath().pathComponents.dropFirst(base).joined(separator: "/")
            files[relative] = Int64(values.fileSize ?? 0)
        }
        return files
    }

    /// Sends the mirror's mapping.yaml of the game up once the server has asked for it and `confirm` returns true.
    /// Returns false when `confirm` declined, which leaves the server's mapping.yaml as it was.
    func pushMapping(_ client: RsyncClient, folder: String, like entries: GameEntries,
                     confirm: @escaping @Sendable () async throws -> Bool) async throws -> Bool {
        let sent = try await upload(client, folder: folder, paths: [LudusaviStore.mappingFile], like: entries) { _ in try await confirm() }
        return !sent.isEmpty
    }

    @discardableResult
    private func upload(_ client: RsyncClient, folder: String, paths: [String], like entries: GameEntries,
                        confirm: (@Sendable (String) async throws -> Bool)? = nil) async throws -> [String] {
        let template = entries.folder ?? entries.root
        let owner = template?.owner
        let folderMode = template?.permissions ?? 0o755
        let fileMode = folderMode & 0o666
        let gameDir = mirror.appendingPathComponent(folder, isDirectory: true)
        // The game folder itself goes first, so a new game gets a folder with the right owner.
        var uploads = [RsyncClient.Upload(entry: RsyncEntry(path: folder, kind: .directory, size: 0,
                                                            modified: entries.folder?.modified ?? Int64(Date.now.timeIntervalSince1970),
                                                            permissions: entries.folder?.permissions ?? folderMode, owner: entries.folder?.owner ?? owner),
                                          source: nil)]
        for path in paths {
            let url = gameDir.appendingPathComponent(path)
            let values = try url.resourceValues(forKeys: [.isDirectoryKey, .fileSizeKey, .contentModificationDateKey])
            let modified = Int64((values.contentModificationDate ?? .now).timeIntervalSince1970)
            uploads.append(values.isDirectory == true
                ? .init(entry: RsyncEntry(path: "\(folder)/\(path)", kind: .directory, size: 0, modified: modified, permissions: folderMode, owner: owner), source: nil)
                : .init(entry: RsyncEntry(path: "\(folder)/\(path)", kind: .file, size: Int64(values.fileSize ?? 0), modified: modified,
                                          permissions: fileMode, owner: owner), source: url))
        }
        return try await client.push(uploads, to: remote(), owners: owner != nil, confirm: confirm)
    }

    /// Deletes backup folders of the game on the server. Only for this device's own new folder, before any mapping.yaml
    /// names it.
    func delete(_ client: RsyncClient, folder: String, names: [String]) async throws {
        try await client.delete(names, in: remote(folder))
    }
}

/// The games in the store, read from the mirror's mapping files.
struct StoreIndex: Sendable {
    /// Folder name to the game's Ludusavi title.
    var games: [String: String] = [:]
    /// The folder before `<install dir>` that most of the store's backups record under Steam, such as
    /// `/home/deck/.local/share/Steam/steamapps/common/`.
    var steamLibrary: String?
    var root: RsyncEntry?

    init() {}

    init(mirror: LudusaviStore, folders: [String], root: RsyncEntry?) {
        self.root = root
        var libraries: [String: Int] = [:]
        for folder in folders {
            guard let mapping = try? mirror.mapping(folder) else { continue }
            games[folder] = mapping.name
            for path in (mapping.backups.last?.files ?? [:]).keys {
                guard let range = path.range(of: "/steamapps/common/") else { continue }
                libraries[String(path[..<range.upperBound]), default: 0] += 1
            }
        }
        steamLibrary = libraries.max { $0.value != $1.value ? $0.value < $1.value : $0.key > $1.key }?.key
    }
}
