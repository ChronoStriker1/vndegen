import Foundation
import TestSupport
import Testing
@testable import ArchiveImport

private var fm: FileManager { .default }

private func scratch() throws -> URL {
    let url = TestFolders.root.appendingPathComponent("enigmaimport-\(UUID().uuidString)", isDirectory: true)
    try fm.createDirectory(at: url, withIntermediateDirectories: true)
    return url
}

private func tree(_ folder: URL) -> [String] {
    (fm.enumerator(atPath: folder.path)?.allObjects as? [String] ?? []).sorted()
}

@MainActor
private func waitForImport(_ importer: ArchiveImporter) async throws {
    let deadline = ContinuousClock.now + .seconds(30)
    while importer.step == .extracting || importer.step == .opening {
        try #require(ContinuousClock.now < deadline, "import finished in time")
        try await Task.sleep(for: .milliseconds(10))
    }
}

private func bytes(_ text: String) -> [UInt8] { Array(text.utf8) }

private func game(web root: String = "www") -> [SyntheticEnigmaPack.Node] {
    [
        SyntheticEnigmaPack.defaultFolder([
            .file("credits.html", [UInt8](repeating: 1, count: 600_000)),
            .file("Game.exe", bytes("MZ runtime")),
            .file("nw.pak", bytes("pak")),
            .file("icudtl.dat", bytes("icu")),
            .folder("locales", [.file("en-US.pak", bytes("en")), .file("ja.pak", bytes("ja"))]),
            .file("package.json", bytes("{\"main\":\"www/index.html\"}")),
            .folder(root, [
                .file("index.html", bytes("<html>")),
                .folder("js", [.file("rpg_core.js", bytes("core"))]),
                .folder("data", [.file("System.json", bytes("\u{FEFF}{\"gameTitle\":\"Pack Quest: Two\"}"))]),
                .folder("save", []),
            ]),
        ]),
        .folder("%SYSTEM FOLDER%", [.file("helper.dll", bytes("dll"))]),
    ]
}

private func put(_ data: Data, as name: String, in folder: URL) throws -> URL {
    let url = folder.appendingPathComponent(name)
    try data.write(to: url)
    return url
}

private func run(_ tool: String, _ arguments: [String]) throws {
    let process = Process()
    process.executableURL = URL(fileURLWithPath: tool)
    process.arguments = arguments
    process.standardOutput = FileHandle.nullDevice
    try process.run()
    process.waitUntilExit()
    try #require(process.terminationStatus == 0, "\(tool) \(arguments)")
}

@Test(arguments: [false, true])
func readsBothPackLayouts(legacy: Bool) throws {
    let exe = try put(SyntheticEnigmaPack.make(game(), legacy: legacy), as: "Game_full.exe", in: try scratch())
    #expect(EnigmaPack.isPack(exe))
    let pack = try EnigmaPack(exe)
    #expect(pack.title == "Pack Quest: Two")
    #expect(pack.files.map(\.path) == [
        "credits.html", "Game.exe", "nw.pak", "icudtl.dat", "locales/en-US.pak", "locales/ja.pak", "package.json",
        "www/index.html", "www/js/rpg_core.js", "www/data/System.json",
    ])
    #expect(pack.directories == ["locales", "www", "www/js", "www/data", "www/save"])
    let handle = try FileHandle(forReadingFrom: exe)
    defer { try? handle.close() }
    var core = Data()
    try pack.copy(pack.files[8], from: handle) { core.append(contentsOf: $0) }
    #expect(String(decoding: core, as: UTF8.self) == "core")
}

@Test func refusesFilesThatAreNotPacks() throws {
    let folder = try scratch()
    #expect(!EnigmaPack.isPack(try put(Data(repeating: 0x4d, count: 4096), as: "Game.exe", in: folder)))
    #expect(!EnigmaPack.isPack(try put(Data("MZ".utf8), as: "Tiny.exe", in: folder)))
    #expect(throws: EnigmaPack.Failure.unsupported("the file table doesn't add up to the pack's size")) {
        try EnigmaPack(try put(SyntheticEnigmaPack.make(game(), totalAdjustment: 8), as: "Odd.exe", in: folder))
    }
}

@MainActor @Test func importsAPackedGameWithoutTheNWjsRuntime() async throws {
    let folder = try scratch()
    let games = folder.appendingPathComponent("Games")
    try fm.createDirectory(at: games, withIntermediateDirectories: true)
    let exe = try put(SyntheticEnigmaPack.make(game()), as: "Game_full.exe", in: folder)
    let importer = ArchiveImporter(gamesRoot: games)
    await importer.choose([exe])
    try #require(importer.step == .reviewing, "\(importer.errorMessage ?? "")")
    #expect(importer.contents?.isPackedGame == true)
    #expect(importer.pickedPackedGame)
    #expect(importer.folderName == "Pack Quest - Two")
    #expect(importer.contents?.fileCount == 4)
    importer.startImport()
    try await waitForImport(importer)
    guard case let .finished(result) = importer.step else { Issue.record("\(importer.errorMessage ?? "")"); return }
    #expect(result.folderName == "Pack Quest - Two")
    #expect(tree(games.appendingPathComponent("Pack Quest - Two")) == [
        "package.json", "www", "www/data", "www/data/System.json", "www/index.html", "www/js", "www/js/rpg_core.js", "www/save",
    ])
    #expect(fm.fileExists(atPath: exe.path))
}

