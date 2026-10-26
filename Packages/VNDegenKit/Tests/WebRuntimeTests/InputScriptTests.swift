import Foundation
import Testing
@testable import WebRuntime

#if DEBUG
@Test func scriptsParseIntoSteps() throws {
    let steps = try InputScript.parse("""
    # boot
    wait 2.5
    press confirm
    hold right 1
    tap 0.25 0.75
    scroll -30
    js SceneManager._scene.constructor.name
    fps 2.5
    log hello there
    quit
    """)
    #expect(steps == [
        .wait(2.5), .press(.confirm), .hold(.right, 1), .tap(CGPoint(x: 0.25, y: 0.75)), .scroll(-30),
        .js("SceneManager._scene.constructor.name"), .fps(2.5), .log("hello there"), .quit,
    ])
    #expect(try InputScript.parse("wait 1; press cancel", inline: true) == [.wait(1), .press(.cancel)])
    #expect(try InputScript.parse("js a(); b()") == [.js("a(); b()")])
    #expect(throws: InputScriptError(line: "press fire")) { try InputScript.parse("press fire") }
}
#endif
