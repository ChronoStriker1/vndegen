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
@Test func licensePagesTakeScrollButtonsAndEachSheetGetsItsOwnPager() async throws {
    let (nav, _) = try await makeNavigator()
    nav.activate(.settingsPage(.licenses))
    let pager = nav.pager
    // Directions and shoulder buttons scroll rather than move a focus the page doesn't have. The pager's own tests check
    // where they land.
    press(nav, .down, .pageDown)
    #expect(nav.sheet != nil)
    #expect(nav.focus == nil)
    press(nav, .cancel)
    #expect(nav.sheet == nil)
    #expect(nav.pager !== pager)
}

@MainActor
@Test func settingsPagesOpenFreshExceptLudusaviWhichKeepsWhatWasTyped() async throws {
    let (nav, _) = try await makeNavigator()
    nav.activate(.settingsPage(.metadata))
    let metadata = nav.metadataSettings
    press(nav, .cancel)
    nav.activate(.settingsPage(.metadata))
    #expect(nav.metadataSettings !== metadata)

    nav.hub.input.noteInput(.gamepad)
    press(nav, .cancel)
    nav.activate(.settingsPage(.saves))
    let saves = nav.saveSettings
    press(nav, .down, .confirm)
    #expect(nav.keyboard != nil)
    // Closing the page under its keyboard, as a swipe does, drops the keyboard.
    nav.sheet = nil
    #expect(saves.keyboard == nil)
    nav.activate(.settingsPage(.saves))
    #expect(nav.saveSettings === saves)
}

@MainActor
@Test func stackedPageActionsTakeTheirOwnRowUnderPlay() async throws {
    let (nav, _) = try await makeNavigator()
    nav.setFocus(.pagePlay)
    press(nav, .right)
    #expect(nav.focus == .pageArtwork)
    // A narrow window puts Artwork and the rest under Play, so Down reaches them and Right has nowhere to go.
    nav.pageActionsStacked = true
    nav.setFocus(.pagePlay)
    press(nav, .right)
    #expect(nav.focus == .pagePlay)
    press(nav, .down)
    #expect(nav.focus == .pageArtwork)
    press(nav, .up)
    #expect(nav.focus == .pagePlay)
}

@MainActor
@Test func eachLibraryLayoutHasItsOwnHomeRowsAndTheViewButtonStepsThroughThem() async throws {
    let (nav, id) = try await makeNavigator()
    nav.hub.library.update(id) { $0.lastPlayedAt = .now }
    nav.back()
    #expect(nav.focus == .tile(.recent, id) || nav.focus == .tile(.all, id))
    nav.layout = .featured
    #expect(nav.rows().map(\.id) == ["bar", "hero", "recent", "all"])
    nav.layout = .capsules
    #expect(nav.rows().map(\.id) == ["bar", "recent", "all"])
    // The header list leaves out Recently played, which would repeat the top of All games.
    nav.layout = .headers
    #expect(nav.rows().map(\.id) == ["bar", "all-0"])
    nav.setFocus(.layoutButton)
    press(nav, .confirm)
    #expect(nav.layout == .featured)
    #expect(nav.focus == .layoutButton)
    #expect(UserDefaults.standard.string(forKey: LibraryLayout.storageKey) == LibraryLayout.featured.rawValue)
}

@MainActor
@Test func xOpensTheFocusedTilesMenuWithTheGamePagesActions() async throws {
    let (nav, id) = try await makeNavigator()
    nav.back()
    nav.layout = .capsules
    nav.setFocus(.tile(.all, id))
    press(nav, .dash)
    guard case .gameMenu(id)? = nav.sheet else { Issue.record("no game menu"); return }
    let actions: [Navigator.GameAction] = [.play, .openPage, .artwork, .metadata, .hide, .showFolder, .remove]
    #expect(nav.rows().flatMap(\.items) == actions.map { .gameAction($0) })
    #expect(nav.focus == .gameAction(.play))
    press(nav, .down, .confirm)
    #expect(nav.sheet == nil && nav.route == .game(id))
    nav.back()
    #expect(nav.focus == .tile(.all, id))
    press(nav, .dash, .down, .down, .confirm)
    guard case .artwork? = nav.sheet else { Issue.record("no artwork editor"); return }
    press(nav, .cancel)
    #expect(nav.sheet == nil && nav.focus == .tile(.all, id))
    // X does nothing where no game has focus.
    nav.setFocus(.settingsButton)
    press(nav, .dash)
    #expect(nav.sheet == nil)
}

@MainActor
@Test func removingAGameAsksFirstWithCancelFocused() async throws {
    let (nav, id) = try await makeNavigator()
    nav.perform(.remove, on: id)
    guard case .confirm? = nav.sheet else { Issue.record("no confirmation"); return }
    #expect(nav.focus == .confirmCancel)
    #expect(nav.rows().flatMap(\.items) == [.confirmAction, .confirmCancel])
    press(nav, .confirm)
    #expect(nav.sheet == nil)
    #expect(FileManager.default.fileExists(atPath: nav.hub.library.folder(for: id).path))
    #expect(nav.hub.library.games.map(\.id) == [id])
}

@MainActor
@Test func settingsListsEachAddedFolderWithItsButtons() async throws {
    let (nav, _) = try await makeNavigator()
    let added = nav.hub.library.paths.gamesRoot.deletingLastPathComponent().appendingPathComponent("Added", isDirectory: true)
    try FileManager.default.createDirectory(at: added, withIntermediateDirectories: true)
    try await nav.hub.addLibraryFolder(added)
    let folder = try #require(nav.hub.library.folders.first)
    nav.back()
    nav.push(.settings)
    let items = nav.rows().map(\.items)
    #expect(items.prefix(4) == [[.back], [.openFolder], [.showLibraryFolder(folder.id), .removeLibraryFolder(folder.id)], [.addLibraryFolder]])
    nav.activate(.removeLibraryFolder(folder.id))
    guard case let .confirm(confirmation)? = nav.sheet else { Issue.record("no confirmation"); return }
    // What Remove folder runs, awaited here rather than in the task the button starts.
    nav.sheet = nil
    await confirmation.perform()
    #expect(nav.hub.library.folders.isEmpty)
    #expect(FileManager.default.fileExists(atPath: added.path))
}
