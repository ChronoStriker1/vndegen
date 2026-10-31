import Testing
import VNCore
@testable import HubUI

private let speeds = GameMenuItem(id: "1", title: "Text speed", kind: .submenu([
    GameMenuItem(id: "1.0", title: "Fast", isRadio: true),
    .separator(id: "1.1"),
    GameMenuItem(id: "1.2", title: "Normal", isChecked: true, isRadio: true),
]))

@MainActor @Test func theGameMenuShowsOnlyWhenTheGameHasOne() {
    let menu = PlayerMenu()
    #expect(!menu.items(for: ControlMethod.automatic).contains(.gameMenu))
    menu.gameItems = [GameMenuItem(id: "0", title: "Quick save")]
    #expect(Array(menu.items(for: ControlMethod.automatic).prefix(3)) == [.resume, .gameMenu, .quit])
}

@MainActor @Test func theGameMenuOpensSubmenusAndGoesBack() {
    let menu = PlayerMenu()
    menu.gameItems = [GameMenuItem(id: "0", title: "Quick save"), speeds]
    menu.openGameMenu()
    #expect(menu.page == .game && menu.gamePath == [] && menu.gameFocus == "0")
    menu.moveGameFocus(by: 1)
    #expect(menu.focusedGameItem == speeds)
    menu.moveGameFocus(by: 1)
    #expect(menu.gameFocus == "1")

    menu.openSubmenu(speeds)
    #expect(menu.gameLevel.count == 3 && menu.gameFocus == "1.0")
    // The separator takes no focus.
    menu.moveGameFocus(by: 1)
    #expect(menu.gameFocus == "1.2")
    menu.moveGameFocus(by: -5)
    #expect(menu.gameFocus == "1.0")

    menu.gameBack()
    #expect(menu.page == .game && menu.gamePath == [] && menu.gameFocus == "1")
    menu.gameBack()
    #expect(menu.page == .main && menu.focus == .gameMenu)
}
