import Foundation
import VNCore
import WebRuntime

extension RPGMakerPlugin {
    /// NW.js builds save with `fs` to `<web root>/save/`, the folder holding index.html. Resolved the way the game's own
    /// `fs` calls are, so a backup or restore touches the same folder the game does.
    nonisolated public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let directory = GameFolder(rpgMakerGameAt: folder).saveDirectory else { return [] }
        return [SaveLocation(directory: directory, relativePath: Self.saveRelativePath)]
    }

    /// `/home/deck/.../common/NoSocksRPG/save/file0.rmmzsave` maps to `file0.rmmzsave`. Saves sit directly in a
    /// `save/` folder that has an install directory above it.
    nonisolated static func saveRelativePath(_ path: String) -> String? {
        let parts = path.split(separator: "/")
        guard parts.count >= 3, parts[parts.count - 2].lowercased() == "save", let name = parts.last else { return nil }
        let lower = name.lowercased()
        guard lower.hasSuffix(".rmmzsave") || lower.hasSuffix(".rpgsave") else { return nil }
        return String(name)
    }
}

/// What `GameFolder.moveCommunityCoreSaves` found and did.
nonisolated enum LegacySaveMove: Equatable, Sendable {
    /// Not a community-core game, or no save slot waits in the top-level save/.
    case nothing
    /// These files moved from save/ into the web root's save/. `mergedIndex` is true when the slot index there now lists
    /// the moved slots too.
    case moved([String], mergedIndex: Bool)
    /// Both folders hold saves that can't be merged without hiding or replacing one, so these stay in save/.
    case keptBoth([String])
}

