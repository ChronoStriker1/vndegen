import Foundation
import Testing
import VNCore
@testable import EngineKiriKiri

/// An in-memory listing of `files` plus the folders above them, as `FolderProbe(scanning:)` would list them.
private func probe(_ files: [String]) -> FolderProbe {
    var entries = Set<String>()
    for file in files {
        var parts = file.split(separator: "/").map(String.init)
        while !parts.isEmpty {
            entries.insert(parts.joined(separator: "/"))
            parts.removeLast()
        }
    }
    return FolderProbe(root: URL(fileURLWithPath: "/nonexistent"), entries: entries)
}

/// Writes `files` (relative path to contents) into a fresh folder.
private func folder(_ files: [String: Data]) throws -> URL {
    let root = FileManager.default.temporaryDirectory.appendingPathComponent("vndegen-kirikiri-\(UUID().uuidString)", isDirectory: true)
    for (path, contents) in files {
        let url = root.appendingPathComponent(path)
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try contents.write(to: url)
    }
    return root
}

private let xp3 = Data([0x58, 0x50, 0x33, 0x0D, 0x0A, 0x20, 0x0A, 0x1A, 0x8B, 0x67, 0x01, 0x17, 0, 0, 0])

@Test func detectsADataArchiveInTheRoot() {
    let listing = probe(["Test Game 08.exe", "data.xp3", "patch.xp3", "plugin/menu.dll"])
    #expect(KiriKiriLayout(listing) == KiriKiriLayout(root: "", data: .archive("data.xp3")))
    #expect(KiriKiriPlugin().detect(listing) == EngineMatch(engine: .kiriKiri, confidence: .certain, evidence: "data.xp3"))
}

@Test func detectsADataArchiveOneAndTwoFoldersDown() {
    #expect(KiriKiriLayout(probe(["Test Game 08/Game.eXe", "Test Game 08/Data.XP3"]))
        == KiriKiriLayout(root: "Test Game 08", data: .archive("Test Game 08/Data.XP3")))
    #expect(KiriKiriLayout(probe(["Test Game 08/Test Game 08 v1.1/game.exe", "Test Game 08/Test Game 08 v1.1/data.xp3",
                                  "Test Game 08/readme.txt"]))
        == KiriKiriLayout(root: "Test Game 08/Test Game 08 v1.1", data: .archive("Test Game 08/Test Game 08 v1.1/data.xp3")))
}

@Test func prefersTheShallowestRoot() {
    let listing = probe(["Test Game 08/game.exe", "Test Game 08/data.xp3", "Test Game 08/old/game.exe", "Test Game 08/old/data.xp3"])
    #expect(KiriKiriLayout(listing)?.root == "Test Game 08")
}

@Test func detectsAnUnpackedDataFolder() {
    let listing = probe(["Test Game 01/game.exe", "Test Game 01/data/startup.tjs", "Test Game 01/data/system/startup.tjs",
                         "Test Game 01/bgm.xp3"])
    #expect(KiriKiriLayout(listing) == KiriKiriLayout(root: "Test Game 01", data: .folder("Test Game 01/data")))
    #expect(KiriKiriPlugin().detect(listing)?.evidence == "Test Game 01/data/startup.tjs")
}

@Test func prefersTheDataArchiveOverTheDataFolder() {
    let listing = probe(["game.exe", "data.xp3", "data/startup.tjs"])
    #expect(KiriKiriLayout(listing)?.data == .archive("data.xp3"))
}

@Test func ignoresAppleDoubleFiles() {
    #expect(KiriKiriLayout(probe(["game.exe", "._data.xp3", "._game.exe"])) == nil)
    let listing = probe(["._data.xp3", "Test Game 08/._data.xp3", "Test Game 08/game.exe", "Test Game 08/data.xp3"])
    #expect(KiriKiriLayout(listing) == KiriKiriLayout(root: "Test Game 08", data: .archive("Test Game 08/data.xp3")))
}

