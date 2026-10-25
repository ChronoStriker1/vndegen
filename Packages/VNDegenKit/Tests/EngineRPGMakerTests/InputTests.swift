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

#if DEBUG
@Test func scriptsParseIntoSteps() throws {
    let steps = try InputScript.parse("""
    # boot
    wait 2.5; press confirm
    hold right 1
    tap 0.25 0.75
    scroll -30
    shot /tmp/a b.png
    js SceneManager._scene.constructor.name
    log hello there
    quit
    """)
    #expect(steps == [
        .wait(2.5), .press(.confirm), .hold(.right, 1), .tap(CGPoint(x: 0.25, y: 0.75)), .scroll(-30),
        .snapshot("/tmp/a b.png"), .js("SceneManager._scene.constructor.name"), .log("hello there"), .quit,
    ])
    #expect(throws: InputScriptError(line: "press fire")) { try InputScript.parse("press fire") }
}
#endif
