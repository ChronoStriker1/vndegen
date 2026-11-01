import Foundation

/// What an engine plugin says it may remove from a game folder: only what its games don't need in VNDegen. It is an
/// allowlist, so anything it doesn't name stays. Names match in any case and are relative to `root`.
public struct EngineFileList: Equatable, Sendable {
    /// Where the names below are, relative to the game folder. Empty when they are at its top.
    public var root: String
    /// Files, by path, such as `nw.dll` or `resources/electron.asar`.
    public var files: Set<String>
    /// Files that count only at this size or more, because a game could have a small file of the same name.
    public var filesOfAtLeast: [String: Int64]
    /// Folders whose files count when their names end in one of these, such as `.pak` in `locales`.
    public var fileEndings: [String: [String]]
    /// Folders that go whole, because only the engine puts anything in them.
    public var folders: Set<String>

    public init(root: String = "", files: Set<String> = [], filesOfAtLeast: [String: Int64] = [:],
                fileEndings: [String: [String]] = [:], folders: Set<String> = []) {
        self.root = root
        self.files = Set(files.map { $0.lowercased() })
        self.filesOfAtLeast = Dictionary(uniqueKeysWithValues: filesOfAtLeast.map { ($0.key.lowercased(), $0.value) })
        self.fileEndings = Dictionary(uniqueKeysWithValues: fileEndings.map { ($0.key.lowercased(), $0.value.map { $0.lowercased() }) })
        self.folders = Set(folders.map { $0.lowercased() })
    }

    func names(file path: String, size: Int64) -> Bool {
        if files.contains(path) { return true }
        if let least = filesOfAtLeast[path], size >= least { return true }
        guard let slash = path.lastIndex(of: "/"), let endings = fileEndings[String(path[..<slash])] else { return false }
        return endings.contains { path.hasSuffix($0) }
    }

    /// True when something the list names may be inside the folder at `path`.
    func reaches(folder path: String) -> Bool {
        let prefix = path + "/"
        return fileEndings[path] != nil || (Array(files) + Array(filesOfAtLeast.keys) + Array(folders)).contains { $0.hasPrefix(prefix) }
    }
}

/// A file or folder VNDegen can offer to remove.
public struct EngineFile: Hashable, Sendable {
    /// Relative to the game folder, in its case on disk.
    public let path: String
    public let size: Int64
    /// How many files it holds, 1 for a file.
    public let fileCount: Int
    /// `path` on disk. It differs when iCloud keeps only a placeholder for a file not downloaded yet.
    let stored: String
}

/// What a game offers to remove, or why it can't.
public struct EngineFilePlan: Sendable {
    public var files: [EngineFile] = []
    /// Why nothing can be removed, in words for the player.
    public var problem: String?
    /// True when the game folder is in iCloud Drive, so a removal reaches the player's other devices.
    public var inICloud = false

    public var size: Int64 { files.reduce(0) { $0 + $1.size } }
}

/// What a removal did. The player sees both lists.
public struct EngineFileRemoval: Sendable {
    public struct Failure: Sendable {
        public let path: String
        public let reason: String
    }

    public var removed: [EngineFile] = []
    public var failed: [Failure] = []
}

