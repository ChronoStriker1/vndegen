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
    /// Not a community-core game, the core would still pick save/, or no save slot waits in save/.
    case nothing
    /// These files moved from save/ into the web root's save/, whose slot index now lists the moved slots.
    case moved([String])
    /// The saves can't move without hiding or replacing one, so these stay in save/.
    case keptBoth([String])
}

nonisolated extension GameFolder {
    /// MV's community core (`rpg_managers.js` "community-1.3b") saves to `<web root>/save/` when it can make and then
    /// remove `<web root>/testDirectory/`, and to `save/` beside the web root when it can't. Before the shim had
    /// `rmdirSync` the check always failed in VNDegen, so these games saved to the top-level `save/` here and to
    /// `www/save/` on the PC, and every launch left an empty `testDirectory/` that fails the check on any machine. This
    /// removes that folder and moves the saves to where the game now looks, before the game starts. When something
    /// else is at `testDirectory`, the game keeps using `save/`, so nothing moves.
    ///
    /// Games sometimes ship saves in `www/save/`: Rookie Knight Rathi ships slot 6 and its `global.rpgsave`, the index
    /// MV lists slots from. A save the index doesn't list shows as an empty slot, and saving to that slot replaces it.
    /// So the saves move whenever no slot is in both folders and save/'s index describes every slot that moves. The
    /// index in `www/save/` gets their entries first, so a move that stops partway leaves saves the index lists, and
    /// the next launch finishes it. Otherwise both folders stay as they are and the game uses `www/save/`. No save is
    /// overwritten or deleted. A file other than a slot or the index, such as `config.rpgsave`, stays in save/ when
    /// `www/save/` has one.
    ///
    /// This runs before the page loads, so nothing in the game can change the folders while it works.
    @discardableResult
    func moveCommunityCoreSaves() -> LegacySaveMove {
        guard managersScript.contains("canMakeWwwSaveDirectory") else { return .nothing }
        let fm = FileManager.default
        if let probe = resolve(virtualPath: "\(virtualWebRoot)/testDirectory") {
            // rmdir(2) removes only an empty folder, and refuses a symlink.
            if rmdir(probe.path) == 0 { rpgLog.notice("removed the empty testDirectory an earlier VNDegen left in the web root") }
            var info = stat()
            if lstat(probe.path, &info) == 0 {
                rpgLog.notice("something is at testDirectory in the web root, so the game keeps saving to save/")
                return .nothing
            }
        }
        guard !webRoot.isEmpty, let target = saveDirectory,
              let legacy = resolve(virtualPath: (virtualWebRoot as NSString).deletingLastPathComponent + "/save"),
              let names = try? fm.contentsOfDirectory(atPath: legacy.path) else { return .nothing }
        let isFile = { (url: URL) in (try? fm.attributesOfItem(atPath: url.path))?[.type] as? FileAttributeType == .typeRegular }
        // Dot files are staging files a failed write left behind.
        let files = names.filter { !$0.hasPrefix(".") && isFile(legacy.appendingPathComponent($0)) }.sorted()
        let slots = files.compactMap(Self.slot)
        guard !slots.isEmpty else { return .nothing }
        let present = (try? fm.contentsOfDirectory(atPath: target.path)) ?? []
        let existing = Set(present.map { $0.lowercased() })
        let keepBoth = { (why: String) -> LegacySaveMove in
            rpgLog.notice("saves in save/ can't move to \(self.webRoot, privacy: .public)/save/ (\(why, privacy: .public)), keeping both and using \(self.webRoot, privacy: .public)/save/: \(files.joined(separator: ", "), privacy: .public)")
            return .keptBoth(files)
        }
        if let clash = Set(slots).intersection(present.compactMap(Self.slot)).min() { return keepBoth("slot \(clash) is in both") }

        // Windows ignores case, so `GLOBAL.rpgsave` is the index too. Where a case-sensitive disk holds two spellings,
        // which one the game reads depends on the spelling it asks for, so neither is chosen.
        let ourIndexes = files.filter { $0.lowercased() == Self.indexName }
        let theirIndexes = present.filter { $0.lowercased() == Self.indexName }
        guard ourIndexes.count <= 1, theirIndexes.count <= 1 else { return keepBoth("an index is there in two spellings") }
        let ourIndex = ourIndexes.first, theirIndex = theirIndexes.first
        var index: Data?
        if let ourIndex {
            // The index there must be a file in the game folder, not a symlink that leads anywhere else.
            let theirs: String?
            if let theirIndex {
                let url = target.appendingPathComponent(theirIndex)
                theirs = isFile(url) ? try? String(contentsOf: url, encoding: .utf8) : nil
            } else {
                theirs = LZString.compressToBase64("[]")
            }
            let saves = files.filter { !$0.lowercased().hasSuffix(".bak") }.compactMap(Self.slot)
            guard let ours = try? String(contentsOf: legacy.appendingPathComponent(ourIndex), encoding: .utf8), let theirs,
                  let merged = Self.mergedIndex(ours, into: theirs, slots: saves) else {
                return keepBoth("an index can't be read or doesn't describe every slot")
            }
            index = Data(merged.utf8)
        } else if theirIndex != nil {
            return keepBoth("only \(webRoot)/save/ has an index")
        }

        do {
            try fm.createDirectory(at: target, withIntermediateDirectories: true)
            if let index { try index.write(to: target.appendingPathComponent(theirIndex ?? Self.indexName), options: .atomic) }
        } catch {
            rpgLog.error("could not prepare \(target.path, privacy: .public): \(error.localizedDescription, privacy: .public)")
            return .nothing
        }
        var moved: [String] = [], failed = false
        for name in files where name != ourIndex && !existing.contains(name.lowercased()) {
            do {
                // moveItem refuses to replace a file, so a save that appeared since the check stays.
                try fm.moveItem(at: legacy.appendingPathComponent(name), to: target.appendingPathComponent(name))
                moved.append(name)
            } catch {
                failed = true
                rpgLog.error("could not move save/\(name, privacy: .public): \(error.localizedDescription, privacy: .public)")
            }
        }
        // save/'s index is in www/save/'s now. It stays while a save it describes is still in save/.
        if !failed, let ourIndex { try? fm.removeItem(at: legacy.appendingPathComponent(ourIndex)) }
        // Only when it is empty, so whatever stayed behind is still there.
        rmdir(legacy.path)
        rpgLog.notice("moved saves from save/ to \(self.webRoot, privacy: .public)/save/: \(moved.joined(separator: ", "), privacy: .public)")
        return .moved(moved)
    }

    private static let indexName = "global.rpgsave"

    /// The slot of `fileN.rpgsave`, or of `fileN.rpgsave.bak`, the backup the community core keeps while it saves. MV
    /// numbers slots from 1.
    private static func slot(_ name: String) -> Int? {
        var lower = name.lowercased()
        if lower.hasSuffix(".bak") { lower.removeLast(4) }
        guard lower.hasPrefix("file"), lower.hasSuffix(".rpgsave") else { return nil }
        let digits = lower.dropFirst(4).dropLast(8)
        guard !digits.isEmpty, digits.allSatisfy(\.isASCII), digits.allSatisfy(\.isNumber), let slot = Int(digits), slot > 0 else { return nil }
        return slot
    }

    /// `theirs`, an MV slot index, with the entries for `slots` taken from `ours`. The index is the lz-string base64 of a
    /// JSON array whose entry N describes slot N: its title, playtime, and when it was saved. Nil when either can't be
    /// read, or `ours` has no entry for one of `slots`.
    static func mergedIndex(_ ours: String, into theirs: String, slots: [Int]) -> String? {
        func entries(_ text: String) -> [Any]? {
            LZString.decompressFromBase64(text.trimmingCharacters(in: .whitespacesAndNewlines))
                .flatMap { try? JSONSerialization.jsonObject(with: Data($0.utf8)) as? [Any] }
        }
        guard let mine = entries(ours), var index = entries(theirs) else { return nil }
        for slot in slots {
            // MV lists a slot only when its entry is truthy. An object always is.
            guard slot > 0, slot < mine.count, mine[slot] is [String: Any] else { return nil }
            if index.count <= slot { index += Array(repeating: NSNull(), count: slot + 1 - index.count) }
            index[slot] = mine[slot]
        }
        guard let json = try? JSONSerialization.data(withJSONObject: index, options: .withoutEscapingSlashes) else { return nil }
        return LZString.compressToBase64(String(decoding: json, as: UTF8.self))
    }
}
