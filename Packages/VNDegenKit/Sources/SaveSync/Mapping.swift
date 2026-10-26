import Foundation

/// A game's mapping.yaml in a Ludusavi backup folder. Mirrors Ludusavi's `IndividualMapping` (src/scan/layout.rs).
/// Fields Ludusavi itself does not know are dropped on write, as Ludusavi does.
public struct LudusaviMapping: Equatable, Sendable {
    /// The game's Ludusavi title. The folder name is this title with unsafe characters replaced.
    public var name: String
    /// Drive folder name, such as `drive-0`, to the drive it stands for (`""` for `/`, `C:` on Windows).
    public var drives: [String: String]
    /// Oldest first, as Ludusavi keeps them.
    public var backups: [FullBackup]

    public init(name: String, drives: [String: String] = [:], backups: [FullBackup] = []) {
        self.name = name
        self.drives = drives
        self.backups = backups
    }
}

public struct FileEntry: Equatable, Hashable, Sendable {
    /// Lowercase hex SHA-1 of the content.
    public var hash: String
    public var size: UInt64
}

public struct FullBackup: Equatable, Sendable {
    /// Folder name inside the game folder, such as `backup-20260923T022236Z`. `.` means the game folder itself.
    public var name: String
    /// RFC 3339 timestamp, kept as written so a rewrite does not change its precision.
    public var when: String
    public var os: String?
    public var comment: String?
    public var locked: Bool
    /// Wine prefix metadata from Ludusavi. Passed through untouched.
    var semantics: YAML?
    /// Original absolute path, with forward slashes, to its content.
    public var files: [String: FileEntry]
    public var registryHash: String?
    /// Oldest first. Each one is relative to this full backup, not to the previous differential.
    public var children: [DifferentialBackup]
}

public struct DifferentialBackup: Equatable, Sendable {
    public var name: String
    public var when: String
    public var os: String?
    public var comment: String?
    public var locked: Bool
    var semantics: YAML?
    /// Files that differ from the full backup. A path missing here is inherited from the full backup.
    public var files: [String: Change]
    /// Nil inherits the full backup's registry. A value with a nil hash means the registry was dropped.
    public var registry: RegistryOverride?

    public enum Change: Equatable, Sendable {
        case changed(FileEntry)
        /// The file existed in the full backup but not when this backup ran.
        case removed
    }

    public struct RegistryOverride: Equatable, Sendable {
        public var hash: String?
    }
}

/// One backup the user can restore, flattened the way Ludusavi lists them.
public struct BackupSummary: Identifiable, Hashable, Sendable {
    public var name: String
    public var when: Date?
    public var os: String?
    public var comment: String?
    public var isDifferential: Bool
    /// Files a restore of this backup writes.
    public var fileCount: Int
    /// For a differential backup, the files it changed or removed relative to its full backup.
    public var changedCount: Int
    public var id: String { name }
}

/// A file a backup holds, and the backup folder its bytes live in.
struct StoredFile: Equatable, Sendable {
    var entry: FileEntry
    var backup: String
}

extension LudusaviMapping {
    /// Every backup, oldest first. Each full backup comes before its differentials.
    public var summaries: [BackupSummary] {
        backups.flatMap { full in
            [BackupSummary(name: full.name, when: Timestamp.parse(full.when), os: full.os, comment: full.comment,
                           isDifferential: false, fileCount: full.files.count, changedCount: 0)]
            + full.children.map { diff in
                BackupSummary(name: diff.name, when: Timestamp.parse(diff.when), os: diff.os, comment: diff.comment,
                              isDifferential: true, fileCount: effectiveFiles(full: full, diff: diff).count,
                              changedCount: diff.files.count)
            }
        }
    }

    /// The files a restore of `backup` writes, by original path. Nil picks the latest, as Ludusavi does: the last
    /// full backup plus its last differential. Matches `GameLayout::restorable_files`.
    func effectiveFiles(_ backup: String? = nil) -> [String: StoredFile]? {
        guard let backup else {
            guard let full = backups.last else { return nil }
            return effectiveFiles(full: full, diff: full.children.last)
        }
        for full in backups {
            if full.name == backup { return effectiveFiles(full: full, diff: nil) }
            if let diff = full.children.first(where: { $0.name == backup }) { return effectiveFiles(full: full, diff: diff) }
        }
        return nil
    }

    private func effectiveFiles(full: FullBackup, diff: DifferentialBackup?) -> [String: StoredFile] {
        var files = full.files.mapValues { StoredFile(entry: $0, backup: full.name) }
        guard let diff else { return files }
        for (path, change) in diff.files {
            switch change {
            case let .changed(entry): files[path] = StoredFile(entry: entry, backup: diff.name)
            case .removed: files[path] = nil
            }
        }
        return files
    }