nonisolated enum EngineFiles {
    enum Place: Equatable {
        case local, iCloud
        case refused(String)
    }

    /// Where a game folder is, and whether VNDegen removes files there. Every refusal for a kind of place is in `place`.
    static func place(of folder: URL) -> Place {
        #if canImport(Darwin)
        let values = try? folder.resourceValues(forKeys: [.volumeIsLocalKey, .volumeIsReadOnlyKey, .isUbiquitousItemKey])
        return place(path: folder.resolvingSymlinksInPath().path, isLocal: values?.volumeIsLocal ?? true,
                     isReadOnly: values?.volumeIsReadOnly ?? false, isUbiquitous: values?.isUbiquitousItem ?? false)
        #else
        return .local
        #endif
    }

    /// `isUbiquitous` is true for iCloud Drive and for some other cloud providers, so it can't tell them apart. The path
    /// can: iCloud Drive is under `Library/Mobile Documents` on every Apple platform. The path can't name every other
    /// provider, because on iPhone and iPad they share `File Provider Storage` with On My iPhone. A provider there that
    /// doesn't report `isUbiquitous` looks local.
    static func place(path: String, isLocal: Bool, isReadOnly: Bool, isUbiquitous: Bool) -> Place {
        if !isLocal || path.contains("/com.apple.filesystems.smbclientd/") {
            return .refused("This folder is on a network share, so VNDegen won't remove files from it.")
        }
        if isReadOnly { return .refused("This folder is on a read-only volume, so VNDegen can't remove files from it.") }
        if path.contains("/Library/Mobile Documents/") { return .iCloud }
        if isUbiquitous || path.contains("/Library/CloudStorage/") {
            return .refused("This folder is in a cloud storage service, so VNDegen won't remove files from it. Of the cloud services, only iCloud Drive is allowed.")
        }
        return .local
    }

    /// The size of a file or folder entry from its metadata, so nothing is downloaded. Links aren't followed.
    private static func attributes(_ path: String) -> (type: FileAttributeType?, size: Int64)? {
        guard let values = try? FileManager.default.attributesOfItem(atPath: path) else { return nil }
        return (values[.type] as? FileAttributeType, (values[.size] as? NSNumber)?.int64Value ?? 0)
    }

    /// The name of the file an iCloud placeholder stands for, as in `.nw.dll.icloud` for `nw.dll`.
    static func placeholderTarget(_ name: String) -> String? {
        guard name.hasPrefix("."), name.hasSuffix(".icloud"), name.count > ".".count + ".icloud".count else { return nil }
        return String(name.dropFirst().dropLast(".icloud".count))
    }

    /// A placeholder is a small property list that records the real file's size.
    static func placeholderSize(_ url: URL) -> Int64 {
        guard let data = try? Data(contentsOf: url), data.count < 1 << 16,
              let plist = try? PropertyListSerialization.propertyList(from: data, format: nil) as? [String: Any]
        else { return 0 }
        return (plist["NSURLFileSizeKey"] as? NSNumber)?.int64Value ?? 0
    }

    private static func folderContents(_ url: URL, iCloud: Bool) -> (size: Int64, count: Int) {
        var size: Int64 = 0, count = 0
        guard let items = FileManager.default.enumerator(atPath: url.path) else { return (0, 0) }
        for case let item as String in items {
            let path = url.appendingPathComponent(item)
            guard let found = attributes(path.path), found.type != .typeDirectory else { continue }
            let isPlaceholder = iCloud && placeholderTarget(path.lastPathComponent) != nil
            size += isPlaceholder ? placeholderSize(path) : found.size
            count += 1
        }
        return (size, count)
    }

    /// What `list` names in `folder`, read from the disk now. A link is never followed or offered. A folder all of whose
    /// entries are named is offered as one folder, so it leaves no empty folder behind and goes back whole from the Trash.
    static func files(_ list: EngineFileList, in folder: URL, iCloud: Bool = false) -> [EngineFile] {
        let base = list.root.isEmpty ? "" : list.root + "/"

        func visit(_ relative: String) -> (found: [EngineFile], all: Bool) {
            let directory = relative.isEmpty ? folder.appendingPathComponent(list.root) : folder.appendingPathComponent(base + relative)
            guard let names = try? FileManager.default.contentsOfDirectory(atPath: directory.path), !names.isEmpty else { return ([], false) }
            var found: [EngineFile] = [], all = true
            for stored in names {
                let name = iCloud ? placeholderTarget(stored) ?? stored : stored
                let prefix = relative.isEmpty ? "" : relative + "/"
                let path = prefix + name, lower = path.lowercased()
                let url = directory.appendingPathComponent(stored)
                guard let item = attributes(url.path) else { all = false; continue }
                if item.type == .typeDirectory {
                    if list.folders.contains(lower) {
                        let contents = folderContents(url, iCloud: iCloud)
                        found.append(EngineFile(path: base + path, size: contents.size, fileCount: contents.count, stored: base + prefix + stored))
                    } else if list.reaches(folder: lower) {
                        let inner = visit(path)
                        if inner.all {
                            let size = inner.found.reduce(0) { $0 + $1.size }, count = inner.found.reduce(0) { $0 + $1.fileCount }
                            found.append(EngineFile(path: base + path, size: size, fileCount: count, stored: base + prefix + stored))
                        } else {
                            found += inner.found
                            all = false
                        }
                    } else {
                        all = false
                    }
                } else if item.type == .typeRegular {
                    let size = name == stored ? item.size : placeholderSize(url)
                    if list.names(file: lower, size: size) {
                        found.append(EngineFile(path: base + path, size: size, fileCount: 1, stored: base + prefix + stored))
                    } else {
                        all = false
                    }
                } else {
                    all = false
                }
            }
            return (found, all)
        }
        return visit("").found.sorted { $0.size != $1.size ? $0.size > $1.size : $0.path < $1.path }
    }

    /// Removes one offered file or folder: to the Trash when `toTrash`, otherwise for good. An iCloud item goes through a
    /// file coordinator, which lets iCloud finish with it first, and deleting never downloads it.
    static func remove(_ file: EngineFile, in folder: URL, toTrash: Bool, iCloud: Bool) throws {
        let stored = folder.appendingPathComponent(file.stored)
        func delete(_ url: URL) throws {
            #if canImport(Darwin)
            if toTrash { return try FileManager.default.trashItem(at: url, resultingItemURL: nil) }
            #endif
            try FileManager.default.removeItem(at: url)
        }
        #if canImport(Darwin)
        if iCloud {
            let named = folder.appendingPathComponent(file.path)
            var coordination: NSError?, failure: (any Error)?
            NSFileCoordinator().coordinate(writingItemAt: named, options: .forDeleting, error: &coordination) { actual in
                do { try delete(file.stored == file.path ? actual : stored) } catch { failure = error }
            }
            if let error = coordination ?? failure { throw error }
            return
        }
        #endif
        try delete(stored)
    }
}

