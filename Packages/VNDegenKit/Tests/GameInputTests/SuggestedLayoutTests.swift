import Foundation
import Synchronization
import TestSupport
import Testing
import VNCore
@testable import GameInput

private final class Reads: Sendable {
    let count = Mutex(0)
}

private struct SuggestingEngine: EnginePlugin {
    let version: Int
    let buttons: [ControlInput: ControlAction]
    let reads: Reads

    var descriptor: EngineDescriptor {
        EngineDescriptor(id: EngineID("suggesting"), displayName: "Suggesting", shortName: "Suggesting", platforms: Set(Platform.allCases))
    }

    func detect(_ probe: FolderProbe) -> EngineMatch? { nil }
    @MainActor func makeSession(_ launch: GameLaunch) throws -> any EngineSession { throw CancellationError() }
    var controlReaderVersion: Int { version }

    func suggestedControls(folder: URL) -> ControlSuggestion? {
        reads.count.withLock { $0 += 1 }
        return buttons.isEmpty ? nil : ControlSuggestion(buttons: buttons, labels: [.r2: "Skip"])
    }
}

private let skip = ControlAction.key(KeyboardKey(windowsKeyCode: 83)!)

@MainActor
private func makeLibrary(_ paths: LibraryPaths, engine: SuggestingEngine, games: [String]) async throws -> LibraryStore {
    for name in games {
        try FileManager.default.createDirectory(at: paths.gamesRoot.appending(path: name), withIntermediateDirectories: true)
    }
    let library = LibraryStore(paths: paths, engines: [engine])
    await library.refresh()
    for name in games { library.update(GameID(name)) { $0.engineOverride = engine.descriptor.id } }
    return library
}

@MainActor
private func prepared(_ layouts: LayoutLibrary, _ library: LibraryStore, _ names: [String], version: Int) async throws {
    for name in names { layouts.prepareSuggestion(for: try #require(library.game(GameID(name))), in: library) }
    for _ in 0..<500 where !names.allSatisfy({ layouts.suggestions[GameID($0)]?.version == version }) {
        try await Task.sleep(for: .milliseconds(10))
    }
    #expect(names.allSatisfy { layouts.suggestions[GameID($0)]?.version == version })
}

private func makePaths() throws -> LibraryPaths {
    let root = TestFolders.root.appending(path: "vndegen-suggested-\(UUID().uuidString)")
    let paths = LibraryPaths(gamesRoot: root.appending(path: "Games"), support: root.appending(path: "Support"))
    try FileManager.default.createDirectory(at: paths.support, withIntermediateDirectories: true)
    return paths
}

@Test @MainActor func aSuggestionBecomesTheStartingLayoutAndNeverTouchesAPlayersButtons() async throws {
    let paths = try makePaths()
    let reads = Reads()
    let engine = SuggestingEngine(version: 1, buttons: [.r2: skip], reads: reads)
    let library = try await makeLibrary(paths, engine: engine, games: ["Follows", "Changed"])
    library.update(GameID("Changed")) { $0.buttonMapping = [.a: .nothing] }
    let layouts = LayoutLibrary(paths: paths)
    let follows = try #require(library.game(GameID("Follows")))
    #expect(layouts.mapping(for: follows, in: library) == ButtonMapping())

    try await prepared(layouts, library, ["Follows", "Changed"], version: 1)
    #expect(layouts.mapping(for: follows, in: library).action(for: .r2) == skip)
    #expect(layouts.shown(for: layouts.mapping(for: follows, in: library), game: follows, in: library)?.id == .suggested)
    #expect(layouts.all(for: follows, in: library).map(\.id).prefix(2) == [.suggested, .visualNovel])
    #expect(layouts.all(for: follows, in: library).contains { $0.id == .gamepad })
    #expect(layouts.label(for: .r2, game: follows, in: library) == "Skip")

    let changed = try #require(library.game(GameID("Changed")))
    #expect(layouts.mapping(for: changed, in: library) == ButtonMapping([.a: .nothing]))
    #expect(layouts.label(for: .r2, game: changed, in: library) == nil)
    #expect(layouts.layout(.suggested, for: changed, in: library)?.mapping.action(for: .r2) == skip)

    let reopened = LayoutLibrary(paths: paths)
    #expect(reopened.suggestion(for: follows) == ControlSuggestion(buttons: [.r2: skip], labels: [.r2: "Skip"]))
    reopened.prepareSuggestion(for: follows, in: library)
    try await Task.sleep(for: .milliseconds(50))
    #expect(reads.count.withLock { $0 } == 2)
}

@Test @MainActor func aNewerReaderReplacesTheSuggestionAndLeavesStoredButtonsAlone() async throws {
    let paths = try makePaths()
    let old = SuggestingEngine(version: 1, buttons: [.r2: skip], reads: Reads())
    let library = try await makeLibrary(paths, engine: old, games: ["Follows", "Xbox"])
    let layouts = LayoutLibrary(paths: paths)
    try await prepared(layouts, library, ["Follows", "Xbox"], version: 1)
    let xbox = try #require(library.game(GameID("Xbox")))
    let stored = ButtonMapping().stored(default: layouts.defaultLayout(for: xbox, in: library))
    library.update(xbox.id) { $0.buttonMapping = stored }
    #expect(library.game(xbox.id)?.buttonMapping == [:])

    let newer = SuggestingEngine(version: 2, buttons: [.l2: skip], reads: Reads())
    let upgraded = LibraryStore(paths: paths, engines: [newer])
    await upgraded.refresh()
    try await prepared(layouts, upgraded, ["Follows", "Xbox"], version: 2)
    let follows = try #require(upgraded.game(GameID("Follows")))
    #expect(layouts.mapping(for: follows, in: upgraded) == ButtonMapping([.l2: skip]))
    #expect(upgraded.game(xbox.id)?.buttonMapping == [:])
    #expect(layouts.mapping(for: try #require(upgraded.game(xbox.id)), in: upgraded) == ButtonMapping())
}

@Test @MainActor func aGameWithNothingToSuggestKeepsItsEnginesLayoutAndIsNotReadAgain() async throws {
    let paths = try makePaths()
    let reads = Reads()
    let library = try await makeLibrary(paths, engine: SuggestingEngine(version: 1, buttons: [:], reads: reads), games: ["Plain"])
    let layouts = LayoutLibrary(paths: paths)
    try await prepared(layouts, library, ["Plain"], version: 1)
    let plain = try #require(library.game(GameID("Plain")))
    #expect(layouts.suggestion(for: plain) == nil)
    #expect(layouts.all(for: plain, in: library) == layouts.all)
    layouts.prepareSuggestion(for: plain, in: library)
    try await Task.sleep(for: .milliseconds(50))
    #expect(reads.count.withLock { $0 } == 1)
}

@Test func everyKeyTheEditorOffersMatchesItsWindowsKeyCode() {
    for key in KeyboardKey.choices { #expect(KeyboardKey(windowsKeyCode: key.keyCode) == key) }
}
