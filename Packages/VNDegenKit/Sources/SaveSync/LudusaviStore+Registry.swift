import Foundation
import RegistryFile
import VNCore

/// The registry values a game keeps, such as a Unity game's PlayerPrefs, as `RegistrySave` names them. Only that one key
/// moves: a restore takes its values from a backup into the game's file here, and a backup puts the file's values back
/// under that key, with every other key the backup holds kept as it was.
extension LudusaviStore {
    /// The key `localHashes` and `latestHashes` give the registry values, beside the `location:relative` of each save.
    static let registryHashKey = "registry"

    /// The registry data a restore of `backup` applies, as Ludusavi's `registry_content` finds it: a differential's own,
    /// else nothing when it drops the registry, else its full backup's. Nil for the latest is `latestRegistry`.
    static func storedRegistry(_ mapping: LudusaviMapping, backup: String?, in gameDir: URL) -> StoredRegistry? {
        guard let backup else { return latestRegistry(mapping, in: gameDir) }
        for full in mapping.backups {
            if full.name == backup { return stored(full.registryHash, in: full.name, gameDir: gameDir) }
            guard let diff = full.children.first(where: { $0.name == backup }) else { continue }
            if let override = diff.registry {
                if let found = stored(override.hash, in: diff.name, gameDir: gameDir) { return found }
                if override.hash == nil { return nil }
            }
            return stored(full.registryHash, in: full.name, gameDir: gameDir)
        }
        return nil
    }

    /// The registry files the latest backup records when none of them can be read, such as a damaged `registry.reg`,
    /// so a backup carries them forward byte for byte with their recorded hash rather than dropping them. Nil when it
    /// records none, when its files are missing, or when `latestRegistry` finds a readable one.
    static func unreadableRegistry(_ mapping: LudusaviMapping, in gameDir: URL) -> StoredRegistry? {
        guard let full = mapping.backups.last else { return nil }
        func present(_ hash: String?, in backup: String) -> StoredRegistry? {
            guard let hash, !backup.hasSuffix(".zip") else { return nil }
            let folder = backup == "." ? gameDir : gameDir.appendingPathComponent(backup, isDirectory: true)
            let files = registryFiles.filter { FileManager.default.fileExists(atPath: folder.appendingPathComponent($0).path) }
            return files.isEmpty ? nil : StoredRegistry(hash: hash, backup: backup, files: files)
        }
        if let diff = full.children.last, let override = diff.registry {
            if let found = present(override.hash, in: diff.name) { return found }
            if override.hash == nil { return nil }
        }
        return present(full.registryHash, in: full.name)
    }

    /// True when the latest backup records registry data, whether or not its files can be read, as `latestRegistry`
    /// would look for it.
    static func recordsRegistry(_ mapping: LudusaviMapping) -> Bool {
        guard let full = mapping.backups.last else { return false }
        // A differential that drops the registry records none. One that names a hash records one, and so does a full
        // backup with a hash, which a differential without an override inherits.
        if let override = full.children.last?.registry { return override.hash != nil }
        return full.registryHash != nil
    }

    /// What a backup holds, read as Ludusavi reads it: the first of `registryFiles` that parses. Nil when none does, or
    /// the backup is a zip.
    static func registryData(_ stored: StoredRegistry, in gameDir: URL) -> RegistryData? {
        guard !stored.backup.hasSuffix(".zip") else { return nil }
        let folder = stored.backup == "." ? gameDir : gameDir.appendingPathComponent(stored.backup, isDirectory: true)
        for name in stored.files {
            guard let data = try? Data(contentsOf: folder.appendingPathComponent(name)) else { continue }
            if name.hasSuffix(".yaml") {
                if let text = String(data: data, encoding: .utf8), let parsed = try? registryData(yaml: text) { return parsed }
            } else if let parsed = try? RegistryData(reg: data) {
                return parsed
            }
        }
        return nil
    }

    /// The values `save` keeps in a restore of `backup`. Nil when that backup holds none for its key.
    static func backupRegistryValues(_ mapping: LudusaviMapping, backup: String?, save: RegistrySave, in gameDir: URL) -> [String: RegistryValue]? {
        storedRegistry(mapping, backup: backup, in: gameDir).flatMap { registryData($0, in: gameDir) }?.values(of: save.key)
    }

