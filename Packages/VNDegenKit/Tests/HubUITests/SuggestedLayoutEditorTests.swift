import Foundation
import TestSupport
import GameInput
import Testing
import VNCore
@testable import HubUI

private struct SuggestingEngine: EnginePlugin {
    var descriptor: EngineDescriptor {
        EngineDescriptor(id: EngineID("suggesting"), displayName: "Suggesting", shortName: "Suggesting", platforms: Set(Platform.allCases))
    }

    func detect(_ probe: FolderProbe) -> EngineMatch? { nil }
    @MainActor func makeSession(_ launch: GameLaunch) throws -> any EngineSession { throw CancellationError() }
    var controlReaderVersion: Int { 1 }

    func suggestedControls(folder: URL) -> ControlSuggestion? {
        ControlSuggestion(buttons: [.r2: .key(KeyboardKey(windowsKeyCode: 83)!)], labels: [.r2: "Skip"])
    }
}

@Test @MainActor func theSheetListsTheSuggestionFirstAndResetGoesBackToIt() async throws {
    let base = TestFolders.root.appendingPathComponent("suggested-editor-\(UUID().uuidString)", isDirectory: true)
    let paths = LibraryPaths(gamesRoot: base.appendingPathComponent("Games"), support: base.appendingPathComponent("Support"))
    let id = GameID("Test Game")
    try FileManager.default.createDirectory(at: paths.folder(for: id), withIntermediateDirectories: true)
    let library = LibraryStore(paths: paths, engines: [SuggestingEngine()])
    await library.refresh()
    library.update(id) { $0.engineOverride = EngineID("suggesting") }
    let layouts = LayoutLibrary(paths: paths)
    layouts.prepareSuggestion(for: try #require(library.game(id)), in: library)
    for _ in 0..<500 where layouts.suggestions[id] == nil { try await Task.sleep(for: .milliseconds(10)) }

    let editor = ButtonMappingEditor(library: library, layouts: layouts, gameID: id)
    #expect(editor.currentLayout?.id == .suggested && editor.currentLayout?.name == "Suggested for this game")
    #expect(editor.allLayouts.first?.id == .suggested)
    #expect(editor.label(for: .r2) == "Skip" && editor.label(for: .a) == nil)
    #expect(editor.hasSuggestion)

    editor.select(.layouts)
    #expect(editor.rows().first { $0.contains(.apply(.suggested)) } != nil)
    editor.select(.apply(.gamepad))
    #expect(library.game(id)?.buttonMapping == [:])
    #expect(editor.currentLayout?.id == .gamepad)
    #expect(editor.label(for: .r2) == nil)

    editor.select(.reset)
    #expect(library.game(id)?.buttonMapping == nil)
    #expect(editor.currentLayout?.id == .suggested)
}