    /// Ludusavi's `forget_excess_backups`: drops the oldest unlocked backups beyond `retention`'s limits. A full backup
    /// with a locked differential counts as locked. Returns the folders that held them, which Ludusavi then deletes.
    mutating func forgetExcessBackups(_ retention: Retention) -> [String] {
        let isLocked = { (full: FullBackup) in full.locked || full.children.contains(where: \.locked) }
        var excessFulls = max(0, backups.filter { !isLocked($0) }.count - retention.full)
        var removed: [String] = []
        var kept: [FullBackup] = []
        for var full in backups {
            if !isLocked(full), excessFulls > 0 {
                excessFulls -= 1
                removed += [full.name] + full.children.map(\.name)
                continue
            }
            var excessDiffs = max(0, full.children.filter { !$0.locked }.count - retention.differential)
            full.children.removeAll { diff in
                guard !diff.locked, excessDiffs > 0 else { return false }
                excessDiffs -= 1
                removed.append(diff.name)
                return true
            }
            kept.append(full)
        }
        backups = kept
        // A full backup named `.` keeps its files in the drive folders at the top of the game folder.
        return removed.flatMap { $0 == "." ? drives.keys.sorted() : [$0] }
    }

    /// Where a backed-up file's bytes are, relative to the game folder: `<backup>/<drive folder>/<path without drive>`.
    func storedPath(of path: String, in backup: String) -> String? {
        guard let (drive, plain) = Self.splitDrive(path) else { return nil }
        let folder = drives.first { $0.value == drive }?.key ?? Self.driveFolderName(drive)
        return backup == "." ? "\(folder)/\(plain)" : "\(backup)/\(folder)/\(plain)"
    }

    /// `/home/deck/x` is drive `""` and `home/deck/x`. `C:/Users/x` is drive `C:` and `Users/x`.
    static func splitDrive(_ path: String) -> (String, String)? {
        if path.hasPrefix("/"), !path.hasPrefix("//") { return ("", String(path.dropFirst())) }
        let chars = Array(path.prefix(3))
        if chars.count == 3, chars[0].isLetter, chars[1] == ":", chars[2] == "/" {
            return (String(path.prefix(2)), String(path.dropFirst(3)))
        }
        return nil
    }

    static func driveFolderName(_ drive: String) -> String {
        drive.isEmpty ? "drive-0" : "drive-" + escapeFolderName(drive.replacingOccurrences(of: ":", with: ""))
    }

    /// Ludusavi's `escape_folder_name`, used for game folders and drive folders.
    public static func escapeFolderName(_ name: String) -> String {
        var escaped = name
        if escaped.hasPrefix(".") { escaped = "_" + escaped.dropFirst() }
        if escaped.hasSuffix(".") { escaped = escaped.dropLast() + "_" }
        let invalid: Set<Character> = ["\\", "/", ":", "*", "?", "\"", "<", ">", "|", "\0"]
        return String(escaped.map { invalid.contains($0) ? "_" : $0 })
    }
}

// MARK: - YAML

extension LudusaviMapping {
    public init(yaml text: String) throws {
        let root = try YAML.parse(text)
        guard let name = root["name"]?.string else { throw YAMLError(line: 1, reason: "no game name") }
        self.name = name
        drives = Dictionary(root["drives"]?.entries.map { ($0.key, $0.value.string ?? "") } ?? [], uniquingKeysWith: { a, _ in a })
        backups = try (root["backups"]?.items ?? []).map(FullBackup.init(yaml:))
    }

    public func yamlString() -> String {
        YAML.mapping([
            .init(key: "name", value: .scalar(name)),
            .init(key: "drives", value: .mapping(drives.sortedByKey.map { .init(key: $0.key, value: .scalar($0.value)) })),
            .init(key: "backups", value: .sequence(backups.map(\.yaml))),
        ]).serialized()
    }
}

extension FullBackup {
    init(yaml node: YAML) throws {
        guard let name = node["name"]?.string else { throw YAMLError(line: 0, reason: "backup without a name") }
        self.name = name
        when = node["when"]?.string ?? Timestamp.epoch
        os = node["os"]?.string
        comment = node["comment"]?.string
        locked = node["locked"]?.string == "true"
        semantics = node["semantics"]
        files = try Dictionary(uniqueKeysWithValues: (node["files"]?.entries ?? []).map { ($0.key, try FileEntry(yaml: $0.value)) })
        registryHash = node["registry"]?["hash"]?.string
        children = try (node["children"]?.items ?? []).map(DifferentialBackup.init(yaml:))
    }

