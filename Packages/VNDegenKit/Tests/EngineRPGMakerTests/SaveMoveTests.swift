import Foundation
import Testing
import WebRuntime
@testable import EngineRPGMaker

/// An MV game deployed in www/ with `files`, path to contents. Its core is the community one unless `core` says otherwise.
private func mvGame(_ files: [String: String], core: String = "// rpg_managers.js v1.6.1 (community-1.3b)\nStorageManager.canMakeWwwSaveDirectory = function() {};") throws -> URL {
    let fm = FileManager.default
    let root = fm.temporaryDirectory.appendingPathComponent("vndegen-savemove-\(UUID().uuidString)", isDirectory: true)
    var all = files
    all["www/js/rpg_core.js"] = ""
    all["www/js/rpg_managers.js"] = core
    for (path, text) in all {
        let url = root.appendingPathComponent(path)
        try fm.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data(text.utf8).write(to: url)
    }
    return root
}

private func contents(_ folder: URL) -> [String: String] {
    let names = (try? FileManager.default.contentsOfDirectory(atPath: folder.path)) ?? []
    return Dictionary(uniqueKeysWithValues: names.map { ($0, (try? String(contentsOf: folder.appendingPathComponent($0), encoding: .utf8)) ?? "") })
}

/// A slot index as MV writes it, with an entry titled with the slot number for each of `slots`.
private func index(_ slots: [Int]) -> String {
    let entries = (0...(slots.max() ?? 0)).map { slots.contains($0) ? #"{"globalId":"RPGMV","title":"slot \#($0)","playtime":"00:01:02"}"# : "null" }
    return LZString.compressToBase64("[\(entries.joined(separator: ","))]")
}

/// The slot titles an index lists, by slot.
private func titles(_ index: String?) throws -> [Int: String] {
    let json = try #require(index.flatMap(LZString.decompressFromBase64))
    let entries = try #require(try JSONSerialization.jsonObject(with: Data(json.utf8)) as? [Any])
    return entries.enumerated().reduce(into: [:]) { if let entry = $1.element as? [String: Any] { $0[$1.offset] = entry["title"] as? String } }
}

@Test func lzStringWritesWhatMVsOwnLibraryWrites() throws {
    // From lz-string 1.3's compressToBase64 in Rookie Knight Rathi's js/libs/lz-string.js.
    let vectors = [
        "hello hello hello": "BYUwNmD2AEoTcpAA",
        #"[null,{"globalId":"RPGMV","title":"ルーキーナイト","playtime":"00:01:02"}]"#:
            "NoOwrgNhA0DeBEBzCB7ARgQwgSQCbwC54AlABQHEBZANXmngBcBLBiAU0PkGuGQH4ZBahh6AphkAlDIAmGOvAAOEDAE9mAWw5EADCoIqAjBoBM8AL4BdIAAA",
    ]
    for (text, compressed) in vectors {
        #expect(LZString.compressToBase64(text) == compressed)
        #expect(LZString.decompressFromBase64(compressed) == text)
    }
    let long = String(repeating: "セーブ save 0123456789 ", count: 500)
    #expect(LZString.decompressFromBase64(LZString.compressToBase64(long)) == long)
    #expect(LZString.decompressFromBase64("") == nil)
    #expect(LZString.decompressFromBase64("not lz-string!") == nil)
}

@Test func communityCoreSavesMoveBesideIndexHtml() throws {
    // Rookie Knight Rathi ships a config, slot 6, and an index listing slot 6 in www/save/. The player's slots join them.
    let root = try mvGame([
        "save/file1.rpgsave": "mine", "save/file1.rpgsave.bak": "backup", "save/file2.rpgsave": "mine too",
        "save/global.rpgsave": index([1, 2]), "save/config.rpgsave": "my config",
        "www/save/config.rpgsave": "shipped config", "www/save/file6.rpgsave": "shipped save", "www/save/global.rpgsave": index([6]),
    ])
    defer { try? FileManager.default.removeItem(at: root) }
    try FileManager.default.createDirectory(at: root.appendingPathComponent("www/testDirectory"), withIntermediateDirectories: true)
    let folder = GameFolder(rpgMakerGameAt: root)

    #expect(folder.moveCommunityCoreSaves() == .moved(["file1.rpgsave", "file1.rpgsave.bak", "file2.rpgsave"]))
    var saves = contents(root.appendingPathComponent("www/save"))
    #expect(try titles(saves.removeValue(forKey: "global.rpgsave")) == [1: "slot 1", 2: "slot 2", 6: "slot 6"])
    #expect(saves == [
        "file1.rpgsave": "mine", "file1.rpgsave.bak": "backup", "file2.rpgsave": "mine too",
        "config.rpgsave": "shipped config", "file6.rpgsave": "shipped save",
    ])
    // What www/save/ already had stays, and save/ keeps the config that would have replaced the game's.
    #expect(contents(root.appendingPathComponent("save")) == ["config.rpgsave": "my config"])
    #expect(!FileManager.default.fileExists(atPath: root.appendingPathComponent("www/testDirectory").path))
    #expect(folder.moveCommunityCoreSaves() == .nothing)
}

