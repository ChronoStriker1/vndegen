import Foundation
import Testing
import VNCore
import WebRuntime
@testable import EngineRPGMaker

private func temporaryGame() throws -> URL {
    let root = FileManager.default.temporaryDirectory.appendingPathComponent("vndegen-\(UUID().uuidString)", isDirectory: true)
    try FileManager.default.createDirectory(at: root.appendingPathComponent("js"), withIntermediateDirectories: true)
    try Data("<html></html>".utf8).write(to: root.appendingPathComponent("index.html"))
    return root
}

@Test func webRootFollowsTheEngineCore() throws {
    let root = try temporaryGame()
    defer { try? FileManager.default.removeItem(at: root) }
    try Data().write(to: root.appendingPathComponent("js/rmmz_core.js"))
    #expect(GameFolder(rpgMakerGameAt: root).webRoot == "")

    let mv = try temporaryGame()
    defer { try? FileManager.default.removeItem(at: mv) }
    try FileManager.default.createDirectory(at: mv.appendingPathComponent("WWW/JS"), withIntermediateDirectories: true)
    try Data("mv".utf8).write(to: mv.appendingPathComponent("WWW/index.html"))
    try Data().write(to: mv.appendingPathComponent("WWW/JS/rpg_core.js"))
    let folder = GameFolder(rpgMakerGameAt: mv)
    #expect(folder.webRoot == "www")
    #expect(try folder.resolve(servedPath: "/").map { try Data(contentsOf: $0) } == Data("mv".utf8))
}

/// A library's RPG Maker game folders as imported, each with a wrapper left in: a readme beside the game's folder in the
/// archive, or two folders above the game.
@Test(arguments: [
    ("Game/www/js/rpg_core.js", "Game", "www"),
    ("DQGame/js/rmmz_core.js", "DQGame", ""),
    ("Outer/Inner/js/rmmz_core.js", "Outer/Inner", ""),
    ("Outer/Inner/www/js/rpg_core.js", "Outer/Inner", "www"),
    ("A/B/Game/www/js/rpg_core.js", "A/B/Game", "www"),
    ("A/B/C/js/rmmz_core.js", "A/B/C", ""),
    ("OUTER/WWW/JS/RPG_CORE.JS", "OUTER", "www"),
])
func findsAGameInsideWrapperFolders(core: String, root: String, webRoot: String) throws {
    let folder = FileManager.default.temporaryDirectory.appendingPathComponent("vndegen-\(UUID().uuidString)", isDirectory: true)
    defer { try? FileManager.default.removeItem(at: folder) }
    let url = folder.appendingPathComponent(core)
    try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
    try Data().write(to: url)
    try Data("readme".utf8).write(to: folder.appendingPathComponent("readme.txt"))

    let probe = FolderProbe(scanning: folder)
    let plugin = core.lowercased().hasSuffix("rmmz_core.js") ? RPGMakerPlugin(.mz) : RPGMakerPlugin(.mv)
    // In any case, because a disk that ignores case gives the name as it was asked for.
    #expect(plugin.detect(probe)?.evidence.lowercased() == core.lowercased())
    #expect(plugin.detect(probe)?.confidence == .likely)
    let game = GameFolder(rpgMakerGameAt: folder)
    #expect(game.root.standardizedFileURL.path.hasSuffix("/" + root))
    #expect(game.webRoot == webRoot)
}

@Test func theShallowestGameWinsAndALongListingIsNotSearched() throws {
    let folder = FileManager.default.temporaryDirectory.appendingPathComponent("vndegen-\(UUID().uuidString)", isDirectory: true)
    defer { try? FileManager.default.removeItem(at: folder) }
    for core in ["Tool/Old/Copy/js/rpg_core.js", "Game/www/js/rpg_core.js"] {
        let url = folder.appendingPathComponent(core)
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data().write(to: url)
    }
    #expect(RPGMakerPlugin(.mv).detect(FolderProbe(scanning: folder))?.evidence == "Game/www/js/rpg_core.js")

    // Past the listing, in a folder not named js or www, under a listing too long to be a wrapper.
    let long = FileManager.default.temporaryDirectory.appendingPathComponent("vndegen-\(UUID().uuidString)", isDirectory: true)
    defer { try? FileManager.default.removeItem(at: long) }
    let core = long.appendingPathComponent("A/B/Game/js/rpg_core.js")
    try FileManager.default.createDirectory(at: core.deletingLastPathComponent(), withIntermediateDirectories: true)
    try Data().write(to: core)
    for index in 0..<201 { try Data().write(to: long.appendingPathComponent("A/\(index).txt")) }
    #expect(RPGMakerPlugin(.mv).detect(FolderProbe(scanning: long)) == nil)
}