@MainActor @Test func refusesACompressedPack() async throws {
    var nodes = game()
    nodes[0] = SyntheticEnigmaPack.defaultFolder([
        .file("package.json", bytes("{}")),
        .folder("www", [.compressed("index.html", size: 600, stored: bytes("packed"))]),
    ])
    let exe = try put(SyntheticEnigmaPack.make(nodes), as: "Game.exe", in: try scratch())
    let importer = ArchiveImporter(gamesRoot: try scratch())
    await importer.choose([exe])
    #expect(importer.step == .choosing)
    #expect(importer.errorMessage == ArchiveError.packedGame(.compressed).message)
}

@MainActor @Test func unpacksAPackedGameInsideAnArchiveBesideItsSaves() async throws {
    let folder = try scratch()
    let content = folder.appendingPathComponent("Pack Quest")
    try fm.createDirectory(at: content.appendingPathComponent("www/save"), withIntermediateDirectories: true)
    _ = try put(SyntheticEnigmaPack.make(game(web: "WWW"), legacy: true), as: "Game_boxed.exe", in: content)
    try Data("read me".utf8).write(to: content.appendingPathComponent("readme.txt"))
    try Data("config".utf8).write(to: content.appendingPathComponent("www/save/config.rpgsave"))
    let zip = folder.appendingPathComponent("Pack Quest.zip")
    try run("/usr/bin/ditto", ["-c", "-k", "--norsrc", "--keepParent", content.path, zip.path])
    let games = folder.appendingPathComponent("Games")
    try fm.createDirectory(at: games, withIntermediateDirectories: true)
    let importer = ArchiveImporter(gamesRoot: games)
    await importer.choose([zip])
    importer.startImport()
    try await waitForImport(importer)
    #expect(importer.step == .nested(fileName: "Game_boxed.exe"))
    #expect(importer.nestedSource?.isPackedGame == true)
    importer.extractNested()
    try await waitForImport(importer)
    guard case let .finished(result) = importer.step else { Issue.record("\(importer.errorMessage ?? "")"); return }
    #expect(result.folderName == "Pack Quest")
    #expect(result.files == 4)
    let unpacked = games.appendingPathComponent("Pack Quest")
    #expect(tree(unpacked) == [
        "package.json", "readme.txt", "www", "www/data", "www/data/System.json", "www/index.html", "www/js",
        "www/js/rpg_core.js", "www/save", "www/save/config.rpgsave",
    ])
    #expect(try String(contentsOf: unpacked.appendingPathComponent("www/save/config.rpgsave"), encoding: .utf8) == "config")
    #expect(tree(games.appendingPathComponent(".importing")).isEmpty)
}

@MainActor @Test func unpacksAPackedGameTwoFoldersDownInAnArchive() async throws {
    let folder = try scratch()
    let outer = folder.appendingPathComponent("GAME-RJ000001")
    let content = outer.appendingPathComponent("Pack Quest ver1.00")
    try fm.createDirectory(at: content, withIntermediateDirectories: true)
    _ = try put(SyntheticEnigmaPack.make(game()), as: "packquest.exe", in: content)
    try Data("read me".utf8).write(to: content.appendingPathComponent("readme.txt"))
    let zip = folder.appendingPathComponent("Pack Quest.zip")
    try run("/usr/bin/ditto", ["-c", "-k", "--norsrc", "--keepParent", outer.path, zip.path])
    let games = folder.appendingPathComponent("Games")
    try fm.createDirectory(at: games, withIntermediateDirectories: true)
    let importer = ArchiveImporter(gamesRoot: games)
    await importer.choose([zip])
    importer.startImport()
    try await waitForImport(importer)
    #expect(importer.step == .nested(fileName: "packquest.exe"))
    #expect(importer.nestedSource?.isPackedGame == true)
    importer.extractNested()
    try await waitForImport(importer)
    guard case let .finished(result) = importer.step else { Issue.record("\(importer.errorMessage ?? "")"); return }
    #expect(result.folderName == "Pack Quest")
    #expect(tree(games.appendingPathComponent("Pack Quest")) == [
        "package.json", "readme.txt", "www", "www/data", "www/data/System.json", "www/index.html", "www/js",
        "www/js/rpg_core.js", "www/save",
    ])
    #expect(tree(games.appendingPathComponent(".importing")).isEmpty)
}

@Test func listsAPackedGameReadyToImport() throws {
    let folder = try scratch()
    _ = try put(SyntheticEnigmaPack.make(game()), as: "Game_full.exe", in: folder)
    #expect(LocalArchives.scan([folder]).map(\.fileName) == ["Game_full.exe"])
}
