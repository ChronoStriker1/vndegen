import Foundation
import Testing
@testable import VNCore

private struct MarkerPlugin: EnginePlugin {
    var descriptor: EngineDescriptor { EngineDescriptor(id: EngineID("mz"), displayName: "MZ", shortName: "MZ", platforms: [.macOS]) }
    func detect(_ probe: FolderProbe) -> EngineMatch? {
        probe.contains("js/rmmz_core.js") ? EngineMatch(engine: descriptor.id, confidence: .certain, evidence: "js/rmmz_core.js") : nil
    }
    func makeSession(_ launch: GameLaunch) throws -> any EngineSession { fatalError() }
}

private struct Scratch {
    let base = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
    var paths: LibraryPaths { LibraryPaths(gamesRoot: base.appendingPathComponent("Games"), support: base.appendingPathComponent("Support")) }
    func folder(_ name: String) -> URL { base.appendingPathComponent(name, isDirectory: true) }

    func addGame(_ name: String, in root: URL) throws {
        let marker = root.appendingPathComponent(name).appendingPathComponent("js/rmmz_core.js")
        try FileManager.default.createDirectory(at: marker.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data().write(to: marker)
    }

    @MainActor func library() -> LibraryStore {
        let library = LibraryStore(paths: paths, engines: [MarkerPlugin()])
        library.deletesToTrash = false
        return library
    }
}

/// Makes `folder` unreadable, as a drive that was unplugged or a share that went offline is, or readable again. Moving
/// the folder wouldn't do: on a Mac its bookmark follows it.
private func setReachable(_ folder: URL, _ reachable: Bool) throws {
    try FileManager.default.setAttributes([.posixPermissions: reachable ? 0o755 : 0], ofItemAtPath: folder.path)
}

@MainActor private func ids(_ library: LibraryStore) -> [String] { library.games.map(\.id.rawValue).sorted() }

/// A library.json written by VNDegen 0.13, before library folders: the demo library and an RGSS test library.
@Test @MainActor func aLibraryFromBeforeFoldersKeepsEveryGame() async throws {
    let scratch = Scratch()
    let fixture = URL(filePath: #filePath).deletingLastPathComponent().appending(path: "Fixtures/library-0.13.json")
    let decoder = JSONDecoder()
    decoder.dateDecodingStrategy = .iso8601
    let before = try decoder.decode([Game].self, from: Data(contentsOf: fixture))
    try FileManager.default.createDirectory(at: scratch.paths.support, withIntermediateDirectories: true)
    try FileManager.default.copyItem(at: fixture, to: scratch.paths.libraryFile)
    for game in before { try scratch.addGame(game.id.rawValue, in: scratch.paths.gamesRoot) }

    let library = scratch.library()
    await library.refresh()
    // A folder added beside them, with a game named like one of theirs, takes nothing from them.
    let external = scratch.folder("External")
    try scratch.addGame(before[0].id.rawValue, in: external)
    try await library.addFolder(external)

    let reopened = scratch.library()
    await reopened.refresh()
    #expect(reopened.games.count == before.count + 1)
    for old in before {
        var game = try #require(reopened.game(old.id))
        #expect(game.libraryFolder == nil && game.folderName == nil)
        #expect(reopened.folder(for: old.id) == scratch.paths.folder(for: old.id))
        // Detection runs again on every scan, so only the detected engine may differ.
        game.detected = old.detected
        #expect(game == old)
    }
}

@Test @MainActor func gamesOfTheSameNameInTwoFoldersGetTheirOwnStableIDs() async throws {
    let scratch = Scratch()
    let external = scratch.folder("External")
    for name in ["Shared", "Only Here"] { try scratch.addGame(name, in: scratch.paths.gamesRoot) }
    for name in ["Shared", "Only There"] { try scratch.addGame(name, in: external) }

    let library = scratch.library()
    try await library.addFolder(external)
    #expect(ids(library) == ["Only Here", "Only There", "Shared", "Shared (External)"])
    let twin = GameID("Shared (External)")
    #expect(library.game(twin)?.title == "Shared")
    #expect(library.folder(for: twin).standardizedFileURL == external.appendingPathComponent("Shared").standardizedFileURL)
    #expect(library.folder(for: GameID("Shared")) == scratch.paths.folder(for: GameID("Shared")))
    #expect(library.gameID(named: "Shared", in: library.folders[0].id) == twin)
    #expect(library.game(twin)?.engine == EngineID("mz"))

    library.update(twin) { $0.playTime = 90 }
    let reopened = scratch.library()
    await reopened.refresh()
    #expect(ids(reopened) == ids(library))
    #expect(reopened.game(twin)?.playTime == 90)
}

@Test @MainActor func anUnreachableFolderKeepsItsGamesUntilItIsBack() async throws {
    let scratch = Scratch()
    let external = scratch.folder("External")
    try scratch.addGame("Away", in: external)
    let library = scratch.library()
    try await library.addFolder(external)
    let away = GameID("Away")
    #expect(library.unavailableReason(away) == nil)

    // The drive is unplugged.
    try setReachable(external, false)
    defer { try? setReachable(external, true) }
    await library.refresh()
    #expect(ids(library) == ["Away"])
    let reason = try #require(library.unavailableReason(away))
    #expect(library.folderProblems[library.folders[0].id] == reason)

    // Restarted while it's away, the library still lists the game.
    let reopened = scratch.library()
    await reopened.refresh()
    #expect(ids(reopened) == ["Away"] && reopened.unavailableReason(away) != nil)

    try setReachable(external, true)
    await reopened.refresh()
    #expect(ids(reopened) == ["Away"])
    #expect(reopened.unavailableReason(away) == nil && reopened.folderProblems.isEmpty)
}

#if canImport(Darwin)
@Test @MainActor func aRenamedFolderIsFoundThroughItsBookmark() async throws {
    let scratch = Scratch()
    let external = scratch.folder("External")
    try scratch.addGame("Travels", in: external)
    let library = scratch.library()
    try await library.addFolder(external)
    let renamed = scratch.folder("Renamed")
    try FileManager.default.moveItem(at: external, to: renamed)

    let reopened = scratch.library()
    await reopened.refresh()
    #expect(ids(reopened) == ["Travels"] && reopened.folderProblems.isEmpty)
    #expect(reopened.folders[0].url.standardizedFileURL.resolvingSymlinksInPath() == renamed.standardizedFileURL.resolvingSymlinksInPath())
    #expect(reopened.folder(for: GameID("Travels")).lastPathComponent == "Travels")
}
#endif

@Test @MainActor func removingAFolderLeavesItsFilesAndAddingItBackRestoresItsGames() async throws {
    let scratch = Scratch()
    let external = scratch.folder("External")
    try scratch.addGame("Kept", in: external)
    let library = scratch.library()
    try await library.addFolder(external)
    let kept = GameID("Kept")
    library.update(kept) { $0.playTime = 600 }
    try library.setArtwork(Data([7]), fileExtension: "png", slot: .capsule, for: kept)

    await library.removeFolder(library.folders[0].id)
    #expect(library.games.isEmpty && library.folders.isEmpty)
    #expect(FileManager.default.fileExists(atPath: external.appendingPathComponent("Kept/js/rmmz_core.js").path))

    try await library.addFolder(external)
    #expect(ids(library) == ["Kept"])
    #expect(library.game(kept)?.playTime == 600)
    #expect(try Data(contentsOf: #require(library.artworkURL(.capsule, for: kept))) == Data([7]))
}

@Test @MainActor func aGameMovedToAnotherFolderKeepsItsRecord() async throws {
    let scratch = Scratch()
    let external = scratch.folder("External")
    try scratch.addGame("Mover", in: scratch.paths.gamesRoot)
    try FileManager.default.createDirectory(at: external, withIntermediateDirectories: true)
    let library = scratch.library()
    try await library.addFolder(external)
    let mover = GameID("Mover")
    library.update(mover) { $0.playTime = 42 }

    try FileManager.default.moveItem(at: scratch.paths.folder(for: mover), to: external.appendingPathComponent("Mover"))
    await library.refresh()
    #expect(ids(library) == ["Mover"])
    #expect(library.game(mover)?.playTime == 42)
    #expect(library.game(mover)?.libraryFolder == library.folders[0].id)
}

@Test @MainActor func foldersThatOverlapTheLibraryAreRefused() async throws {
    let scratch = Scratch()
    let external = scratch.folder("External")
    try FileManager.default.createDirectory(at: external.appendingPathComponent("Inner"), withIntermediateDirectories: true)
    try FileManager.default.createDirectory(at: scratch.paths.gamesRoot, withIntermediateDirectories: true)
    let library = scratch.library()
    await #expect(throws: LibraryFolderError.self) { try await library.addFolder(scratch.paths.gamesRoot) }
    await #expect(throws: LibraryFolderError.self) { try await library.addFolder(scratch.base) }
    try await library.addFolder(external)
    await #expect(throws: LibraryFolderError.self) { try await library.addFolder(external) }
    await #expect(throws: LibraryFolderError.self) { try await library.addFolder(external.appendingPathComponent("Inner")) }
    #expect(library.folders.count == 1)
}

@Test @MainActor func deletingAGameFolderKeepsItsRecordAndNothingElse() async throws {
    let scratch = Scratch()
    let fm = FileManager.default
    for name in ["Doomed", "Neighbour"] { try scratch.addGame(name, in: scratch.paths.gamesRoot) }
    let library = scratch.library()
    await library.refresh()
    let doomed = GameID("Doomed")
    library.update(doomed) { $0.playTime = 5 }
    let data = scratch.paths.dataDirectory(for: doomed)
    try fm.createDirectory(at: data, withIntermediateDirectories: true)
    try Data([1]).write(to: data.appendingPathComponent("save.dat"))

    try await library.deleteFolder(of: doomed)
    #expect(ids(library) == ["Neighbour"])
    #expect(!fm.fileExists(atPath: scratch.paths.folder(for: doomed).path))
    #expect(fm.fileExists(atPath: data.appendingPathComponent("save.dat").path))
    #expect(library.game(doomed)?.playTime == 5)

    // Putting the folder back restores the game with its record.
    try scratch.addGame("Doomed", in: scratch.paths.gamesRoot)
    await library.refresh()
    #expect(ids(library) == ["Doomed", "Neighbour"] && library.game(doomed)?.playTime == 5)
}

@Test @MainActor func deletingRefusesLinksAndUnreachableFolders() async throws {
    let scratch = Scratch()
    let fm = FileManager.default
    // A game folder that is a link to a folder outside the library.
    let outside = scratch.folder("Outside")
    try scratch.addGame("Target", in: outside)
    try fm.createDirectory(at: scratch.paths.gamesRoot, withIntermediateDirectories: true)
    try fm.createSymbolicLink(at: scratch.paths.gamesRoot.appendingPathComponent("Linked"), withDestinationURL: outside.appendingPathComponent("Target"))
    let external = scratch.folder("External")
    try scratch.addGame("Away", in: external)

    let library = scratch.library()
    try await library.addFolder(external)
    #expect(ids(library) == ["Away", "Linked"])
    await #expect(throws: LibraryFolderError.self) { try await library.deleteFolder(of: GameID("Linked")) }
    #expect(fm.fileExists(atPath: outside.appendingPathComponent("Target/js/rmmz_core.js").path))

    try setReachable(external, false)
    await library.refresh()
    await #expect(throws: LibraryFolderError.self) { try await library.deleteFolder(of: GameID("Away")) }
    try setReachable(external, true)
    #expect(fm.fileExists(atPath: external.appendingPathComponent("Away").path))
}

@Test func aSlowValueGivesWayToTheTimeout() async {
    let slow = Task<Int, Never> {
        try? await Task.sleep(for: .seconds(5))
        return 1
    }
    let start = ContinuousClock.now
    #expect(await LibraryStore.value(of: slow, within: .milliseconds(100)) == nil)
    #expect(ContinuousClock.now - start < .seconds(2))
    #expect(await LibraryStore.value(of: Task { 2 }, within: .seconds(5)) == 2)
    slow.cancel()
}

@Test @MainActor func namesThatDifferOnlyInCaseGetSeparateIDs() async throws {
    let scratch = Scratch()
    let external = scratch.folder("External")
    try scratch.addGame("Game", in: scratch.paths.gamesRoot)
    try scratch.addGame("game", in: external)
    let library = scratch.library()
    try await library.addFolder(external)
    // The artwork and data folders are named for the ID, and a Mac volume would fold "Game" and "game" together.
    #expect(ids(library) == ["Game", "game (External)"])
    #expect(library.game(GameID("game (External)"))?.folderName == "game")
}

@Test @MainActor func aFolderAddedBackKeepsItsOwnRecordsWhenItsTwinIsGone() async throws {
    let scratch = Scratch()
    let fm = FileManager.default
    let external = scratch.folder("External")
    try scratch.addGame("Game", in: scratch.paths.gamesRoot)
    try scratch.addGame("Game", in: external)
    let library = scratch.library()
    try await library.addFolder(external)
    let twin = GameID("Game (External)")
    library.update(twin) { $0.playTime = 77 }
    library.update(GameID("Game")) { $0.playTime = 11 }

    await library.removeFolder(library.folders[0].id)
    #expect(library.unavailableReason(twin) != nil)
    // Its record never points at the built-in game's folder.
    #expect(library.folder(for: twin).standardizedFileURL == external.appendingPathComponent("Game").standardizedFileURL)
    try fm.removeItem(at: scratch.paths.folder(for: GameID("Game")))
    await library.refresh()

    try await library.addFolder(external)
    #expect(ids(library) == ["Game (External)"])
    #expect(library.game(twin)?.playTime == 77 && library.game(GameID("Game"))?.playTime == 11)
    #expect(library.unavailableReason(twin) == nil)
    // Restarted, the folder is the same one.
    let reopened = scratch.library()
    await reopened.refresh()
    #expect(ids(reopened) == ["Game (External)"])
}

@Test @MainActor func anotherFolderNeverTakesARemovedFoldersRecords() async throws {
    let scratch = Scratch()
    let first = scratch.folder("A").appendingPathComponent("External", isDirectory: true)
    let second = scratch.folder("B").appendingPathComponent("External", isDirectory: true)
    try scratch.addGame("Game", in: scratch.paths.gamesRoot)
    try scratch.addGame("Game", in: first)
    try scratch.addGame("Game", in: second)
    let library = scratch.library()
    try await library.addFolder(first)
    let original = GameID("Game (External)")
    library.update(original) { $0.playTime = 300 }

    await library.removeFolder(library.folders[0].id)
    try await library.addFolder(second)
    #expect(ids(library) == ["Game", "Game (External) 2"])
    #expect(library.game(GameID("Game (External) 2"))?.playTime == 0)

    try await library.addFolder(first)
    #expect(ids(library) == ["Game", "Game (External)", "Game (External) 2"])
    #expect(library.game(original)?.playTime == 300)
    #expect(library.folder(for: original).standardizedFileURL == first.appendingPathComponent("Game").standardizedFileURL)
}

@Test @MainActor func aGameWithAQualifiedIDKeepsItWhenMovedToAnotherFolder() async throws {
    let scratch = Scratch()
    let fm = FileManager.default
    let external = scratch.folder("External"), other = scratch.folder("Other")
    try scratch.addGame("Game", in: scratch.paths.gamesRoot)
    try scratch.addGame("Game", in: external)
    try fm.createDirectory(at: other, withIntermediateDirectories: true)
    let library = scratch.library()
    try await library.addFolder(external)
    try await library.addFolder(other)
    let twin = GameID("Game (External)")
    library.update(twin) { $0.playTime = 12 }

    try fm.moveItem(at: external.appendingPathComponent("Game"), to: other.appendingPathComponent("Game"))
    await library.refresh()
    #expect(ids(library) == ["Game", "Game (External)"])
    #expect(library.game(twin)?.playTime == 12)
    #expect(library.game(twin)?.libraryFolder == library.folders[1].id)
}

#if canImport(Darwin)
@Test @MainActor func aRemovedFolderRenamedMeanwhileComesBackAsItself() async throws {
    let scratch = Scratch()
    let external = scratch.folder("External")
    try scratch.addGame("Game", in: scratch.paths.gamesRoot)
    try scratch.addGame("Game", in: external)
    let library = scratch.library()
    try await library.addFolder(external)
    library.update(GameID("Game (External)")) { $0.playTime = 34 }
    await library.removeFolder(library.folders[0].id)

    let renamed = scratch.folder("Renamed")
    try FileManager.default.moveItem(at: external, to: renamed)
    try await library.addFolder(renamed)
    #expect(ids(library) == ["Game", "Game (External)"])
    #expect(library.game(GameID("Game (External)"))?.playTime == 34)
}
#endif
