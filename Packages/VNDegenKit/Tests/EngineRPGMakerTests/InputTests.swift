import Foundation
import Testing
import VNCore
@testable import EngineRPGMaker

@Test func everyButtonExceptHubMapsToAnRPGMakerKey() {
    for button in VirtualButton.allCases {
        #expect((KeyStroke.stroke(for: button) == nil) == (button == .hub), "\(button)")
    }
    #expect(KeyStroke.stroke(for: .confirm)?.keyCode == 13)
    #expect(KeyStroke.stroke(for: .cancel)?.keyCode == 27)
    #expect(KeyStroke.stroke(for: .menu)?.keyCode == 88)
    #expect(KeyStroke.stroke(for: .dash)?.keyCode == 16)
    #expect(KeyStroke.stroke(for: .pageUp)?.keyCode == 81)
    #expect(KeyStroke.stroke(for: .pageDown)?.keyCode == 87)
    #expect(KeyStroke.stroke(for: .up)?.keyCode == 38)
    #expect(KeyStroke.stroke(for: .left)?.keyCode == 37)
}
