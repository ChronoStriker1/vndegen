import Foundation

/// An archive the user picked: one file, or every volume of a multi-volume set in read order.
nonisolated public struct ArchiveSource: Sendable, Hashable {
    public let volumes: [URL]
    /// The archive's file name without its extension or part suffix, offered as the game's folder name.
    public let suggestedFolderName: String

    public var fileName: String { volumes[0].lastPathComponent }

    /// Groups picked files into one archive. Picking a single volume also pulls in its sibling volumes when the folder
    /// can be listed, which works on macOS and for files already in the app's container.
    public init(picked urls: [URL]) throws(ArchiveError) {
        let named = urls.compactMap { url in VolumeName(url.lastPathComponent).map { (url, $0) } }
        guard let first = named.first else { throw .notAnArchive }
        guard named.allSatisfy({ $0.1.set == first.1.set }) else { throw .severalArchives }

        var members = named
        if first.1.kind != .single, named.count == 1 {
            let folder = first.0.deletingLastPathComponent()
            let siblings = ((try? FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: nil)) ?? [])
                .compactMap { url in VolumeName(url.lastPathComponent).map { (url, $0) } }
                .filter { $0.1.set == first.1.set }
            // The listing holds the picked file too, unless the folder can't be listed.
            if !siblings.isEmpty { members = siblings }
        }
        var volumes: [Int: URL] = [:]
        for (url, name) in members {
            // Such as `game.part1.rar` beside `game.part01.rar`, which read as the same part.
            guard volumes.updateValue(url, forKey: name.index) == nil else { throw .duplicateVolume(name.index) }
        }
        let order = volumes.keys.sorted()
        if first.1.kind != .single, let missing = Self.firstGap(order, startingAt: first.1.kind.firstIndex) {
            throw .missingVolume(missing)
        }
        self.volumes = order.map { volumes[$0]! }
        suggestedFolderName = first.1.stem
    }

    private init(volumes: [URL], suggestedFolderName: String) {
        self.volumes = volumes
        self.suggestedFolderName = suggestedFolderName
    }

    /// Runs `body` while every volume is read under file coordination, so a file provider such as iCloud Drive can't
    /// move, replace, or evict a volume meanwhile. `body` gets the source at the URLs the coordinator handed out.
    func coordinatedRead<T>(_ body: (ArchiveSource) throws -> T) throws -> T {
        try NSFileCoordinator.read(volumes) { try body(ArchiveSource(volumes: $0, suggestedFolderName: suggestedFolderName)) }
    }

    /// The first part number missing from a volume set, counted the way the files are named (part1, r00, 001).
    private static func firstGap(_ indexes: [Int], startingAt start: Int) -> Int? {
        for (offset, index) in indexes.enumerated() where index != start + offset { return start + offset }
        return nil
    }
}

nonisolated extension NSFileCoordinator {
    /// Runs `body` inside one coordinated read of each of `urls`, passing the URLs the coordinator handed out. A
    /// coordinated read also downloads a file the provider evicted.
    static func read<T>(_ urls: [URL], _ body: ([URL]) throws -> T) throws -> T {
        let coordinator = NSFileCoordinator()
        func read(from index: Int, granted: [URL]) throws -> T {
            guard index < urls.count else { return try body(granted) }
            var result: Result<T, Error>?
            var coordinationError: NSError?
            coordinator.coordinate(readingItemAt: urls[index], options: [], error: &coordinationError) { url in
                result = Result { try read(from: index + 1, granted: granted + [url]) }
            }
            guard let result else { throw coordinationError ?? CocoaError(.fileReadUnknown) }
            return try result.get()
        }
        return try read(from: 0, granted: [])
    }

    /// Deletes `url` inside a coordinated write when the file there still has `identity`. Returns false when a different
    /// file is there now, which it leaves alone.
    static func delete(_ url: URL, ifStill identity: FileIdentity) -> Bool {
        var replaced = false
        var coordinationError: NSError?
        NSFileCoordinator().coordinate(writingItemAt: url, options: .forDeleting, error: &coordinationError) { url in
            let current = FileIdentity(of: url)
            guard current == identity else {
                replaced = current != nil
                return
            }
            do {
                try FileManager.default.removeItem(at: url)
            } catch {
                importLog.error("Could not delete \(url.lastPathComponent, privacy: .public): \(error.localizedDescription, privacy: .public)")
            }
        }
        if let coordinationError {
            importLog.error("Could not delete \(url.lastPathComponent, privacy: .public): \(coordinationError.localizedDescription, privacy: .public)")
        }
        return !replaced
    }
}

