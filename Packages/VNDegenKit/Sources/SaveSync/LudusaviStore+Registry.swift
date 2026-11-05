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
    static func writeLocalRegistry(_ values: [String: RegistryValue], to save: RegistrySave) throws {
        try FileManager.default.createDirectory(at: save.file.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data(registryFile(values, key: save.key).regText.utf8).write(to: save.file, options: .atomic)
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
                    if let value = registryValue(entry.value) { values[entry.key] = value }
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

    /// Nil for kinds Ludusavi reads as unknown, which it leaves out of a restore.
    private static func registryValue(_ node: YAML) -> RegistryValue? {
        guard let entry = node.entries.first, node.entries.count == 1 else { return nil }
        let bytes = { (node: YAML) -> [UInt8]? in
            let items = node.items.compactMap { $0.string.flatMap { UInt8($0) } }
            return items.count == node.items.count ? items : nil
        }
        switch entry.key {
        case "sz": return entry.value.string.map(RegistryValue.sz)
        case "expandSz": return entry.value.string.map(RegistryValue.expandSz)
        case "multiSz": return entry.value.string.map(RegistryValue.multiSz)
        case "dword": return entry.value.string.flatMap { UInt32($0) }.map(RegistryValue.dword)
        case "qword": return entry.value.string.flatMap { UInt64($0) }.map(RegistryValue.qword)
        case "binary": return bytes(entry.value).map(RegistryValue.binary)
        case "raw":
            guard let kind = entry.value["kind"]?.string.flatMap({ rawKinds[$0] }), let data = entry.value["data"].flatMap(bytes) else { return nil }
            return .raw(kind: kind, bytes: data)
        default: return nil
        }
    }
}
