import Foundation
import Testing
import VNCore
@testable import EngineKiriKiri

@Test func menuCaptionsLoseTheirAccessKeysAndShortcuts() {
    #expect(KiriKiriMenu.caption("System(&S)") == "System")
    #expect(KiriKiriMenu.caption("Quick Save(&Q)") == "Quick Save")
    #expect(KiriKiriMenu.caption("システム(&S)") == "システム")
    #expect(KiriKiriMenu.caption("Text Processing (&C)") == "Text Processing")
    #expect(KiriKiriMenu.caption("Console(&C)\tShift+F4") == "Console")
    #expect(KiriKiriMenu.caption("&Save") == "Save")
    #expect(KiriKiriMenu.caption("Load && Save") == "Load & Save")
    #expect(KiriKiriMenu.caption("Font(&D) ...") == "Font ...")
    #expect(KiriKiriMenu.caption("24/10/31 12:00 Chapter 2") == "24/10/31 12:00 Chapter 2")
}

/// Tawawa's menu bar as `vnkk_menu` reports it, with the items its Config.tjs hides already left out.
private let tawawaMenu = """
[{"id":"0","title":"System(&S)","checked":false,"radio":false,"enabled":true,"items":[
  {"id":"0.5","title":"-","checked":false,"radio":false,"enabled":true,"items":[]},
  {"id":"0.8","title":"-","checked":false,"radio":false,"enabled":true,"items":[]},
  {"id":"0.10","title":"Config(&C)","checked":false,"radio":false,"enabled":true,"items":[]},
  {"id":"0.11","title":"Save(&S)","checked":false,"radio":false,"enabled":false,"items":[]},
  {"id":"0.13","title":"Return to Title(&R)","checked":false,"radio":false,"enabled":true,"items":[]}]},
 {"id":"1","title":"Text(&C)","checked":false,"radio":false,"enabled":true,"items":[
  {"id":"1.0","title":"-","checked":false,"radio":false,"enabled":true,"items":[]}]},
 {"id":"7","title":"Quick Save(&Q)","checked":false,"radio":false,"enabled":true,"items":[]},
 {"id":"12","title":"Auto(&A)","checked":true,"radio":false,"enabled":true,"items":[]},
 {"id":"13","title":"Speed","checked":false,"radio":false,"enabled":false,"items":[
  {"id":"13.0","title":"Fast(&F)","checked":false,"radio":true,"enabled":false,"items":[]},
  {"id":"13.1","title":"Normal(&N)","checked":true,"radio":true,"enabled":false,"items":[]}]}]
"""

@Test func theMenuBarBecomesGameMenuItems() {
    let items = KiriKiriMenu.items(json: tawawaMenu)
    #expect(items.map(\.title) == ["System", "Quick Save", "Auto", "Speed"])
    // Separators that would lead the list are dropped, and a submenu of separators alone goes.
    #expect(items[0].items.map(\.title) == ["Config", "Save", "Return to Title"])
    #expect(items[0].items.map(\.id) == ["0.10", "0.11", "0.13"])
    #expect(items[0].items[1].isEnabled == false)
    #expect(items[2].isChecked)
    #expect(items[2].kind == .command)
    #expect(items[3].items[1].isChecked && items[3].items[1].isRadio)
    #expect(items[3].isEnabled == false)
}

@Test func separatorsOnlyFallBetweenItems() {
    let json = """
    [{"id":"0","title":"A","checked":false,"radio":false,"enabled":true,"items":[]},
     {"id":"1","title":"-","checked":false,"radio":false,"enabled":true,"items":[]},
     {"id":"2","title":"-","checked":false,"radio":false,"enabled":true,"items":[]},
     {"id":"3","title":"B","checked":false,"radio":false,"enabled":true,"items":[]},
     {"id":"4","title":"-","checked":false,"radio":false,"enabled":true,"items":[]}]
    """
    #expect(KiriKiriMenu.items(json: json).map(\.kind) == [.command, .separator, .command])
    #expect(KiriKiriMenu.items(json: "not json").isEmpty)
}

@Test func theMenuBarChoiceIsKeptPerGame() throws {
    let data = FileManager.default.temporaryDirectory.appendingPathComponent("kirikiri-menu-\(UUID().uuidString)")
    defer { try? FileManager.default.removeItem(at: data) }
    let plugin = KiriKiriPlugin()
    let game = Game(id: GameID("t"))
    #expect(plugin.variants(for: game, dataDirectory: data)?.selected == nil)
    #expect(plugin.variants(for: game, dataDirectory: data)?.options.map(\.id) == ["hidden", "shown"])

    try plugin.selectVariant("shown", for: game, dataDirectory: data)
    #expect(KiriKiriMenuBarSettings.load(data).choice == .shown)
    #expect(plugin.variants(for: game, dataDirectory: data)?.selected == "shown")
    #expect(KiriKiriMenuBar.engineMode(KiriKiriMenuBarSettings.load(data).choice) == 2)

    // What automatic picked stays when the player goes back to automatic.
    var settings = KiriKiriMenuBarSettings.load(data)
    settings.automatic = .hidden
    try settings.save(data)
    try plugin.selectVariant(nil, for: game, dataDirectory: data)
    #expect(KiriKiriMenuBarSettings.load(data) == KiriKiriMenuBarSettings(choice: nil, automatic: .hidden))
    #expect(plugin.variants(for: game, dataDirectory: data)?.automatic == "commands on screen, as the game's scripts expect")
    #expect(KiriKiriMenuBar.engineMode(nil) == 0)
}