nonisolated extension GameFolder {
    /// MV's community core (`rpg_managers.js` "community-1.3b") saves to `<web root>/save/` when it can make and then
    /// remove `<web root>/testDirectory/`, and to `save/` beside the web root when it can't. Before the shim had
    /// `rmdirSync` the check always failed in VNDegen, so these games saved to the top-level `save/` here and to
    /// `www/save/` on the PC, and every launch left an empty `testDirectory/` that fails the check on any machine. This
    /// removes that folder and moves the saves to where the game now looks, before the game starts.
    ///
    /// Games sometimes ship saves in `www/save/`: Rookie Knight Rathi ships slot 6 and its `global.rpgsave`, the index
    /// MV lists slots from. So the saves move whenever no slot is in both folders, and the game's index gets the moved
    /// slots' entries, as if the player had saved there. A moved save the index didn't list would show as an empty slot,
    /// and saving to that slot would replace it. When a slot is in both folders, or only `www/save/` has an index, or an
    /// index can't be read, both folders stay as they are and the game uses `www/save/`. No save is overwritten or
    /// deleted. A file other than a slot or the index, such as `config.rpgsave`, stays in save/ when `www/save/` has one.
    @discardableResult
    func moveCommunityCoreSaves() -> LegacySaveMove {
        guard managersScript.contains("canMakeWwwSaveDirectory") else { return .nothing }
        // rmdir(2) removes only an empty folder, and refuses a symlink.
        if let probe = resolve(virtualPath: "\(virtualWebRoot)/testDirectory"), rmdir(probe.path) == 0 {
            rpgLog.notice("removed the empty testDirectory an earlier VNDegen left in the web root")
        }
        let fm = FileManager.default
        guard !webRoot.isEmpty, let target = saveDirectory,
              let legacy = resolve(virtualPath: (virtualWebRoot as NSString).deletingLastPathComponent + "/save"),
              let names = try? fm.contentsOfDirectory(atPath: legacy.path) else { return .nothing }
        // Dot files are staging files a failed write left behind.
        let files = names.filter { name in
            !name.hasPrefix(".")
                && (try? fm.attributesOfItem(atPath: legacy.appendingPathComponent(name).path))?[.type] as? FileAttributeType == .typeRegular
        }.sorted()
        let slots = files.compactMap(Self.slot)
        guard !slots.isEmpty else { return .nothing }
        // Names on disk, by their lowercase form: Windows ignores case, so `FILE1.rpgsave` is slot 1 too.
        let existing = Dictionary(((try? fm.contentsOfDirectory(atPath: target.path)) ?? []).map { ($0.lowercased(), $0) }) { a, _ in a }
        let ourIndex = files.first { $0.lowercased() == Self.indexName }
        let theirIndex = existing[Self.indexName]
        let keepBoth = { (why: String) -> LegacySaveMove in
            rpgLog.notice("saves in both save/ and \(self.webRoot, privacy: .public)/save/ (\(why, privacy: .public)), keeping both and using \(self.webRoot, privacy: .public)/save/: \(files.joined(separator: ", "), privacy: .public)")
            return .keptBoth(files)
        }
        if let clash = Set(slots).intersection(existing.values.compactMap(Self.slot)).min() { return keepBoth("slot \(clash) is in both") }
        var merged: Data?
        if let theirIndex {
            guard let ourIndex else { return keepBoth("only \(webRoot)/save/ has an index") }
            guard let ours = try? String(contentsOf: legacy.appendingPathComponent(ourIndex), encoding: .utf8),
                  let theirs = try? String(contentsOf: target.appendingPathComponent(theirIndex), encoding: .utf8),
                  let index = Self.mergedIndex(ours, into: theirs, slots: slots) else { return keepBoth("an index can't be read") }
            merged = Data(index.utf8)
        }
        do {
            try fm.createDirectory(at: target, withIntermediateDirectories: true)
            // The index goes first, so a save that then fails to move is never one the index left out. MV drops an
            // entry whose file is missing.
            if let merged, let theirIndex { try merged.write(to: target.appendingPathComponent(theirIndex), options: .atomic) }
        } catch {
            rpgLog.error("could not prepare \(target.path, privacy: .public): \(error.localizedDescription, privacy: .public)")
            return .nothing
        }
        var moved: [String] = []
        for name in files where existing[name.lowercased()] == nil {
            do {
                // moveItem refuses to replace a file, so a save that appeared since the check stays.
                try fm.moveItem(at: legacy.appendingPathComponent(name), to: target.appendingPathComponent(name))
                moved.append(name)
            } catch {
                rpgLog.error("could not move save/\(name, privacy: .public): \(error.localizedDescription, privacy: .public)")
            }
        }
        // Only when it is empty, so whatever stayed behind is still there.
        rmdir(legacy.path)
        rpgLog.notice("moved saves from save/ to \(self.webRoot, privacy: .public)/save/\(merged == nil ? "" : " and merged the slot index", privacy: .public): \(moved.joined(separator: ", "), privacy: .public)")
        return .moved(moved, mergedIndex: merged != nil)
    }

    private static let indexName = "global.rpgsave"

    /// The slot of `fileN.rpgsave`, or of `fileN.rpgsave.bak`, the backup the community core keeps while it saves.
    private static func slot(_ name: String) -> Int? {
        var lower = name.lowercased()
        if lower.hasSuffix(".bak") { lower.removeLast(4) }
        guard lower.hasPrefix("file"), lower.hasSuffix(".rpgsave") else { return nil }
        return Int(lower.dropFirst(4).dropLast(8))
    }

    /// `theirs`, an MV slot index, with the entries for `slots` taken from `ours`. The index is the lz-string base64 of a
    /// JSON array whose entry N describes slot N: its title, playtime, and when it was saved. Nil when either can't be
    /// read.
    static func mergedIndex(_ ours: String, into theirs: String, slots: [Int]) -> String? {
        func entries(_ text: String) -> [Any]? {
            LZString.decompressFromBase64(text.trimmingCharacters(in: .whitespacesAndNewlines))
                .flatMap { try? JSONSerialization.jsonObject(with: Data($0.utf8)) as? [Any] }
        }
        guard let mine = entries(ours), var index = entries(theirs) else { return nil }
        for slot in slots where slot < mine.count && !(mine[slot] is NSNull) {
            if index.count <= slot { index += Array(repeating: NSNull(), count: slot + 1 - index.count) }
            index[slot] = mine[slot]
        }
        guard let json = try? JSONSerialization.data(withJSONObject: index, options: .withoutEscapingSlashes) else { return nil }
        return LZString.compressToBase64(String(decoding: json, as: UTF8.self))
    }
}
