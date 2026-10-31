import Foundation
import Testing
import VNCore
import WebRuntime
@testable import EngineHTMLVN

func temporaryFolder(_ files: [String: Data]) throws -> URL {
    let root = FileManager.default.temporaryDirectory.appendingPathComponent("vndegen-htmlvn-\(UUID().uuidString)", isDirectory: true)
    try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
    for (path, data) in files {
        let url = root.appendingPathComponent(path)
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try data.write(to: url)
    }
    return root
}

/// The files every Visual Novel Maker export has in its web root, under `prefix`.
private func engineFiles(_ prefix: String) -> [String: Data] {
    ["index.html", "data/ENGINE.js", "data/SCRIPTS.js", "data/lib/boot.js"].reduce(into: [:]) { files, name in
        files[prefix + name] = Data("// \(name)".utf8)
    }
}

private func detect(_ root: URL) -> EngineMatch? { VisualNovelMakerPlugin().detect(FolderProbe(scanning: root)) }

@Test func detectsAnNWjsBuildBesideGameExe() throws {
    let root = try temporaryFolder(engineFiles("").merging(["package.json": Data(#"{"name":"Love Ritual"}"#.utf8), "Game.exe": Data()]) { a, _ in a })
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(detect(root) == EngineMatch(engine: .visualNovelMaker, confidence: .certain, evidence: "data/ENGINE.js"))
    let game = try #require(VisualNovelMakerGame(folder: root))
    #expect(game == VisualNovelMakerGame(root: "", webRoot: "", packaging: .nwjs))
    #expect(VisualNovelMakerPlugin.runtime(for: game, folder: GameFolder(root: root, webRoot: "")) == .nwjs(execPath: "/game/Game.exe"))
}

@Test func detectsAnElectronBuildInResourcesApp() throws {
    let root = try temporaryFolder(engineFiles("resources/app/").merging(["resources/app/main.js": Data(), "Game.exe": Data()]) { a, _ in a })
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(detect(root)?.evidence == "resources/app/data/ENGINE.js")
    let game = try #require(VisualNovelMakerGame(folder: root))
    #expect(game.packaging == .electron && game.root.isEmpty)
    let folder = GameFolder(root: root, webRoot: game.webRoot)
    #expect(VisualNovelMakerPlugin.runtime(for: game, folder: folder) == .electron(appPath: "/game/resources/app"))
}

/// An archive import wraps the game in a folder named after the archive, and some archives wrap it once more.
@Test func detectsAGameOneOrTwoFoldersDown() throws {
    let root = try temporaryFolder(engineFiles("Siren's Call 1.131/Siren's Call/resources/app/")
        .merging(engineFiles("Siren's Call 1.131/Siren's Call/Extras/old/")) { a, _ in a })
    defer { try? FileManager.default.removeItem(at: root) }
    let game = try #require(VisualNovelMakerGame(folder: root))
    #expect(game == VisualNovelMakerGame(root: "Siren's Call 1.131/Siren's Call", webRoot: "resources/app", packaging: .electron))
    #expect(detect(root)?.evidence == "Siren's Call 1.131/Siren's Call/resources/app/data/ENGINE.js")

    let nwjs = try temporaryFolder(engineFiles("Love Ritual/").merging(["Love Ritual/package.json": Data("{}".utf8)]) { a, _ in a })
    defer { try? FileManager.default.removeItem(at: nwjs) }
    #expect(VisualNovelMakerGame(folder: nwjs) == VisualNovelMakerGame(root: "Love Ritual", webRoot: "", packaging: .nwjs))
}

@Test func otherHTMLGamesAreNotVisualNovelMaker() throws {
    var files = engineFiles("")
    files["data/SCRIPTS.js"] = nil
    let root = try temporaryFolder(files.merging(["js/rpg_core.js": Data()]) { a, _ in a })
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(detect(root) == nil)
}

@Test func savesAreVndataFilesInTheGamesOwnFolder() throws {
    let root = try temporaryFolder(engineFiles("Siren's Call/resources/app/"))
    defer { try? FileManager.default.removeItem(at: root) }
    let data = root.appendingPathComponent("Data", isDirectory: true)
    let locations = VisualNovelMakerPlugin().saveLocations(folder: root, dataDirectory: data)
    #expect(locations.count == 1)
    let location = try #require(locations.first)
    #expect(location.directory.standardizedFileURL == root.appendingPathComponent("Siren's Call", isDirectory: true).standardizedFileURL)
    #expect(location.relativePath("C:/Games/Siren's Call/SaveGame_1.vndata") == "SaveGame_1.vndata")
    #expect(location.relativePath("C:/Users/player/AppData/Roaming/Siren's Call/settings.vndata") == "settings.vndata")
    #expect(location.relativePath("C:/Games/Siren's Call/SaveGame_1_Header.VNDATA") == "SaveGame_1_Header.VNDATA")
    #expect(location.relativePath("C:/Games/Siren's Call/Game.exe") == nil)
}

@Test func aWebExportSavesToLocalStorage() throws {
    let root = try temporaryFolder(engineFiles(""))
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(VisualNovelMakerGame(folder: root)?.packaging == .browser)
    let data = root.appendingPathComponent("Data", isDirectory: true)
    let location = try #require(VisualNovelMakerPlugin().saveLocations(folder: root, dataDirectory: data).first)
    #expect(location.directory == data.appendingPathComponent("localStorage", isDirectory: true))
    #expect(location.relativePath("/VNDegen/game/localStorage/abc_SaveGame_1.value") == "abc_SaveGame_1.value")
}

@Test func confirmPressesEnter() {
    #expect(VisualNovelMakerPlugin.buttonScript(.confirm, true) == "__vndegen.key('keydown', 'Enter', 'Enter', 13)")
    #expect(VisualNovelMakerPlugin.buttonScript(.cancel, true) == nil)
    #expect(VisualNovelMakerPlugin().defaultControls == .keyboardAndMouse)
}