    /// The values in the game's file here. Nil when there is no file. A file holding anything but its key is refused,
    /// so no other key ever reaches a backup.
    static func localRegistryValues(_ save: RegistrySave) throws -> [String: RegistryValue]? {
        let data: Data
        do {
            data = try Data(contentsOf: save.file)
        } catch CocoaError.fileReadNoSuchFile {
            return nil
        } catch {
            throw SaveSyncError.unreadable(save.file.path)
        }
        guard let parsed = try? RegistryData(reg: data), parsed.keys.count <= 1 else { throw SaveSyncError.unreadable(save.file.path) }
        guard let values = parsed.values(of: save.key) else {
            if parsed.isEmpty { return [:] }
            throw SaveSyncError.unreadable(save.file.path)
        }
        return values
    }

    /// Writes `values` as the game's file, holding its key alone.
    /// Writes `values` as the game's file, holding its key alone. A value whose name breaks a line is left out, as the
    /// file goes to Wine's regedit, which couldn't read it as one value.
    static func writeLocalRegistry(_ values: [String: RegistryValue], to save: RegistrySave) throws {
        try FileManager.default.createDirectory(at: save.file.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data(registryFile(RegistryData.writable(values), key: save.key).regText.utf8).write(to: save.file, options: .atomic)
    }

    static func registryFile(_ values: [String: RegistryValue], key: String) -> RegistryData {
        var file = RegistryData()
        file.setValues(values, of: key)
        return file
    }

    /// Compares the key's values here and in a backup, whatever case either spells the key in.
    static func registryHash(_ values: [String: RegistryValue], key: String) -> String {
        sha1(Data(registryFile(values, key: key).regText.utf8))
    }

    // MARK: registry.yaml

    /// Ludusavi's `registry.yaml`, from versions before it wrote `registry.reg`: hives, then keys, then values, each value a
    /// one-entry mapping of its kind, as serde_yaml 0.8 writes Ludusavi's `Entry`.
    static func registryData(yaml text: String) throws -> RegistryData {
        // serde_yaml reads anything but a mapping as no registry at all.
        guard case let .mapping(hives) = try YAML.parse(text) else { throw YAMLError(line: 1, reason: "registry data isn't a mapping") }
        var data = RegistryData()
        for hive in hives {
            for key in hive.value.entries {
                var values: [String: RegistryValue] = [:]
                for entry in key.value.entries {
                    if let value = try registryValue(entry.value) { values[entry.key] = value }
                }
                data.setValues(values, of: "\(hive.key)\\\(key.key)")
            }
        }
        return data
    }

    private static let rawKinds: [String: UInt8] = [
        "none": 0, "sz": 1, "expandSz": 2, "binary": 3, "dword": 4, "dwordBigEndian": 5, "link": 6, "multiSz": 7,
        "resourceList": 8, "fullResourceDescriptor": 9, "resourceRequirementsList": 0xa, "qword": 0xb,
    ]

    /// Nil for kinds Ludusavi reads as unknown, which it leaves out of a restore. A known kind with data that doesn't
    /// fit it throws, as serde fails Ludusavi's whole file then, so a damaged differential gives way to the full backup.
    private static func registryValue(_ node: YAML) throws -> RegistryValue? {
        let bad = YAMLError(line: 0, reason: "a registry value doesn't fit its kind")
        guard case let .mapping(entries) = node, let entry = entries.first, entries.count == 1 else { throw bad }
        func bytes(_ node: YAML?) throws -> [UInt8] {
            guard case let .sequence(items)? = node else { throw bad }
            return try items.map { item in
                guard let byte = item.string.flatMap({ UInt8($0) }) else { throw bad }
                return byte
            }
        }
        func text() throws -> String {
            guard let text = entry.value.string else { throw bad }
            return text
        }
        switch entry.key {
        case "sz": return .sz(try text())
        case "expandSz": return .expandSz(try text())
        case "multiSz": return .multiSz(try text())
        case "dword":
            guard let number = UInt32(try text()) else { throw bad }
            return .dword(number)
        case "qword":
            guard let number = UInt64(try text()) else { throw bad }
            return .qword(number)
        case "binary": return .binary(try bytes(entry.value))
        case "raw":
            guard let kind = entry.value["kind"]?.string.flatMap({ rawKinds[$0] }) else { throw bad }
            return .raw(kind: kind, bytes: try bytes(entry.value["data"]))
        default: return nil
        }
    }
}