@Test func savesMoveIntoAMissingSaveFolder() throws {
    let root = try mvGame(["save/file2.rpgsave": "two", "save/global.rpgsave": index([2])])
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(GameFolder(rpgMakerGameAt: root).moveCommunityCoreSaves() == .moved(["file2.rpgsave"]))
    var saves = contents(root.appendingPathComponent("www/save"))
    #expect(try titles(saves.removeValue(forKey: "global.rpgsave")) == [2: "slot 2"])
    #expect(saves == ["file2.rpgsave": "two"])
    #expect(!FileManager.default.fileExists(atPath: root.appendingPathComponent("save").path))
}

@Test func aMoveThatStoppedPartwayFinishesNextTime() throws {
    // The index was written and slot 1 moved, then the app stopped before slot 2.
    let root = try mvGame([
        "save/file2.rpgsave": "two", "save/global.rpgsave": index([1, 2]),
        "www/save/file1.rpgsave": "one", "www/save/global.rpgsave": index([1, 2]),
    ])
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(GameFolder(rpgMakerGameAt: root).moveCommunityCoreSaves() == .moved(["file2.rpgsave"]))
    var saves = contents(root.appendingPathComponent("www/save"))
    #expect(try titles(saves.removeValue(forKey: "global.rpgsave")) == [1: "slot 1", 2: "slot 2"])
    #expect(saves == ["file1.rpgsave": "one", "file2.rpgsave": "two"])
    #expect(!FileManager.default.fileExists(atPath: root.appendingPathComponent("save").path))
}

@Test func savesThatCantMergeStayWhereTheyAre() throws {
    let ours = ["save/file1.rpgsave": "mine", "save/global.rpgsave": index([1])]
    for (mine, theirs) in [
        // The same slot in both folders, in any case, or its backup.
        (ours, ["www/save/FILE1.rpgsave": "pc save", "www/save/global.rpgsave": index([1])]),
        (ours, ["www/save/file1.rpgsave.bak": "pc backup"]),
        // An index that can't be read.
        (ours, ["www/save/global.rpgsave": "not an index"]),
        // An index of ours that doesn't describe the slot, so the slot would show as empty there.
        (["save/file1.rpgsave": "mine", "save/global.rpgsave": index([3])], ["www/save/global.rpgsave": index([6])]),
        // No index of ours at all.
        (["save/file1.rpgsave": "mine"], ["www/save/global.rpgsave": index([6])]),
    ] {
        let root = try mvGame(mine.merging(theirs) { a, _ in a })
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(GameFolder(rpgMakerGameAt: root).moveCommunityCoreSaves() == .keptBoth(mine.keys.map { ($0 as NSString).lastPathComponent }.sorted()))
        #expect(contents(root.appendingPathComponent("save")) == mine.reduce(into: [:]) { $0[($1.key as NSString).lastPathComponent] = $1.value })
        #expect(contents(root.appendingPathComponent("www/save")) == theirs.reduce(into: [:]) { $0[($1.key as NSString).lastPathComponent] = $1.value })
    }
}

