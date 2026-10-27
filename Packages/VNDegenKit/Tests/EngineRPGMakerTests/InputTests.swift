import Foundation
import Testing
import VNCore
@testable import EngineRPGMaker

@Test func everyButtonExceptHubMapsToAnRPGMakerKey() {
    for button in VirtualButton.allCases {
        #expect((KeyStroke.key(for: button) == nil) == (button == .hub), "\(button)")
    }
    #expect(KeyStroke.key(for: .confirm)?.keyCode == 13)
    #expect(KeyStroke.key(for: .cancel)?.keyCode == 27)
    #expect(KeyStroke.key(for: .menu)?.keyCode == 88)
    #expect(KeyStroke.key(for: .dash)?.keyCode == 16)
    #expect(KeyStroke.key(for: .pageUp)?.keyCode == 81)
    #expect(KeyStroke.key(for: .pageDown)?.keyCode == 87)
    #expect(KeyStroke.key(for: .up)?.keyCode == 38)
    #expect(KeyStroke.key(for: .left)?.keyCode == 37)
}

@Test func legacyRemapsPressTheKeysTheyUsedTo() {
    let plugin = RPGMakerPlugin(.mz)
    #expect(plugin.legacyAction(for: .confirm) == .key(KeyboardKey(key: "Enter", code: "Enter", keyCode: 13)))
    #expect(plugin.legacyAction(for: .menu) == .key(KeyboardKey(key: "x", code: "KeyX", keyCode: 88)))
    #expect(plugin.legacyAction(for: .hub) == nil)
}

@Test func rpgMakerGamesKeepTheGamepadLayout() {
    #expect(RPGMakerPlugin(.mz).defaultControls == .gamepad)
    #expect(RPGMakerPlugin(.mv).defaultControls == .gamepad)
}
