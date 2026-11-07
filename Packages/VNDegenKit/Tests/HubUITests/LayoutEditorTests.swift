import Foundation
import TestSupport
import GameInput
import Testing
import VNCore
@testable import HubUI

/// An editor on Test Game, which follows the Xbox layout.
@MainActor
private func makeEditor() async throws -> (ButtonMappingEditor, LibraryStore, LayoutLibrary, GameID) {
    let base = TestFolders.root.appendingPathComponent("layouts-\(UUID().uuidString)", isDirectory: true)
    let paths = LibraryPaths(gamesRoot: base.appendingPathComponent("Games"), support: base.appendingPathComponent("Support"))
    let id = GameID("Test Game")
    try FileManager.default.createDirectory(at: paths.folder(for: id), withIntermediateDirectories: true)
    let library = LibraryStore(paths: paths, engines: [])
    await library.refresh()
    let layouts = LayoutLibrary(paths: paths)
    return (ButtonMappingEditor(library: library, layouts: layouts, gameID: id), library, layouts, id)
}

@MainActor
private func press(_ editor: ButtonMappingEditor, _ buttons: VirtualButton...) {
    for button in buttons {
        if let keyboard = editor.keyboard { keyboard.handle(button) } else { _ = editor.handle(button) }
    }
}

/// Moves focus to `target` and presses A, by controller.
@MainActor
private func activate(_ editor: ButtonMappingEditor, _ target: ButtonMappingEditor.Target) throws {
    let rows = editor.rows()
    let row = try #require(rows.firstIndex { $0.contains(target) })
    let focus = try #require(editor.focus)
    let start = try #require(rows.firstIndex { $0.contains(focus) })
    // The first press only reveals the ring, and moving past an edge stays there.
    press(editor, .left)
    for _ in 0..<abs(row - start) { press(editor, row > start ? .down : .up) }
    for _ in rows[row] { press(editor, .left) }
    for _ in 0..<(rows[row].firstIndex(of: target) ?? 0) { press(editor, .right) }
    #expect(editor.focus == target)
    press(editor, .confirm)
}

@Test @MainActor func aControllerPicksSavesEditsAndDeletesALayout() async throws {
    let (editor, library, layouts, id) = try await makeEditor()
    #expect(editor.focus == .apply(.gamepad))

    // A built-in layout copies into the game.
    try activate(editor, .apply(.rpgMaker))
    #expect(library.game(id)?.buttonMapping == ButtonMapping.rpgMaker.overrides)
    #expect(editor.currentLayout?.id == .rpgMaker)

    // A mapping already in the library can't be saved again.
    try activate(editor, .saveNew)
    #expect(editor.keyboard == nil)

    // Changing a button changes the game and leaves the built-in alone.
    try activate(editor, .input(.a))
    #expect(editor.picking == .a)
    try activate(editor, .action(.nothing))
    #expect(editor.picking == nil)
    #expect(editor.mapping.action(for: .a) == .nothing)
    #expect(editor.currentLayout == nil)
    #expect(ControllerLayout.builtIns[2].mapping == .rpgMaker)

    // B on the keyboard saves nothing, and neither does a blank name.
    try activate(editor, .saveNew)
    press(editor, .confirm, .cancel)
    #expect(editor.keyboard == nil && layouts.saved.isEmpty)
    try activate(editor, .saveNew)
    press(editor, .menu)
    #expect(editor.keyboard == nil && layouts.saved.isEmpty)

    // Typing a name saves the game's buttons, and the game now shows the layout.
    try activate(editor, .saveNew)
    press(editor, .confirm, .menu)
    let saved = try #require(layouts.saved.first)
    #expect(saved.name == "q" && saved.mapping == editor.mapping)
    #expect(editor.currentLayout?.id == saved.id)
    #expect(editor.focus == .apply(saved.id))

    // Editing the layout goes to the layout, and to the game showing it. Built-ins have no Edit.
    #expect(editor.rows().contains { $0 == [.apply(saved.id), .edit(saved.id)] })
    #expect(editor.rows().contains { $0 == [.apply(.mouse)] })
    try activate(editor, .edit(saved.id))
    #expect(editor.editing == saved.id)
    try activate(editor, .input(.b))
    try activate(editor, .action(.showKeyboard))
    #expect(layouts.layout(saved.id)?.mapping.action(for: .b) == .showKeyboard)
    #expect(library.game(id)?.buttonMapping?[.b] == .showKeyboard)

    // Rename opens the keyboard on the name, and B steps back to the list.
    try activate(editor, .rename)
    press(editor, .confirm, .menu)
    #expect(layouts.layout(saved.id)?.name == "qq")
    press(editor, .cancel)
    #expect(editor.editing == nil && editor.focus == .edit(saved.id))
    #expect(editor.handle(.cancel) == false)

    // Deleting asks first, B cancels, and the game keeps its buttons.
    let buttons = library.game(id)?.buttonMapping
    press(editor, .confirm)
    try activate(editor, .delete)
    #expect(editor.confirmingDelete && editor.focus == .cancelDelete)
    press(editor, .cancel)
    #expect(!editor.confirmingDelete && !layouts.saved.isEmpty && editor.focus == .delete)
    try activate(editor, .delete)
    try activate(editor, .confirmDelete)
    #expect(layouts.saved.isEmpty && editor.editing == nil && editor.currentLayout == nil)
    #expect(library.game(id)?.buttonMapping == buttons)
}

@Test @MainActor func editingOneOfTwoMatchingLayoutsLeavesTheGameShowingTheOther() async throws {
    let (editor, library, layouts, id) = try await makeEditor()
    var mapping = ButtonMapping()
    mapping.set(.nothing, for: .a)
    let first = try #require(layouts.add(name: "First", mapping: mapping))
    let second = try #require(layouts.add(name: "Second", mapping: mapping))
    library.update(id) { $0.buttonMapping = mapping.overrides }
    #expect(editor.currentLayout?.id == first)

    try activate(editor, .edit(second))
    try activate(editor, .input(.b))
    try activate(editor, .action(.nothing))
    #expect(layouts.layout(second)?.mapping.action(for: .b) == .nothing)
    #expect(library.game(id)?.buttonMapping == mapping.overrides)
    press(editor, .cancel)
    #expect(editor.currentLayout?.id == first)
}
