import Foundation
import GameInput
import Lock
import Metadata
import SaveSync
import Testing
import VNCore
@testable import HubUI

private struct TestPlugin: EnginePlugin {
    var descriptor: EngineDescriptor {
        EngineDescriptor(id: EngineID("test"), displayName: "Test", shortName: "Test", platforms: Set(Platform.allCases))
    }

    func detect(_ probe: FolderProbe) -> EngineMatch? {
        probe.contains("game.txt") ? EngineMatch(engine: descriptor.id, confidence: .certain, evidence: "game.txt") : nil
    }

    @MainActor func makeSession(_ launch: GameLaunch) throws -> any EngineSession { throw CancellationError() }

    func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] { [] }
}

/// A hub with one game, Test Game, and a navigator on its page.
@MainActor
private func makeNavigator() async throws -> (Navigator, GameID) {
    let base = FileManager.default.temporaryDirectory.appendingPathComponent("nav-\(UUID().uuidString)", isDirectory: true)
    let paths = LibraryPaths(gamesRoot: base.appendingPathComponent("Games"), support: base.appendingPathComponent("Support"))
    try FileManager.default.createDirectory(at: paths.gamesRoot.appendingPathComponent("Test Game"), withIntermediateDirectories: true)
    try Data().write(to: paths.gamesRoot.appendingPathComponent("Test Game/game.txt"))
    let library = LibraryStore(paths: paths, engines: [TestPlugin()])
    await library.refresh()
    let hub = Hub(library: library, metadata: MetadataService(), input: InputHub(),
                  lock: LockService(keychainService: "vndegen-tests-\(UUID().uuidString)", defaults: UserDefaults(suiteName: UUID().uuidString)!),
                  saveSync: SaveSyncService(library: library))
    let nav = Navigator(hub: hub)
    let id = GameID("Test Game")
    nav.push(.game(id))
    return (nav, id)
}

@MainActor
private func press(_ nav: Navigator, _ buttons: VirtualButton...) {
    for button in buttons { nav.handle(.button(button, pressed: true)) }
}

@MainActor
@Test func aSheetThatReplacesAnotherStartsOnItsOwnDefaultAndClosingReturnsToTheOpener() async throws {
    let (nav, id) = try await makeNavigator()
    nav.setFocus(.pageArtwork)
    nav.activate(.pageArtwork)
    #expect(nav.focus == .artworkSlot(.capsule))
    // Another sheet replaces the artwork editor, as a runtime license asked for meanwhile does.
    nav.sheet = .engine(id)
    #expect(nav.focus == .engineOption(nil))
    press(nav, .cancel)
    #expect(nav.sheet == nil)
    #expect(nav.focus == .pageArtwork)
}

@MainActor
@Test func aOrBClosesTheLaunchAlertWithoutLeavingTheScreenBehindIt() async throws {
    let (nav, id) = try await makeNavigator()
    nav.hub.launchError = "Test Game failed to start."
    press(nav, .down, .cancel)
    #expect(nav.hub.launchError == nil)
    #expect(nav.route == .game(id))
    #expect(nav.focus == .pagePlay)
    nav.hub.launchError = "Again."
    press(nav, .confirm)
    #expect(nav.hub.launchError == nil)
    #expect(nav.sheet == nil)
}

@MainActor
@Test func aControllerTypesTheMetadataSearchOnTheOnScreenKeyboard() async throws {
    let (nav, _) = try await makeNavigator()
    nav.hub.input.noteInput(.gamepad)
    nav.activate(.pageMetadata)
    guard case let .metadata(search)? = nav.sheet else { Issue.record("no metadata sheet"); return }
    search.query = ""
    press(nav, .confirm)
    #expect(search.keyboard != nil)
    #expect(!search.editingQuery)
    // The keyboard takes every button: these type q and w, delete the w, and type the s under it.
    press(nav, .confirm, .right, .confirm, .dash, .down, .confirm)
    #expect(search.query == "qs")
    press(nav, .cancel)
    #expect(search.keyboard == nil)
    #expect(nav.sheet != nil)
    #expect(nav.focus == .metadataQuery)
}

@MainActor
@Test func touchFocusesTheMetadataSearchFieldInstead() async throws {
    let (nav, _) = try await makeNavigator()
    nav.activate(.pageMetadata)
    guard case let .metadata(search)? = nav.sheet else { Issue.record("no metadata sheet"); return }
    nav.select(.metadataQuery)
    #expect(search.keyboard == nil)
    #expect(search.editingQuery)
}

@MainActor
@Test func theMappingEditorResetsJumpsGroupsAndStillWorksWithEveryButtonMappedToNothing() async throws {
    let (nav, id) = try await makeNavigator()
    let library = nav.hub.library
    library.update(id) { $0.buttonMapping = Dictionary(uniqueKeysWithValues: ControlInput.allCases.map { ($0, ControlAction.nothing) }) }
    // The hub reads controllers through the navigation layout whatever the game's mapping says.
    nav.attachInput()
    #expect(nav.hub.input.buttonMapping == nil)
    for element in GamepadElement.allCases {
        #expect(element.action(in: nav.hub.input.buttonMapping) != .nothing || element.control?.virtualButton == nil)
    }
    nav.activate(.pageMapping)
    guard case let .mapping(editor)? = nav.sheet else { Issue.record("no mapping sheet"); return }
    #expect(editor.mapping.action(for: .a) == .nothing)

    // RB jumps from the layouts to the buttons, and LB back.
    press(nav, .down, .pageDown)
    #expect(editor.focus == .input(.a))
    press(nav, .pageUp)
    #expect(editor.focus == .apply(.keyboardAndMouse))

    // Reset sits under the layouts and puts back the engine's layout.
    let reset = editor.rows().firstIndex { $0 == [.reset] }!
    for _ in 0..<reset { press(nav, .down) }
    #expect(editor.focus == .reset)
    press(nav, .confirm)
    #expect(library.game(id)?.buttonMapping == nil)
    #expect(editor.mapping == ButtonMapping())

    // Picking an action: RB jumps between kinds of action, B goes back to the button list, and B again closes.
    press(nav, .pageDown, .confirm)
    #expect(editor.picking == .a)
    press(nav, .pageDown)
    #expect(editor.focus == .action(ControlAction.keyChoices[0]))
    press(nav, .cancel)
    #expect(editor.picking == nil)
    press(nav, .cancel)
    #expect(nav.sheet == nil)
}

@MainActor
@Test func licensesScrollRatherThanLoseFocus() async throws {
    let (nav, _) = try await makeNavigator()
    nav.sheet = .settingsPage(.licenses)
    let pager = nav.pager
    press(nav, .down, .pageDown)
    #expect(nav.sheet != nil)
    nav.sheet = nil
    // Each sheet scrolls from its own top.
    #expect(nav.pager !== pager)
}