extension LibraryStore {
    /// True when the game's engine has a list and detection, not the player, chose that engine. A game the player
    /// assigned by hand may not be what the list expects, so it's offered nothing.
    public func offersEngineFiles(_ id: GameID) -> Bool {
        guard let game = game(id), let detected = game.detected?.engine, game.engine == detected else { return false }
        return plugin(for: detected)?.listsEngineFiles == true
    }

    /// What the game offers to remove, read from its folder now. It reads names and sizes only, and the game's own files
    /// that detection reads, so it downloads nothing it would remove.
    public func engineFilePlan(for id: GameID) async -> EngineFilePlan {
        guard let game = game(id), games.contains(where: { $0.id == id }), offersEngineFiles(id),
              let plugin = plugin(for: game.engine) else { return EngineFilePlan() }
        if let problem = unavailableReason(id) { return EngineFilePlan(problem: "Its folder can't be reached. \(problem)") }
        guard let root = root(of: game.libraryFolder) else { return EngineFilePlan(problem: "Its library folder was removed.") }
        let folder = folder(for: id)
        return await Task.detached { Self.plan(plugin, game: game, folder: folder, root: root) }.value
    }

    /// Removes what the player confirmed, if it's still on the list and still the size they saw. Anything else stays, and
    /// the result says so. The folder is checked again, since it may have changed since the plan.
    public func removeEngineFiles(_ confirmed: [EngineFile], of id: GameID) async -> EngineFileRemoval {
        guard let game = game(id), games.contains(where: { $0.id == id }), offersEngineFiles(id),
              let plugin = plugin(for: game.engine), let root = root(of: game.libraryFolder), unavailableReason(id) == nil else {
            return EngineFileRemoval(failed: confirmed.map { .init(path: $0.path, reason: "The game's folder can't be reached.") })
        }
        let folder = folder(for: id), toTrash = deletesToTrash
        let result = await Task.detached { () -> EngineFileRemoval in
            let plan = Self.plan(plugin, game: game, folder: folder, root: root)
            var result = EngineFileRemoval()
            if let problem = plan.problem {
                result.failed = confirmed.map { .init(path: $0.path, reason: problem) }
                return result
            }
            for file in confirmed {
                guard let current = plan.files.first(where: { $0.path == file.path && $0.size == file.size }) else {
                    result.failed.append(.init(path: file.path, reason: "It changed after VNDegen listed it."))
                    continue
                }
                do {
                    // iCloud always uses the Trash, so Recently Deleted can keep what the sheet says it should keep.
                    try EngineFiles.remove(current, in: folder, toTrash: toTrash || plan.inICloud, iCloud: plan.inICloud)
                    result.removed.append(current)
                } catch {
                    result.failed.append(.init(path: file.path, reason: error.localizedDescription))
                }
            }
            return result
        }.value
        await refresh()
        return result
    }

    private nonisolated static func plan(_ plugin: any EnginePlugin, game: Game, folder: URL, root: URL) -> EngineFilePlan {
        do {
            try FolderAccess.checkGameFolder(folder, inside: root)
        } catch {
            return EngineFilePlan(problem: error.localizedDescription)
        }
        let place = EngineFiles.place(of: folder)
        if case let .refused(problem) = place { return EngineFilePlan(problem: problem) }
        guard FileManager.default.isWritableFile(atPath: folder.path) else {
            return EngineFilePlan(problem: "VNDegen can't write to this folder.")
        }
        let probe = FolderProbe(scanning: folder)
        guard let list = plugin.engineFileList(probe) else { return EngineFilePlan() }
        let files = EngineFiles.files(list, in: folder, iCloud: place == .iCloud)
        // Without these files the plugin must still find the same game. Only the plugin looks, because other plugins read
        // whole `.exe` files while detecting, which in iCloud would download what's about to go.
        let gone = files.map { $0.path.lowercased() }
        let kept = probe.entries.filter { entry in
            let lower = entry.lowercased()
            return !gone.contains { lower == $0 || lower.hasPrefix($0 + "/") }
        }
        guard plugin.detect(FolderProbe(root: folder, entries: kept))?.engine == game.detected?.engine else { return EngineFilePlan() }
        return EngineFilePlan(files: files, inICloud: place == .iCloud)
    }
}