@Test func aGameAtTheTopWinsOverAnEngineCopyBelowIt() throws {
    let folder = FileManager.default.temporaryDirectory.appendingPathComponent("vndegen-\(UUID().uuidString)", isDirectory: true)
    defer { try? FileManager.default.removeItem(at: folder) }
    for core in ["www/js/rpg_core.js", "Backup/js/rmmz_core.js"] {
        let url = folder.appendingPathComponent(core)
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data().write(to: url)
    }
    let match = EngineDetector.detect(FolderProbe(scanning: folder), using: [RPGMakerPlugin(.mz), RPGMakerPlugin(.mv)])
    #expect(match?.engine == .rpgMakerMV)
    #expect(match?.confidence == .certain)
    #expect(GameFolder(rpgMakerGameAt: folder).webRoot == "www")
}

/// The listing follows a link that `GameFolder` won't, so they disagree about the core. That must not loop.
@Test func aCoreBehindALinkOutOfTheFolderIsNotAGame() throws {
    let base = FileManager.default.temporaryDirectory.appendingPathComponent("vndegen-\(UUID().uuidString)", isDirectory: true)
    defer { try? FileManager.default.removeItem(at: base) }
    let outside = base.appendingPathComponent("outside/js", isDirectory: true), game = base.appendingPathComponent("game", isDirectory: true)
    try FileManager.default.createDirectory(at: outside, withIntermediateDirectories: true)
    try FileManager.default.createDirectory(at: game, withIntermediateDirectories: true)
    try Data().write(to: outside.appendingPathComponent("rpg_core.js"))
    try FileManager.default.createSymbolicLink(at: game.appendingPathComponent("js"), withDestinationURL: outside)
    #expect(GameFolder(rpgMakerGameAt: game).webRoot == "")
}

@Test func savesGoBesideIndexHtmlWhateverTheirCase() throws {
    let root = try temporaryGame()
    defer { try? FileManager.default.removeItem(at: root) }
    try FileManager.default.createDirectory(at: root.appendingPathComponent("Save"), withIntermediateDirectories: true)
    try Data().write(to: root.appendingPathComponent("js/rmmz_core.js"))
    try Data().write(to: root.appendingPathComponent("Save/file1.rmmzsave"))
    let saves = try #require(GameFolder(rpgMakerGameAt: root).saveDirectory)
    #expect(try FileManager.default.contentsOfDirectory(atPath: saves.path) == ["file1.rmmzsave"])
    #expect(GameFolder(root: root, webRoot: "www").saveDirectory?.path == root.appendingPathComponent("www/save").path)
}

/// MessageSkip's auto and skip reach Game commands only when the game turns the plugin on.
@Test func offersMessageSkipsCommandsWhenThePluginIsOn() throws {
    let root = FileManager.default.temporaryDirectory.appendingPathComponent("vndegen-\(UUID().uuidString)", isDirectory: true)
    defer { try? FileManager.default.removeItem(at: root) }
    try FileManager.default.createDirectory(at: root.appendingPathComponent("www/js"), withIntermediateDirectories: true)
    let plugins = """
    // Generated by RPG Maker.
    var $plugins =
    [
    {"name":"Community_Basic","status":true,"description":"","parameters":{}},
    {"name": "MessageSkip", "status": true, "description": "", "parameters": {"押し続けスキップ": "true"}},
    {"name":"MessageWindowHidden","status":false,"description":"","parameters":{}}
    ];
    """
    try Data(plugins.utf8).write(to: root.appendingPathComponent("www/js/plugins.js"))
    let folder = GameFolder(root: root, webRoot: "www")
    #expect(folder.enabledPlugins == ["Community_Basic", "MessageSkip"])
    #expect(RPGMakerPlugin.menuCommands(plugins: folder.enabledPlugins).map(\.title) == ["Auto", "Skip"])
    #expect(RPGMakerPlugin.menuCommands(plugins: ["Community_Basic"]).isEmpty)
    #expect(GameFolder(root: root, webRoot: "").enabledPlugins.isEmpty)
}
