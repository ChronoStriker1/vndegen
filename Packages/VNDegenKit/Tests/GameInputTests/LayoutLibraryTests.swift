import Foundation
import TestSupport
import Testing
import VNCore
@testable import GameInput

/// An engine with no gamepad support, whose old Confirm pressed Return, as in RPG Maker.
private struct KeyboardEngine: EnginePlugin {
    var descriptor: EngineDescriptor {
        EngineDescriptor(id: EngineID("keyboard"), displayName: "Keyboard", shortName: "Keyboard", platforms: Set(Platform.allCases))
    }

    func detect(_ probe: FolderProbe) -> EngineMatch? { nil }
    @MainActor func makeSession(_ launch: GameLaunch) throws -> any EngineSession { throw CancellationError() }
    func legacyAction(for button: VirtualButton) -> ControlAction? { button == .confirm ? .key(.enter) : nil }
    var defaultControls: ControlLayout { .keyboardAndMouse }
}

private func makePaths() throws -> LibraryPaths {
    let root = TestFolders.root.appending(path: "vndegen-layouts-\(UUID().uuidString)")
    let paths = LibraryPaths(gamesRoot: root.appending(path: "Games"), support: root.appending(path: "Support"))
    try FileManager.default.createDirectory(at: paths.support, withIntermediateDirectories: true)
    return paths
}

/// A library of folders named `games`, on the engine above unless a game's name starts with "Gamepad".
@MainActor
private func makeLibrary(_ paths: LibraryPaths, games: [String]) async throws -> LibraryStore {
    for name in games {
        try FileManager.default.createDirectory(at: paths.gamesRoot.appending(path: name), withIntermediateDirectories: true)
    }
    let library = LibraryStore(paths: paths, engines: [KeyboardEngine()])
    await library.refresh()
    for name in games where !name.hasPrefix("Gamepad") {
        library.update(GameID(name)) { $0.engineOverride = EngineID("keyboard") }
    }
    return library
}

