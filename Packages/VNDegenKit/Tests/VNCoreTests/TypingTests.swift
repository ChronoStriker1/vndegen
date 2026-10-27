import Foundation
import Testing
@testable import VNCore

@MainActor private final class KeySession: EngineSession {
    var events: [ControllerEvent] = []
    var view: PlatformView { fatalError("not used") }
    func send(_ event: ControllerEvent) { events.append(event) }
    func setPaused(_ paused: Bool) {}
    func terminate() {}
}

@MainActor @Test func typingPressesKeysInEnginesThatReadNoText() {
    let session = KeySession()
    session.type(.text("a1 é"))
    let a = KeyboardKey(key: "a", code: "KeyA", keyCode: 65)
    let one = KeyboardKey(key: "1", code: "Digit1", keyCode: 49)
    let space = KeyboardKey(key: " ", code: "Space", keyCode: 32)
    #expect(session.events == [a, one, space].flatMap { [.key($0, pressed: true), .key($0, pressed: false)] })
    session.events = []
    session.type(.newline)
    session.type(.deleteBackward)
    #expect(session.events.map { if case let .key(key, _) = $0 { key.code } else { "" } } == ["Enter", "Enter", "Backspace", "Backspace"])
}