@Test func anIndexOutsideTheGameIsNeverRead() throws {
    let fm = FileManager.default
    let outside = fm.temporaryDirectory.appendingPathComponent("vndegen-outside-\(UUID().uuidString).rpgsave")
    try Data(index([6]).utf8).write(to: outside)
    let root = try mvGame(["save/file1.rpgsave": "mine", "save/global.rpgsave": index([1])])
    defer { try? fm.removeItem(at: root); try? fm.removeItem(at: outside) }
    try fm.createDirectory(at: root.appendingPathComponent("www/save"), withIntermediateDirectories: true)
    try fm.createSymbolicLink(at: root.appendingPathComponent("www/save/global.rpgsave"), withDestinationURL: outside)
    #expect(GameFolder(rpgMakerGameAt: root).moveCommunityCoreSaves() == .keptBoth(["file1.rpgsave", "global.rpgsave"]))
    #expect(try String(contentsOf: outside, encoding: .utf8) == index([6]))
}

@Test func namesThatArentSlotsAreNotSlots() throws {
    // MV numbers slots from 1. file-1 and file0 are other files, which move with the saves but need no index entry.
    let root = try mvGame(["save/file-1.rpgsave": "a", "save/file0.rpgsave.bak": "b", "save/global.rpgsave": index([1])])
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(GameFolder(rpgMakerGameAt: root).moveCommunityCoreSaves() == .nothing)
    try Data("one".utf8).write(to: root.appendingPathComponent("save/file1.rpgsave"))
    #expect(GameFolder(rpgMakerGameAt: root).moveCommunityCoreSaves() == .moved(["file-1.rpgsave", "file0.rpgsave.bak", "file1.rpgsave"]))
}

/// Exercises the case only on a case-sensitive volume, such as `VNDEGEN_TEST_DIR` from `scripts/build.sh test`.
@Test func anIndexInTwoSpellingsIsLeftAlone() throws {
    let fm = FileManager.default
    let scratch = ProcessInfo.processInfo.environment["VNDEGEN_TEST_DIR"].map { URL(fileURLWithPath: $0, isDirectory: true) } ?? fm.temporaryDirectory
    let root = scratch.appendingPathComponent("vndegen-spellings-\(UUID().uuidString)", isDirectory: true)
    defer { try? fm.removeItem(at: root) }
    for (path, text) in ["www/js/rpg_core.js": "", "www/js/rpg_managers.js": "StorageManager.canMakeWwwSaveDirectory",
                         "save/file1.rpgsave": "mine", "save/global.rpgsave": index([1]),
                         "www/save/global.rpgsave": index([6]), "www/save/GLOBAL.rpgsave": index([7])] {
        let url = root.appendingPathComponent(path)
        try fm.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data(text.utf8).write(to: url)
    }
    guard try fm.contentsOfDirectory(atPath: root.appendingPathComponent("www/save").path).count == 2 else { return }
    #expect(GameFolder(rpgMakerGameAt: root).moveCommunityCoreSaves() == .keptBoth(["file1.rpgsave", "global.rpgsave"]))
}

@Test func otherCoresKeepTheirSaveFolders() throws {
    // The standard core always saved beside index.html, so a top-level save/ isn't VNDegen's to move.
    let root = try mvGame(["save/file1.rpgsave": "pc"], core: "// rpg_managers.js v1.6.2")
    defer { try? FileManager.default.removeItem(at: root) }
    try FileManager.default.createDirectory(at: root.appendingPathComponent("www/testDirectory"), withIntermediateDirectories: true)
    #expect(GameFolder(rpgMakerGameAt: root).moveCommunityCoreSaves() == .nothing)
    #expect(contents(root.appendingPathComponent("save")) == ["file1.rpgsave": "pc"])
    #expect(FileManager.default.fileExists(atPath: root.appendingPathComponent("www/testDirectory").path))
}

@Test func savesStayWhileTheCoreWouldStillPickSave() throws {
    // A testDirectory that isn't an empty folder fails the core's check, so it keeps using save/.
    for blocker in ["www/testDirectory/note.txt", "www/testDirectory"] {
        let root = try mvGame([blocker: "kept", "save/file1.rpgsave": "mine", "save/global.rpgsave": index([1])])
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(GameFolder(rpgMakerGameAt: root).moveCommunityCoreSaves() == .nothing)
        #expect(contents(root.appendingPathComponent("save")) == ["file1.rpgsave": "mine", "global.rpgsave": index([1])])
        #expect(try String(contentsOf: root.appendingPathComponent(blocker), encoding: .utf8) == "kept")
    }
}
