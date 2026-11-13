import Foundation
import TestSupport
import Testing
@testable import VNCore

private struct FakePlugin: EnginePlugin {
    let id: String
    let marker: String
    let confidence: DetectionConfidence
    var descriptor: EngineDescriptor { EngineDescriptor(id: EngineID(id), displayName: id, shortName: id, platforms: [.macOS]) }
    func detect(_ probe: FolderProbe) -> EngineMatch? {
        probe.contains(marker) ? EngineMatch(engine: descriptor.id, confidence: confidence, evidence: marker) : nil
    }
    func makeSession(_ launch: GameLaunch) throws -> any EngineSession { fatalError() }
}

private struct NeverAskedPlugin: EnginePlugin {
    var descriptor: EngineDescriptor { EngineDescriptor(id: EngineID("never"), displayName: "never", shortName: "never", platforms: [.macOS]) }
    func detect(_ probe: FolderProbe) -> EngineMatch? {
        Issue.record("asked after a certain match")
        return nil
    }
    func makeSession(_ launch: GameLaunch) throws -> any EngineSession { fatalError() }
}

private func makeTempLibrary() throws -> LibraryPaths {
    let base = TestFolders.root.appendingPathComponent(UUID().uuidString)
    return LibraryPaths(gamesRoot: base.appendingPathComponent("Games"), support: base.appendingPathComponent("Support"))
}

private func addGame(_ name: String, files: [String], to paths: LibraryPaths) throws {
    for file in files {
        let url = paths.gamesRoot.appendingPathComponent(name).appendingPathComponent(file)
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data().write(to: url)
    }
}

@Test func detectionPrefersTheMostConfidentEngine() {
    let probe = FolderProbe(root: URL(fileURLWithPath: "/"), entries: ["Game.exe", "WWW/JS/rpg_core.js"])
    let plugins: [any EnginePlugin] = [
        FakePlugin(id: "exe", marker: "game.exe", confidence: .weak),
        FakePlugin(id: "mv", marker: "www/js/rpg_core.js", confidence: .certain),
    ]
    #expect(EngineDetector.detect(probe, using: plugins)?.engine == EngineID("mv"))
    // A plugin after a certain match can't change the answer, so it isn't asked. It can be slow on a large folder.
    #expect(EngineDetector.detect(probe, using: plugins + [NeverAskedPlugin()])?.engine == EngineID("mv"))
}

@Test func folderScanListsEveryLevelToItsDepthAndNothingHidden() throws {
    let root = TestFolders.root.appendingPathComponent(UUID().uuidString)
    defer { try? FileManager.default.removeItem(at: root) }
    try FileManager.default.createDirectory(at: root.appendingPathComponent("a/b/c/d"), withIntermediateDirectories: true)
    try FileManager.default.createDirectory(at: root.appendingPathComponent("empty"), withIntermediateDirectories: true)
    try Data().write(to: root.appendingPathComponent("a/.hidden"))
    try Data().write(to: root.appendingPathComponent("a/b/file"))
    try FileManager.default.createSymbolicLink(atPath: root.appendingPathComponent("a/dangling").path, withDestinationPath: "/nonexistent")
    #expect(FolderProbe(scanning: root, maxDepth: 0).entries == [])
    #expect(FolderProbe(scanning: root, maxDepth: 1).entries == ["a", "empty"])
    #expect(FolderProbe(scanning: root, maxDepth: 2).entries == ["a", "empty", "a/b", "a/dangling"])
    #expect(FolderProbe(scanning: root, maxDepth: 3).entries == ["a", "empty", "a/b", "a/dangling", "a/b/c", "a/b/file"])
    #expect(FolderProbe(scanning: root, maxDepth: 4).entries == ["a", "empty", "a/b", "a/dangling", "a/b/c", "a/b/file", "a/b/c/d"])
}

@Test @MainActor func libraryDetectsGamesAndKeepsUserChangesAcrossRestarts() async throws {
    let paths = try makeTempLibrary()
    let plugins: [any EnginePlugin] = [FakePlugin(id: "mz", marker: "js/rmmz_core.js", confidence: .certain)]
    try addGame("No Socks RPG", files: ["js/rmmz_core.js", "index.html"], to: paths)
    try addGame("Mystery", files: ["data.bin"], to: paths)

    let library = LibraryStore(paths: paths, engines: plugins)
    await library.refresh()
    #expect(library.games.map(\.id.rawValue).sorted() == ["Mystery", "No Socks RPG"])
    #expect(library.game(GameID("No Socks RPG"))?.engine == EngineID("mz"))
    #expect(library.game(GameID("Mystery"))?.engine == nil)

    library.update(GameID("Mystery")) { $0.engineOverride = EngineID("mz"); $0.isLocked = true }
    try library.setArtwork(Data([1, 2, 3]), fileExtension: "png", slot: .capsule, for: GameID("Mystery"))

    let reopened = LibraryStore(paths: paths, engines: plugins)
    await reopened.refresh()
    let mystery = try #require(reopened.game(GameID("Mystery")))
    #expect(mystery.engine == EngineID("mz"))
    #expect(mystery.isLocked)
    let art = try #require(reopened.artworkURL(.capsule, for: mystery.id))
    #expect(try Data(contentsOf: art) == Data([1, 2, 3]))

    try FileManager.default.removeItem(at: paths.folder(for: GameID("Mystery")))
    await reopened.refresh()
    #expect(reopened.games.map(\.id.rawValue) == ["No Socks RPG"])
}

