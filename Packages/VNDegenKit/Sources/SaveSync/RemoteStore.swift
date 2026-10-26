import Foundation
import Rclone

/// The Ludusavi store on the server, and VNDegen's partial copy of it (the mirror) that `LudusaviStore` reads and writes.
/// rclone keeps the two in step: mapping files and the backups a restore needs come down, and new backups go up. Nothing
/// goes up with `sync`, which deletes what the mirror lacks; uploads only copy.
struct RemoteStore: Sendable {
    let access: RemoteAccess
    let mirror: URL
    let limits: RcloneLimits
    /// The store's path as the user entered it, for messages.
    var path = ""

    var local: LudusaviStore { LudusaviStore(root: mirror) }
    private var root: String { access.root }

    // MARK: Index

    /// Brings every game's mapping.yaml down, as Ludusavi's download is a sync with include rules. rclone's quick check
    /// moves only changed files, and the backups a restore brought down stay. Game folders gone from the server leave
    /// the mirror too.
    func refreshIndex() async throws -> StoreIndex {
        let fm = FileManager.default
        try fm.createDirectory(at: mirror, withIntermediateDirectories: true)
        try await call("sync/sync", ["srcFs": root, "dstFs": mirror.path, "_filter": ["IncludeRule": ["/*/\(LudusaviStore.mappingFile)"]]])
        var folders: [String] = []
        for folder in (try? fm.contentsOfDirectory(atPath: mirror.path)) ?? [] {
            if fm.fileExists(atPath: mirror.appendingPathComponent(folder).appendingPathComponent(LudusaviStore.mappingFile).path) {
                folders.append(folder)
            } else {
                try? fm.removeItem(at: mirror.appendingPathComponent(folder))
            }
        }
        return StoreIndex(mirror: local, folders: folders.sorted())
    }

    /// How many game folders the store holds. Reads only. Throws `noStoreFolder` when the store folder is missing, so a
    /// mistyped path is reported rather than created by a first backup.
    func countGames() async throws -> Int {
        do {
            return try await list("", options: ["dirsOnly": true]).count
        } catch let error as RcloneError where error.status == 404 {
            throw SaveSyncError.noStoreFolder(path)
        }
    }

    // MARK: One game