@MainActor
private func buttons(_ name: String, _ library: LibraryStore) throws -> ButtonMapping {
    LayoutLibrary(paths: library.paths).mapping(for: try #require(library.game(GameID(name))), in: library)
}

@Test @MainActor func savedMappingsPlayTheSameAfterUpgrading() async throws {
    let paths = try makePaths()
    let scratch = try ScratchDefaults("vndegen-layouts")
    defer { scratch.remove() }
    let defaults = scratch.defaults
    // library.json as 0.22.2 writes it: a remapped game, a game that follows its engine, and one on the plain Xbox layout.
    // "Earlier" is as a version before gamepad buttons wrote it, with a button sending Confirm.
    let enter = #"{"key":{"_0":{"key":"Enter","code":"Enter","keyCode":13}}}"#
    let games = [
        ("Remapped", #"{"b":{"click":{"_0":"secondary"}},"x":{"nothing":{}},"y":\#(enter),"rightStickUp":{"mouseMove":{"_0":"up"}},"l":{"wheel":{"_0":"down"}}}"#),
        ("Follows", "null"),
        ("Xbox", "{}"),
        ("Earlier", #"{"a":{"button":{"_0":"confirm"}},"up":{"button":{"_0":"cancel"}}}"#),
    ]
    let records = games.map { name, mapping in
        #"{"id":"\#(name)","title":"\#(name)","artwork":{},"isLocked":false,"addedAt":"2025-01-01T00:00:00Z","playTime":0,"engineOverride":"keyboard","buttonMapping":\#(mapping)}"#
    }
    try Data("[\(records.joined(separator: ","))]".utf8).write(to: paths.libraryFile)
    for (name, _) in games {
        try FileManager.default.createDirectory(at: paths.gamesRoot.appending(path: name), withIntermediateDirectories: true)
    }

    let library = LibraryStore(paths: paths, engines: [KeyboardEngine()])
    await library.refresh()
    ButtonMapping.convertLegacyMappings(in: library, defaults: defaults)
    let layouts = LayoutLibrary(paths: paths)

    var remapped = ButtonMapping()
    remapped.set(.click(.secondary), for: .b)
    remapped.set(.nothing, for: .x)
    remapped.set(.key(.enter), for: .y)
    remapped.set(.mouseMove(.up), for: .rightStickUp)
    remapped.set(.wheel(.down), for: .l)
    var earlier = ButtonMapping()
    earlier.set(.key(.enter), for: .a)
    earlier.set(.gamepad(.b), for: .up)
    earlier.set(.gamepad(.b), for: .leftStickUp)
    let expected: [(String, ButtonMapping, ControllerLayout.ID?)] = [
        ("Remapped", remapped, nil),
        ("Follows", .keyboardAndMouse, .keyboardAndMouse),
        ("Xbox", ButtonMapping(), .gamepad),
        ("Earlier", earlier, nil),
    ]
    for (name, mapping, shown) in expected {
        let plays = try buttons(name, library)
        for input in ControlInput.allCases {
            #expect(plays.action(for: input) == mapping.action(for: input), "\(name) \(input)")
        }
        #expect(layouts.shown(for: plays)?.id == shown, "\(name)")
    }
    #expect(library.game(GameID("Follows"))?.buttonMapping == nil)
}

@Test @MainActor func savedLayoutsLastAndDeletingOneLeavesItsGames() async throws {
    let paths = try makePaths()
    let library = try await makeLibrary(paths, games: ["Game"])
    var mapping = ButtonMapping()
    mapping.set(.key(.enter), for: .a)
    mapping.set(.wheel(.up), for: .r)

    let layouts = LayoutLibrary(paths: paths)
    #expect(layouts.add(name: "  ", mapping: mapping) == nil)
    let id = try #require(layouts.add(name: " Mine ", mapping: mapping))
    library.update(GameID("Game")) { $0.buttonMapping = mapping.overrides }
    #expect(layouts.shown(for: try buttons("Game", library))?.name == "Mine")

    layouts.rename(id, to: "")
    layouts.rename(id, to: "Renamed")
    let reopened = LayoutLibrary(paths: paths)
    #expect(reopened.saved == [ControllerLayout(id: id, name: "Renamed", mapping: mapping)])
    #expect(reopened.all.map(\.id) == ControllerLayout.builtIns.map(\.id) + [id])

    // Built-in layouts can't be changed or deleted.
    reopened.update(.mouse, mapping: mapping, in: library)
    reopened.rename(.mouse, to: "Renamed")
    reopened.delete(.mouse)
    #expect(reopened.layout(.mouse) == ControllerLayout.builtIns[2])

    reopened.delete(id)
    #expect(LayoutLibrary(paths: paths).saved.isEmpty)
    #expect(try buttons("Game", library) == mapping)
    #expect(reopened.shown(for: mapping) == nil)
}

@Test @MainActor func oldDpadOrderMovesOnceForAliceSoftAndArtemisOnly() async throws {
    let paths = try makePaths()
    let library = try await makeLibrary(paths, games: ["Alice", "Artemis", "Partial", "Kirikiri", "Gamepad follows"])
    var oldOrder: [ControlInput: ControlAction] = [.a: .nothing]
    for (input, digit) in [(ControlInput.up, 1), (.down, 2), (.left, 3), (.right, 4)] { oldOrder[input] = .key(.digit(digit)) }
    var newOrder = oldOrder
    for (input, digit) in [(ControlInput.right, 2), (.down, 3), (.left, 4)] { newOrder[input] = .key(.digit(digit)) }
    var partial = oldOrder
    partial[.left] = .nothing
    let engines = ["Alice": "alicesoft", "Artemis": "artemis", "Partial": "alicesoft", "Kirikiri": "kirikiri"]
    for (name, engine) in engines {
        library.update(GameID(name)) {
            $0.engineOverride = EngineID(engine)
            $0.buttonMapping = name == "Partial" ? partial : oldOrder
        }
    }

    ButtonMapping.moveOldDpadOrder(in: library)
    #expect(library.game(GameID("Alice"))?.buttonMapping == newOrder)
    #expect(library.game(GameID("Artemis"))?.buttonMapping == newOrder)
    #expect(library.game(GameID("Partial"))?.buttonMapping == partial)
    #expect(library.game(GameID("Kirikiri"))?.buttonMapping == oldOrder)
    #expect(library.game(GameID("Gamepad follows"))?.buttonMapping == nil)

    // A player who sets the old order afterwards keeps it, in this run and after reopening the library.
    library.update(GameID("Alice")) { $0.buttonMapping = oldOrder }
    ButtonMapping.moveOldDpadOrder(in: library)
    ButtonMapping.moveOldDpadOrder(in: LibraryStore(paths: paths, engines: []))
    #expect(LibraryStore(paths: paths, engines: []).game(GameID("Alice"))?.buttonMapping == oldOrder)
}

@Test @MainActor func anUnreadableLayoutFileIsKeptRatherThanSavedOver() throws {
    let paths = try makePaths()
    let file = paths.support.appending(path: "controller-layouts.json")
    let unreadable = Data(#"[{"id":"not a UUID"}]"#.utf8)
    try unreadable.write(to: file)

    let layouts = LayoutLibrary(paths: paths)
    #expect(layouts.saved.isEmpty)
    #expect(layouts.add(name: "New", mapping: ButtonMapping()) != nil)

    let names = try FileManager.default.contentsOfDirectory(atPath: paths.support.path)
    let kept = try #require(names.first { $0.hasPrefix("controller-layouts-unreadable-") })
    #expect(try Data(contentsOf: paths.support.appending(path: kept)) == unreadable)
    #expect(LayoutLibrary(paths: paths).saved.map(\.name) == ["New"])
}

@Test @MainActor func editingALayoutMovesOnlyTheGamesThatShowIt() async throws {
    let paths = try makePaths()
    let names = ["Shows", "Tweaked", "Follows", "Gamepad follows", "Gone"]
    let library = try await makeLibrary(paths, games: names)
    let layouts = LayoutLibrary(paths: paths)
    var mapping = ButtonMapping()
    mapping.set(.key(.enter), for: .a)
    var tweaked = mapping
    tweaked.set(.nothing, for: .b)
    // Two layouts with the same buttons. Games show the first.
    let first = try #require(layouts.add(name: "First", mapping: mapping))
    let second = try #require(layouts.add(name: "Second", mapping: mapping))
    library.update(GameID("Shows")) { $0.buttonMapping = mapping.overrides }
    library.update(GameID("Gone")) { $0.buttonMapping = mapping.overrides }
    library.update(GameID("Tweaked")) { $0.buttonMapping = tweaked.overrides }
    try FileManager.default.removeItem(at: paths.gamesRoot.appending(path: "Gone"))
    await library.refresh()
    #expect(layouts.shown(for: try buttons("Shows", library))?.id == first)

    // The second layout changes alone, since no game shows it.
    var changed = mapping
    changed.set(.key(.escape), for: .x)
    layouts.update(second, mapping: changed, in: library)
    #expect(try buttons("Shows", library) == mapping)
    #expect(layouts.layout(second)?.mapping == changed)

    // The first takes the games that show it, whether or not their folder is there, and no others.
    var moved = mapping
    moved.set(.key(.tab), for: .y)
    layouts.update(first, mapping: moved, in: library)
    #expect(try buttons("Shows", library) == moved)
    #expect(try buttons("Gone", library) == moved)
    #expect(try buttons("Tweaked", library) == tweaked)
    #expect(library.game(GameID("Follows"))?.buttonMapping == nil)
    #expect(library.game(GameID("Gamepad follows"))?.buttonMapping == nil)

    // A game that lands on its engine's default follows that default again.
    layouts.update(first, mapping: .keyboardAndMouse, in: library)
    #expect(library.game(GameID("Shows"))?.buttonMapping == nil)
    #expect(try buttons("Shows", library) == .keyboardAndMouse)
}