@Test func doesNotMatchOtherEngines() {
    #expect(KiriKiriLayout(probe(["Test Game.exe", "readme.txt"])) == nil)
    #expect(KiriKiriLayout(probe(["Test Game.exe", "renpy/__init__.py", "game/script.rpyc", "lib/py3-windows-x86_64/python.exe"])) == nil)
    #expect(KiriKiriLayout(probe(["Game.exe", "www/js/rpg_core.js", "www/index.html", "package.json"])) == nil)
}

@Test func detectsAnArchiveEmbeddedInTheExecutable() throws {
    let root = try folder([
        "Test Game 73/Test Game 73.exe": Data(count: 4096),
        "Test Game 73/config.exe": Data(count: 512),
        "Test Game 73/scenario.xp3": xp3,
        "Test Game 73/._scenario.xp3": Data(count: 64),
    ])
    let listing = FolderProbe(scanning: root)
    #expect(KiriKiriLayout(listing) == KiriKiriLayout(root: "Test Game 73", data: .embedded("Test Game 73/Test Game 73.exe")))
    #expect(KiriKiriPlugin().detect(listing)?.evidence == "Test Game 73/Test Game 73.exe")
}

@Test func doesNotTakeAnyXP3NamedFileForAnEmbeddedArchive() throws {
    let root = try folder(["game.exe": Data(count: 64), "bgm.xp3": Data("not an archive".utf8)])
    #expect(KiriKiriLayout(FolderProbe(scanning: root)) == nil)
    #expect(KiriKiriLayout(probe(["game.exe", "bgm.xp3"])) == nil)
}

@Test func mapsSavesFromOtherMachines() {
    let rule = KiriKiriPlugin.saveRelativePath
    #expect(rule("/home/deck/Games/vn/Test Game 08/savedata/datasc.ksd") == "datasc.ksd")
    #expect(rule("C:/Games/Test Game 08/savedata/data0.kdt") == "data0.kdt")
    #expect(rule("C:/Games/Test Game 08/SaveData/data.cfu") == "data.cfu")
    #expect(rule("/home/deck/Games/vn/Test Game 08/savedata/slot/data0.bmp") == "slot/data0.bmp")
}

@Test func rejectsPathsThatNameNoSaveOrLeaveTheSaveFolder() {
    let rule = KiriKiriPlugin.saveRelativePath
    #expect(rule("C:/Games/Test Game 08/data.xp3") == nil)
    #expect(rule("C:/Games/Test Game 08/savedata") == nil)
    #expect(rule("C:/Games/Test Game 08/savedata/") == nil)
    #expect(rule("C:/Games/Test Game 08/savedata//datasc.ksd") == nil)
    #expect(rule("C:/Games/Test Game 08/savedata/./datasc.ksd") == nil)
    #expect(rule("C:/Games/Test Game 08/savedata/../data.xp3") == nil)
    #expect(rule("C:/Games/Test Game 08/savedata/slot/../../library.json") == nil)
}

@Test func keepsSavesInTheRootOfANestedGame() throws {
    let copied = try folder([
        "Test Game 08/game.exe": Data(),
        "Test Game 08/data.xp3": xp3,
        "Test Game 08/savedata/datasc.ksd": Data(),
    ])
    let data = URL(fileURLWithPath: "/nonexistent-data")
    let location = try #require(KiriKiriPlugin().saveLocations(folder: copied, dataDirectory: data).first)
    #expect(location.directory.standardizedFileURL == copied.appendingPathComponent("Test Game 08/savedata").standardizedFileURL)
    let local = location.directory.standardizedFileURL.path
    #expect(location.relativePath("\(local)/datasc.ksd") == "datasc.ksd")
    #expect(location.relativePath("\(local)/slot/data0.kdt") == "slot/data0.kdt")
    #expect(KiriKiriPlugin().saveLocations(folder: try folder(["readme.txt": Data()]), dataDirectory: data).isEmpty)
}

@MainActor @Test func makingASessionReportsTheMissingEngine() {
    let launch = GameLaunch(game: Game(id: GameID("Test Game 08")), folder: URL(fileURLWithPath: "/nonexistent"),
                            dataDirectory: URL(fileURLWithPath: "/nonexistent-data"), onExit: {})
    #expect(throws: KiriKiriError.self) { try KiriKiriPlugin().makeSession(launch) }
}