    var yaml: YAML {
        .mapping(BackupFields.header(name: name, when: when, os: os, comment: comment, locked: locked, semantics: semantics) + [
            .init(key: "files", value: .mapping(files.sortedByKey.map { .init(key: $0.key, value: $0.value.yaml) })),
            .init(key: "registry", value: .mapping([.init(key: "hash", value: registryHash.map(YAML.scalar) ?? .null)])),
            .init(key: "children", value: .sequence(children.map(\.yaml))),
        ])
    }
}

extension DifferentialBackup {
    init(yaml node: YAML) throws {
        guard let name = node["name"]?.string else { throw YAMLError(line: 0, reason: "backup without a name") }
        self.name = name
        when = node["when"]?.string ?? Timestamp.epoch
        os = node["os"]?.string
        comment = node["comment"]?.string
        locked = node["locked"]?.string == "true"
        semantics = node["semantics"]
        files = try Dictionary(uniqueKeysWithValues: (node["files"]?.entries ?? []).map { entry in
            (entry.key, entry.value == .null ? Change.removed : .changed(try FileEntry(yaml: entry.value)))
        })
        registry = switch node["registry"] {
        case nil, .null?: nil
        case let value?: RegistryOverride(hash: value["hash"]?.string)
        }
    }

    var yaml: YAML {
        .mapping(BackupFields.header(name: name, when: when, os: os, comment: comment, locked: locked, semantics: semantics) + [
            .init(key: "files", value: .mapping(files.sortedByKey.map { path, change in
                switch change {
                case let .changed(entry): .init(key: path, value: entry.yaml)
                case .removed: .init(key: path, value: .null)
                }
            })),
            .init(key: "registry", value: registry.map { .mapping([.init(key: "hash", value: $0.hash.map(YAML.scalar) ?? .null)]) } ?? .null),
        ])
    }
}

extension FileEntry {
    init(yaml node: YAML) throws {
        guard let hash = node["hash"]?.string, let size = node["size"]?.string.flatMap({ UInt64($0) }) else {
            throw YAMLError(line: 0, reason: "file entry without hash and size")
        }
        self.hash = hash
        self.size = size
    }

    var yaml: YAML {
        .mapping([.init(key: "hash", value: .scalar(hash)), .init(key: "size", value: .literal(String(size)))])
    }
}

/// Field order and omission rules shared by both backup kinds, from their serde attributes.
private enum BackupFields {
    static func header(name: String, when: String, os: String?, comment: String?, locked: Bool, semantics: YAML?) -> [YAML.Entry] {
        var fields: [YAML.Entry] = [.init(key: "name", value: .scalar(name)), .init(key: "when", value: .scalar(when))]
        if let os { fields.append(.init(key: "os", value: .scalar(os))) }
        if let comment { fields.append(.init(key: "comment", value: .scalar(comment))) }
        if locked { fields.append(.init(key: "locked", value: .literal("true"))) }
        if let semantics, semantics["directories"]?.entries.isEmpty == false {
            fields.append(.init(key: "semantics", value: semantics))
        }
        return fields
    }
}

extension Dictionary where Key == String {
    /// Rust's BTreeMap order: by UTF-8 bytes.
    var sortedByKey: [(key: String, value: Value)] {
        sorted { $0.key.utf8.lexicographicallyPrecedes($1.key.utf8) }
    }
}

enum Timestamp {
    static let epoch = "1970-01-01T00:00:00Z"

    /// Accepts any number of fractional digits. chrono writes up to nine, more than ISO8601DateFormatter reads.
    static func parse(_ text: String) -> Date? {
        guard text.count >= 20 else { return nil }
        let seconds = String(text.prefix(19))
        var rest = text.dropFirst(19)
        var fraction = 0.0
        if rest.first == "." {
            let digits = rest.dropFirst().prefix { $0.isNumber }
            fraction = Double("0." + digits) ?? 0
            rest = rest.dropFirst(1 + digits.count)
        }
        let formatter = ISO8601DateFormatter()
        return formatter.date(from: seconds + rest).map { $0.addingTimeInterval(fraction) }
    }

    /// RFC 3339 with milliseconds, which chrono reads back.
    static func format(_ date: Date) -> String {
        date.formatted(Date.ISO8601FormatStyle(includingFractionalSeconds: true))
    }

    /// Ludusavi's folder-name timestamp, `20261025T164619Z`.
    static func folderStamp(_ date: Date) -> String {
        date.formatted(Date.ISO8601FormatStyle(dateSeparator: .omitted, dateTimeSeparator: .standard, timeSeparator: .omitted))
    }
}