@Test @MainActor func controlsPersistAndOlderLibrariesStillLoad() async throws {
    let paths = try makeTempLibrary()
    let plugins: [any EnginePlugin] = [FakePlugin(id: "mz", marker: "js/rmmz_core.js", confidence: .certain)]
    try addGame("No Socks RPG", files: ["js/rmmz_core.js"], to: paths)
    // A library.json from before control methods and button mappings existed.
    try FileManager.default.createDirectory(at: paths.support, withIntermediateDirectories: true)
    try Data("""
    [{"id": "No Socks RPG", "title": "No Socks RPG", "artwork": {}, "isLocked": true,
      "addedAt": "2025-01-01T00:00:00Z", "playTime": 60, "orientation": "portrait"}]
    """.utf8).write(to: paths.libraryFile)

    let library = LibraryStore(paths: paths, engines: plugins)
    await library.refresh()
    let id = GameID("No Socks RPG")
    let old = try #require(library.game(id))
    #expect(old.isLocked && old.orientation == .portrait)
    #expect(old.controlMethod == nil && old.buttonMapping == nil)

    let escape = KeyboardKey(key: "Escape", code: "Escape", keyCode: 27)
    let mapping: [ControlInput: ControlAction] = [.y: .key(escape), .r2: .click(.secondary), .x: .nothing, .a: .button(.cancel)]
    library.update(id) {
        $0.controlMethod = .controller(mouse: .cursor)
        $0.buttonMapping = mapping
    }

    let reopened = LibraryStore(paths: paths, engines: plugins)
    await reopened.refresh()
    let game = try #require(reopened.game(id))
    #expect(game.controlMethod == .controller(mouse: .cursor))
    #expect(game.buttonMapping == mapping)
    #expect(game.isLocked && game.playTime == 60)
}

@Test @MainActor func valuesFromANewerVersionCostOnlyThemselves() async throws {
    let paths = try makeTempLibrary()
    try addGame("Secret Game", files: ["data.bin"], to: paths)
    try addGame("Other Game", files: ["data.bin"], to: paths)
    try FileManager.default.createDirectory(at: paths.support, withIntermediateDirectories: true)
    try Data("""
    [{"id": "Secret Game", "title": "My Secret Title", "artwork": {"capsule": "capsule-1.png", "banner": "banner-1.png"},
      "isLocked": true, "addedAt": "2026-01-01T00:00:00Z", "lastPlayedAt": "2026-11-01T00:00:00Z", "playTime": 36000,
      "orientation": "upsideDown", "controlMethod": "telepathy",
      "buttonMapping": {"a": {"nothing": {}}, "b": {"teleport": {}}, "paddle1": {"nothing": {}}},
      "metadata": {"title": "Secret", "developers": ["Studio"], "publishers": [], "genres": [],
                   "refs": [{"source": "mobygames", "id": "1"}, {"source": "vndb", "id": "v2"}]}},
     {"title": "No ID"},
     {"id": "Other Game", "title": "Other Game", "artwork": {}, "isLocked": false, "addedAt": "2026-01-01T00:00:00Z",
      "playTime": 0}]
    """.utf8).write(to: paths.libraryFile)

    let library = LibraryStore(paths: paths, engines: [])
    await library.refresh()
    let secret = try #require(library.game(GameID("Secret Game")))
    #expect(secret.title == "My Secret Title" && secret.isLocked && secret.playTime == 36000)
    #expect(secret.lastPlayedAt != nil && secret.artwork == [.capsule: "capsule-1.png"])
    #expect(secret.orientation == nil && secret.controlMethod == nil)
    #expect(secret.buttonMapping == [.a: .nothing])
    #expect(secret.metadata?.title == "Secret" && secret.metadata?.developers == ["Studio"])
    #expect(secret.metadata?.refs == [ExternalRef(source: .vndb, id: "v2")])
    #expect(library.games.map(\.id.rawValue).sorted() == ["Other Game", "Secret Game"])

    library.update(secret.id) { $0.playTime += 60 }
    let saved = try #require(JSONSerialization.jsonObject(with: Data(contentsOf: paths.libraryFile)) as? [[String: Any]])
    #expect(saved.contains { $0["id"] == nil && $0["title"] as? String == "No ID" })
    #expect(LibraryStore(paths: paths, engines: []).game(secret.id)?.playTime == 36060)
}

@Test @MainActor func aLibraryFileThatIsNotJSONIsSetAsideNotOverwritten() async throws {
    let paths = try makeTempLibrary()
    try addGame("Game", files: ["data.bin"], to: paths)
    try FileManager.default.createDirectory(at: paths.support, withIntermediateDirectories: true)
    let damaged = Data("[{\"id\": \"Game\", \"isLocked\": tr".utf8)
    try damaged.write(to: paths.libraryFile)
    try damaged.write(to: paths.foldersFile)

    let library = LibraryStore(paths: paths, engines: [])
    await library.refresh()
    #expect(library.games.map(\.id.rawValue) == ["Game"])
    let aside = try FileManager.default.contentsOfDirectory(atPath: paths.support.path).filter { $0.contains("unreadable") }
    #expect(aside.count == 2)
    for name in aside { #expect(try Data(contentsOf: paths.support.appendingPathComponent(name)) == damaged) }
}

@Test func controlMethodsSavedBeforeMouseStylesKeepTheirMeaning() throws {
    let saved = try JSONDecoder().decode([ControlMethod].self, from: Data(#"["touchGamepad", "touchMouse", "controllerOnly"]"#.utf8))
    #expect(saved == [.gamepad, .mouse(.cursor), .controller(mouse: .touch)])
    let all = try JSONDecoder().decode([ControlMethod].self, from: JSONEncoder().encode(ControlMethod.allCases))
    #expect(all == ControlMethod.allCases)
    #expect(Set(ControlMethod.allCases.map(\.rawValue)).count == ControlMethod.allCases.count)
}
