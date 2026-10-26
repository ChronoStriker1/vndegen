import Foundation
import Testing
import VNCore
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
    #expect(try InputScript.parse("until 30 TYRANO.kag.stat.is_strong_stop && $('.glink_button').length > 0")
            == [.until(30, "TYRANO.kag.stat.is_strong_stop && $('.glink_button').length > 0")])
    #expect(try InputScript.parse("repeat confirm 60 TYRANO.kag.stat.is_strong_stop") == [.repeatPress(.confirm, 60, "TYRANO.kag.stat.is_strong_stop")])
}
#endif

@Test func boundKeysDispatchAsRealKeyEvents() {
    let escape = KeyboardKey(key: "Escape", code: "Escape", keyCode: 27)
    #expect(WebGameSession.keyScript(escape, pressed: true) == #"__vndegen.key('keydown', "Escape", "Escape", 27)"#)
    #expect(WebGameSession.keyScript(KeyboardKey(key: "'", code: "Quote", keyCode: 222), pressed: false)
        == #"__vndegen.key('keyup', "'", "Quote", 222)"#)
}
