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

    #expect(folder.moveCommunityCoreSaves() == .moved(["file1.rpgsave", "file1.rpgsave.bak", "file2.rpgsave"], mergedIndex: true))
    var saves = contents(root.appendingPathComponent("www/save"))
    #expect(try titles(saves.removeValue(forKey: "global.rpgsave")) == [1: "slot 1", 2: "slot 2", 6: "slot 6"])
    #expect(saves == [
        "file1.rpgsave": "mine", "file1.rpgsave.bak": "backup", "file2.rpgsave": "mine too",
        "config.rpgsave": "shipped config", "file6.rpgsave": "shipped save",
    ])
    // What www/save/ already had stays, and save/ keeps the files that would have replaced it.
    #expect(contents(root.appendingPathComponent("save")) == ["config.rpgsave": "my config", "global.rpgsave": index([1, 2])])
    #expect(!FileManager.default.fileExists(atPath: root.appendingPathComponent("www/testDirectory").path))
    #expect(folder.moveCommunityCoreSaves() == .nothing)
}

@Test func savesMoveIntoAMissingSaveFolder() throws {
    let root = try mvGame(["save/file2.rpgsave": "two", "save/global.rpgsave": "index"])
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(GameFolder(rpgMakerGameAt: root).moveCommunityCoreSaves() == .moved(["file2.rpgsave", "global.rpgsave"], mergedIndex: false))
    #expect(contents(root.appendingPathComponent("www/save")) == ["file2.rpgsave": "two", "global.rpgsave": "index"])
    #expect(!FileManager.default.fileExists(atPath: root.appendingPathComponent("save").path))
}

@Test func savesThatCantMergeStayWhereTheyAre() throws {
    let ours = ["save/file1.rpgsave": "mine", "save/global.rpgsave": index([1])]
    for theirs in [
        // The same slot in both folders, in any case, or its backup.
        ["www/save/FILE1.rpgsave": "pc save", "www/save/global.rpgsave": index([1])],
        ["www/save/file1.rpgsave.bak": "pc backup"],
        // An index that can't be read, or one whose slots ours can't be added to.
        ["www/save/global.rpgsave": "not an index"],
    ] {
        let root = try mvGame(ours.merging(theirs) { a, _ in a })
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(GameFolder(rpgMakerGameAt: root).moveCommunityCoreSaves() == .keptBoth(["file1.rpgsave", "global.rpgsave"]))
        #expect(contents(root.appendingPathComponent("save")) == ["file1.rpgsave": "mine", "global.rpgsave": index([1])])
        #expect(contents(root.appendingPathComponent("www/save")) == theirs.reduce(into: [:]) { $0[($1.key as NSString).lastPathComponent] = $1.value })
    }
    // Without an index of our own, a moved slot would show as empty in the game's index.
    let root = try mvGame(["save/file1.rpgsave": "mine", "www/save/global.rpgsave": index([6])])
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(GameFolder(rpgMakerGameAt: root).moveCommunityCoreSaves() == .keptBoth(["file1.rpgsave"]))
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

@Test func aTestDirectoryWithFilesInItStays() throws {
    let root = try mvGame(["www/testDirectory/note.txt": "kept", "save/config.rpgsave": "config only"])
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(GameFolder(rpgMakerGameAt: root).moveCommunityCoreSaves() == .nothing)
    #expect(contents(root.appendingPathComponent("www/testDirectory")) == ["note.txt": "kept"])
    #expect(contents(root.appendingPathComponent("save")) == ["config.rpgsave": "config only"])
}
