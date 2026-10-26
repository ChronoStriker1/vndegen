import Foundation
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

private func makeTempLibrary() throws -> LibraryPaths {
    let base = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
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

@Test func controlMethodsSavedBeforeMouseStylesKeepTheirMeaning() throws {
    let saved = try JSONDecoder().decode([ControlMethod].self, from: Data(#"["touchGamepad", "touchMouse", "controllerOnly"]"#.utf8))
    #expect(saved == [.gamepad, .mouse(.cursor), .controller(mouse: .touch)])
    let all = try JSONDecoder().decode([ControlMethod].self, from: JSONEncoder().encode(ControlMethod.allCases))
    #expect(all == ControlMethod.allCases)
    #expect(Set(ControlMethod.allCases.map(\.rawValue)).count == ControlMethod.allCases.count)
}