/// Tells a file apart from one saved later under the same name, such as a replacement copied in with Files.
nonisolated struct FileIdentity: Equatable, Sendable {
    let volume: Int
    let fileNumber: Int
    let size: Int64
    let modified: Date

    /// Nil when nothing is at `url`.
    init?(of url: URL) {
        guard let attributes = try? FileManager.default.attributesOfItem(atPath: url.path),
              let volume = attributes[.systemNumber] as? Int,
              let fileNumber = attributes[.systemFileNumber] as? Int,
              let size = attributes[.size] as? Int64,
              let modified = attributes[.modificationDate] as? Date else { return nil }
        self.volume = volume
        self.fileNumber = fileNumber
        self.size = size
        self.modified = modified
    }
}

/// What a file name says about the file's place in an archive set.
nonisolated struct VolumeName: Equatable {
    enum Kind: Equatable {
        /// `game.zip`, `game.7z`, or `game.rar` with no other volumes.
        case single
        /// `game.part1.rar`, `game.part2.rar`, ...
        case rarParts
        /// `game.rar`, `game.r00`, `game.r01`, ... The `.rar` volume reads first.
        case rarOldStyle
        /// `game.7z.001`, `game.7z.002`, ... Byte-split files that read as one stream.
        case split

        var firstIndex: Int { self == .rarParts || self == .split ? 1 : 0 }
    }

    /// The name without extension and part suffix.
    var stem: String
    var kind: Kind
    /// Shared by every volume of one set.
    var set: String
    /// Read order inside the set.
    var index: Int

    init?(_ fileName: String) {
        if let match = fileName.wholeMatch(of: /(.+)\.part(\d+)\.rar/.ignoresCase()) {
            // A part number too long for Int is no real volume.
            guard let index = Int(match.2) else { return nil }
            self.init(fileName, stem: match.1, kind: .rarParts, index: index)
        } else if let match = fileName.wholeMatch(of: /(.+)\.r(\d\d)/.ignoresCase()) {
            self.init(fileName, stem: match.1, kind: .rarOldStyle, index: Int(match.2)! + 1)
        } else if let match = fileName.wholeMatch(of: /(.+)\.rar/.ignoresCase()) {
            self.init(fileName, stem: match.1, kind: .rarOldStyle, index: 0)
        } else if let match = fileName.wholeMatch(of: /(.+)\.(zip|7z)\.(\d{3})/.ignoresCase()) {
            self.init(fileName, stem: match.1, kind: .split, index: Int(match.3)!, format: match.2)
        } else if let match = fileName.wholeMatch(of: /(.+)\.(?:zip|7z)/.ignoresCase()) {
            self.init(fileName, stem: match.1, kind: .single, index: 0)
        } else {
            return nil
        }
    }

    private init(_ fileName: String, stem: Substring, kind: Kind, index: Int, format: Substring = "") {
        self.stem = stem.trimmingCharacters(in: .whitespaces)
        self.kind = kind
        self.index = index
        // A lone `game.rar` is old-style volume 0 until a `game.r00` shows up, so both share one key. Split parts keep
        // their format, so `game.zip.001` and `game.7z.001` are two archives.
        set = "\(stem.lowercased())|\(kind == .single ? fileName.lowercased() : "\(kind)\(format.lowercased())")"
    }
}

/// Checks a folder name the user typed for a new game.
nonisolated enum GameFolderName {
    /// The reason the name can't be used in `gamesRoot`, or nil when it can.
    static func problem(with name: String, in gamesRoot: URL) -> String? {
        let name = name.trimmingCharacters(in: .whitespaces)
        if name.isEmpty { return "Enter a folder name." }
        if name.contains("/") || name.contains(":") { return "A folder name can't contain / or :." }
        if name.hasPrefix(".") { return "A folder name can't start with a period." }
        let existing = (try? FileManager.default.contentsOfDirectory(atPath: gamesRoot.path)) ?? []
        // Compared without case because the default APFS volume treats "Game" and "game" as one folder.
        if existing.contains(where: { $0.caseInsensitiveCompare(name) == .orderedSame }) {
            return "Games already has a folder named \"\(name)\". Choose another name."
        }
        return nil
    }
}