    /// The server's mapping.yaml of the game, nil when it has none. Copied whole every time into an empty folder: a
    /// quick check would skip a mapping that another device rewrote at the same size in the same second. Leaves the
    /// mirror alone.
    func readMapping(_ folder: String) async throws -> Data? {
        let scratch = mirror.deletingLastPathComponent().appendingPathComponent("Incoming-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: scratch, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: scratch) }
        do {
            try await call("operations/copyfile", ["srcFs": root, "srcRemote": Self.mappingPath(folder),
                                                   "dstFs": scratch.path, "dstRemote": LudusaviStore.mappingFile])
        } catch let error as RcloneError where error.status == 404 {
            return nil
        }
        return try Data(contentsOf: scratch.appendingPathComponent(LudusaviStore.mappingFile))
    }

    /// `readMapping`, also putting the mapping into the mirror, or removing the mirror's copy when the server has none.
    @discardableResult
    func refreshGame(_ folder: String) async throws -> Data? {
        let mapping = try await readMapping(folder)
        let url = mirror.appendingPathComponent(folder, isDirectory: true).appendingPathComponent(LudusaviStore.mappingFile)
        if let mapping {
            try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
            try mapping.write(to: url, options: .atomic)
        } else {
            try? FileManager.default.removeItem(at: url)
        }
        return mapping
    }

    /// Brings down the backup folders a restore of `backup` (nil for the latest) reads: its full backup and, for a
    /// differential, that one. Folders already in the mirror move only where they differ.
    func fetch(_ folder: String, backup: String?) async throws {
        let mapping = try local.mapping(folder)
        var names: [String] = []
        for full in mapping.backups {
            if backup == nil || full.name == backup { names = [full.name] + (backup == nil ? full.children.suffix(1).map(\.name) : []) }
            if let diff = full.children.first(where: { $0.name == backup }) { names = [full.name, diff.name] }
        }
        guard !names.isEmpty else { throw SaveSyncError.noBackups }
        // A full backup named `.` keeps its files in drive folders at the top of the game folder.
        let rules = names.map { "/\(Self.glob(folder))/\($0 == "." ? "drive-*" : Self.glob($0))/**" }
        try await call("sync/sync", ["srcFs": root, "dstFs": mirror.path, "_filter": ["IncludeRule": rules]])
    }

    // MARK: Sending

    /// Copies the mirror's backup folder `backup` of the game to the server, empty folders included. On SMB the server
    /// sets owners and modes, as it does for the PCs' own writes.
    func push(_ folder: String, backup: String) async throws {
        // Made first, so an empty differential's folder goes up too.
        try await call("operations/mkdir", ["fs": root, "remote": "\(folder)/\(backup)"])
        try await call("sync/copy", ["srcFs": mirror.path, "dstFs": root, "createEmptySrcDirs": true,
                                     "_filter": ["IncludeRule": ["/\(Self.glob(folder))/\(Self.glob(backup))/**"]]])
    }

    /// Throws unless the server's copy of backup folder `backup` holds exactly the mirror's files, at the same sizes.
    /// mapping.yaml goes up only after this, so it never names files the server lacks. Only a differential backup may
    /// hold no files, when every file is the full backup's or was removed.
    func verify(_ folder: String, backup: String, differential: Bool) async throws {
        let local = try Self.files(under: mirror.appendingPathComponent(folder).appendingPathComponent(backup))
        let prefix = "\(folder)/\(backup)/"
        let listed: [Item]
        do {
            listed = try await list("\(folder)/\(backup)", options: ["recurse": true, "filesOnly": true])
        } catch let error as RcloneError where error.status == 404 {
            throw SaveSyncError.incompleteUpload(backup)
        }
        let remote = Dictionary(listed.filter { $0.Path.hasPrefix(prefix) }.map { (String($0.Path.dropFirst(prefix.count)), $0.Size) },
                                uniquingKeysWith: { first, _ in first })
        guard remote == local, remote.count == listed.count, !local.isEmpty || differential else { throw SaveSyncError.incompleteUpload(backup) }
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

    /// The first step of the mapping commit: copies the mirror's mapping.yaml of the game next to the server's, under a
    /// temporary name. The name is this device's own, so a temporary file a failed commit left is replaced here, and
    /// never collides with another device's. The copy always transfers, whatever the size and time of a file there.
    func stageMapping(_ folder: String, device: String) async throws {
        try await call("operations/copyfile", ["srcFs": mirror.path, "srcRemote": Self.mappingPath(folder),
                                               "dstFs": root, "dstRemote": Self.temporaryMapping(folder, device: device)],
                       config: ["IgnoreTimes": true])
    }

    /// The second step: renames the staged mapping over the server's mapping.yaml, only while that still holds exactly
    /// `base`, nil meaning none. Returns false, with the server's mapping.yaml as it was, when it holds anything else.
    /// Throws when the call itself fails, and then VNDegen can't tell whether the rename happened.
    func replaceMapping(_ folder: String, base: Data?, device: String) async throws -> Bool {
        // rclone reads the server's mapping.yaml whole, compares it with the base, and only on a match renames the
        // temporary file over it with the smb backend's Move, which replaces the file in one step, so there is never a
        // moment with no mapping.yaml. The window: another device that writes mapping.yaml after the server sent the
        // bytes of that comparison and before the rename completes is overwritten. On SMB that window is four requests,
        // one after another: the comparison read's CLOSE, the backend's parent folder check (CREATE and CLOSE), the
        // rename's CREATE of the temporary file, and the SET_INFO that replaces mapping.yaml. A few milliseconds on a LAN.
        let reply = try await call("vndegen/replacefile", ["fs": root, "srcRemote": Self.temporaryMapping(folder, device: device),
                                                           "dstRemote": Self.mappingPath(folder), "expected": base?.base64EncodedString() ?? NSNull()])
        return try JSONDecoder().decode(Replaced.self, from: reply).replaced
    }

    /// Deletes this device's new backup folder and its temporary mapping. Only before any mapping.yaml on the server
    /// names the folder.
    func discard(_ folder: String, backup: String, device: String) async throws {
        // Never another device's folder: this device's names end with its ID.
        guard backup.hasPrefix("backup-"), backup.hasSuffix("-" + device) else { return }
        try await ignoringMissing { try await call("operations/deletefile", ["fs": root, "remote": Self.temporaryMapping(folder, device: device)]) }
        try await ignoringMissing { try await call("operations/purge", ["fs": root, "remote": "\(folder)/\(backup)"]) }
    }

    // MARK: Plumbing

    static func mappingPath(_ folder: String) -> String { "\(folder)/\(LudusaviStore.mappingFile)" }

    static func temporaryMapping(_ folder: String, device: String) -> String { "\(folder)/.\(LudusaviStore.mappingFile).vndegen-\(device).tmp" }

    /// `name` as an rclone filter glob that matches only itself.
    static func glob(_ name: String) -> String {
        var escaped = ""
        for character in name {
            if "\\*?[]{}".contains(character) { escaped.append("\\") }
            escaped.append(character)
        }
        return escaped
    }

    struct Item: Decodable {
        var Path: String
        var Size: Int64
        var IsDir: Bool
    }

    private struct Listing: Decodable { var list: [Item] }
    private struct Replaced: Decodable { var replaced: Bool }

    private func list(_ remote: String, options: [String: Any]) async throws -> [Item] {
        let reply = try await call("operations/list", ["fs": root, "remote": remote, "opt": options.merging(["noModTime": true, "noMimeType": true]) { a, _ in a }])
        return try JSONDecoder().decode(Listing.self, from: reply).list
    }

    private func ignoringMissing(_ work: () async throws -> Void) async throws {
        do { try await work() } catch let error as RcloneError where error.status == 404 {}
    }

    /// Runs one rc call with rclone's limits, and `config` on top of them.
    @discardableResult
    private func call(_ method: String, _ input: [String: Any], config: [String: Any] = [:]) async throws -> Data {
        var input = input
        input["_config"] = limits.config.merging(config) { _, new in new }
        let data = try JSONSerialization.data(withJSONObject: input)
        do {
            return try await access.call(method, data)
        } catch let error as RcloneError {
            if let known = SaveSyncError(error) { throw known }
            throw error
        }
    }
}

/// rclone's limits on every call: how long a connection may take, and the idle deadline on every read and write, the
/// SMB login included. One try only, so a stalled server is not retried for minutes.
struct RcloneLimits: Sendable {
    var connect: Duration = .seconds(10)
    var idle: Duration = .seconds(30)

    /// The `_config` block of an rc call. rclone takes its config fields by their Go names (`fs.ConfigInfo`).
    var config: [String: Any] {
        ["ConnectTimeout": Self.text(connect), "Timeout": Self.text(idle), "LowLevelRetries": 1, "Retries": 1]
    }

    private static func text(_ duration: Duration) -> String { "\(Int((duration / .milliseconds(1)).rounded()))ms" }
}

extension SaveSyncError {
    /// The error a user can act on for an rclone failure, when there is one.
    init?(_ error: RcloneError) {
        let message = error.message.lowercased()
        if message.contains("i/o timeout") || message.contains("deadline exceeded") {
            self = .timedOut
        } else if message.contains("attempted logon is invalid") {
            self = .loginRefused
        } else {
            return nil
        }
    }
}

/// The games in the store, read from the mirror's mapping files.
struct StoreIndex: Sendable {
    /// Folder name to the game's Ludusavi title.
    var games: [String: String] = [:]
    /// The folder before `<install dir>` that most of the store's backups record under Steam, such as
    /// `/home/deck/.local/share/Steam/steamapps/common/`.
    var steamLibrary: String?

    init() {}

    init(mirror: LudusaviStore, folders: [String]) {
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
