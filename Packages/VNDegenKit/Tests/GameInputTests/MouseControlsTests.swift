import CoreGraphics
import Foundation
import Testing
import VNCore
@testable import GameInput

@Test func keyboardAndMouseIsSteamsTemplate() {
    let key = { (key: String, code: String, keyCode: Int) in ControlAction.key(KeyboardKey(key: key, code: code, keyCode: keyCode)) }
    let expected: [ControlInput: ControlAction] = [
        .l: .wheel(.down), .r: .wheel(.up), .l2: .click(.secondary), .r2: .click(.primary),
        .view: key("Tab", "Tab", 9), .menu: key("Escape", "Escape", 27),
        .leftStickUp: key("w", "KeyW", 87), .leftStickDown: key("s", "KeyS", 83),
        .leftStickLeft: key("a", "KeyA", 65), .leftStickRight: key("d", "KeyD", 68),
        .l3: key("Shift", "ShiftLeft", 16), .r3: .click(.primary),
        .rightStickUp: .mouseMove(.up), .rightStickDown: .mouseMove(.down),
        .rightStickLeft: .mouseMove(.left), .rightStickRight: .mouseMove(.right),
        .up: key("1", "Digit1", 49), .down: key("2", "Digit2", 50), .left: key("3", "Digit3", 51), .right: key("4", "Digit4", 52),
        .a: key(" ", "Space", 32), .b: key("e", "KeyE", 69), .x: key("r", "KeyR", 82), .y: key("f", "KeyF", 70),
    ]
    #expect(ButtonMapping.keyboardAndMouse.overrides == expected)
    #expect(ButtonMapping.keyboardAndMouse.movesPointer(.right))
    #expect(!ButtonMapping.keyboardAndMouse.movesPointer(.left))
    // Every key the template sends is one the editor offers.
    for case let .key(sent) in expected.values { #expect(KeyboardKey.choices.contains(sent)) }
    #expect(GamepadElement.home.action(in: .keyboardAndMouse) == .button(.hub))
    #expect(ControlLayout.keyboardAndMouse.mapping == .keyboardAndMouse)
}

@Test func mouseLayoutIsTheOriginallyRequestedOne() {
    let mapping = ButtonMapping.mouse
    let expected: [ControlInput: ControlAction] = [
        .l: .wheel(.down), .r: .wheel(.up), .l2: .click(.secondary), .r2: .click(.primary), .menu: .key(.escape),
        .a: .click(.primary), .b: .key(.escape), .x: .click(.secondary), .y: .showKeyboard,
        .leftStickUp: .mouseMove(.up), .leftStickDown: .mouseMove(.down), .leftStickLeft: .mouseMove(.left),
        .leftStickRight: .mouseMove(.right), .rightStickUp: .mouseMove(.up), .rightStickDown: .mouseMove(.down),
        .rightStickLeft: .mouseMove(.left), .rightStickRight: .mouseMove(.right),
    ]
    #expect(mapping.overrides == expected)
    // The D-pad, View, and the stick buttons stay gamepad buttons, and Home is never mapped.
    for input in [ControlInput.up, .down, .left, .right, .view, .l3, .r3] {
        #expect(mapping.action(for: input) == .gamepad(input))
    }
    #expect(GamepadElement.home.action(in: mapping) == .button(.hub))
    #expect(Stick.allCases.allSatisfy(mapping.movesPointer))
    #expect(ControlLayout.gamepad.mapping == ButtonMapping())
}

@Test func storedMappingsFollowTheEngineDefault() {
    // Nil follows the engine's default, and an empty mapping is the plain Xbox layout.
    #expect(ButtonMapping(stored: nil, default: .keyboardAndMouse) == .keyboardAndMouse)
    #expect(ButtonMapping(stored: [:], default: .keyboardAndMouse) == ButtonMapping())
    #expect(ButtonMapping(stored: nil, default: ButtonMapping()) == ButtonMapping())
    #expect(ButtonMapping.keyboardAndMouse.stored(default: .keyboardAndMouse) == nil)
    #expect(ButtonMapping().stored(default: .keyboardAndMouse) == [:])
    #expect(ButtonMapping().stored(default: ButtonMapping()) == nil)
    #expect(ButtonMapping.rpgMaker.stored(default: ButtonMapping()) == ButtonMapping.rpgMaker.overrides)

    // Remapping one button of the default keeps the rest of the default.
    var mapping = ButtonMapping(stored: nil, default: .keyboardAndMouse)
    mapping.set(.key(.enter), for: .a)
    let stored = mapping.stored(default: .keyboardAndMouse)
    #expect(stored?[.a] == .key(.enter))
    #expect(stored?[.b] == .key(.letter("E")))
    #expect(mapping.changes(from: .keyboardAndMouse) == 1)
    #expect(ButtonMapping.keyboardAndMouse.changes(from: ButtonMapping()) == ButtonMapping.keyboardAndMouse.overrides.count)
}

@Test func mappingsFromEarlierVersionsDecodeUnchanged() throws {
    // `Game.buttonMapping` as 0.6.0 wrote it.
    let json = #"""
    {"a":{"key":{"_0":{"key":"Enter","code":"Enter","keyCode":13}}},"b":{"click":{"_0":"secondary"}},"x":{"nothing":{}},
     "y":{"gamepad":{"_0":"menu"}},"l":{"button":{"_0":"pageUp"}}}
    """#
    let decoded = try JSONDecoder().decode([ControlInput: ControlAction].self, from: Data(json.utf8))
    #expect(decoded == [.a: .key(.enter), .b: .click(.secondary), .x: .nothing, .y: .gamepad(.menu), .l: .button(.pageUp)])
    // A stored mapping is against the Xbox layout whatever the engine's default, so a KiriKiri game remapped before
    // keeps every other button as it was.
    let mapping = ButtonMapping(stored: decoded, default: .keyboardAndMouse)
    #expect(mapping.action(for: .r2) == .gamepad(.r2))
    #expect(mapping.action(for: .leftStickUp) == .gamepad(.leftStickUp))
}

@Test func newActionsRoundTrip() throws {
    let stored: [ControlInput: ControlAction] = [
        .a: .mouseMove(.left), .b: .wheel(.up), .y: .showKeyboard, .leftStickDown: .mouseMove(.down), .x: .wheel(.down),
    ]
    let data = try JSONEncoder().encode(stored)
    #expect(try JSONDecoder().decode([ControlInput: ControlAction].self, from: data) == stored)
}

@Test func clickTitlesNameTheMouseButton() {
    #expect(ControlAction.click(.primary).title == "Left click")
    #expect(ControlAction.click(.secondary).title == "Right click")
    #expect(ControlAction.click(.primary).badge == "Left click")
    #expect(ControlAction.click(.secondary).badge == "Right click")
    #expect(ControlAction.mouseChoices.contains(.mouseMove(.up)))
    #expect(ControlAction.mouseChoices.contains(.wheel(.down)))
}

@Test func aWholeStickSwitchesBetweenPointerAndStick() {
    var mapping = ButtonMapping()
    #expect(!mapping.movesPointer(.left))
    mapping.setStick(.left, movesPointer: true)
    #expect(mapping.movesPointer(.left))
    #expect(!mapping.movesPointer(.right))
    #expect(mapping.action(for: .leftStickRight) == .mouseMove(.right))
    mapping.set(.key(.enter), for: .leftStickUp)
    #expect(!mapping.movesPointer(.left))
    mapping.setStick(.left, movesPointer: false)
    #expect(mapping == ButtonMapping())
}

@Test func mouseActionsSendNoPressOfTheirOwn() {
    for action in [ControlAction.mouseMove(.up), .wheel(.up), .showKeyboard] {
        #expect(action.event(pressed: true) == nil)
        #expect(action.isVirtualMouse)
    }
    #expect(!ControlAction.click(.primary).isVirtualMouse)
}

@Test func stickPushHasADeadZoneAndGrowsWithTheStick() {
    let mapping = ButtonMapping.mouse
    let push = { (x: Double, y: Double) in PointerMotion.push(x: x, y: y, stick: .left) { mapping.action(for: $0) } }
    #expect(push(0.1, 0.05) == .zero)
    let full = push(1, 0)
    #expect(abs(full.dx - 1) < 1e-9 && full.dy == 0)
    // Stick up is screen up, which is negative y.
    let up = push(0, 1)
    #expect(up.dx == 0 && abs(up.dy + 1) < 1e-9)
    let half = push(0.5, 0)
    #expect(half.dx > 0 && half.dx < 0.5)
    // A stick left as a gamepad stick moves nothing.
    #expect(PointerMotion.push(x: 1, y: 0, stick: .left) { ButtonMapping().action(for: $0) } == .zero)
    // Directions mapped another way move the pointer that way.
    var swapped = ButtonMapping()
    swapped.set(.mouseMove(.left), for: .rightStickRight)
    let reversed = PointerMotion.push(x: 1, y: 0, stick: .right) { swapped.action(for: $0) }
    #expect(abs(reversed.dx + 1) < 1e-9)
}

@Test func pointerMovesAtTheSameSpeedAcrossAndDownAndStaysOnScreen() {
    var motion = PointerMotion()
    motion.held[.right] = 1
    motion.held[.down] = 1
    let size = CGSize(width: 2000, height: 1000)
    let moved = motion.step(CGPoint(x: 0.5, y: 0.5), seconds: 0.1, size: size)
    // The same number of points each way.
    #expect(abs((moved.x - 0.5) * size.width - (moved.y - 0.5) * size.height) < 1e-6)
    #expect(abs((moved.x - 0.5) * size.width - PointerMotion.buttonSpeed * 0.1 * 2000) < 1e-6)
    let edge = motion.step(CGPoint(x: 0.99, y: 0.99), seconds: 1, size: size)
    #expect(edge == CGPoint(x: 1, y: 1))
}

@MainActor @Test func virtualMouseTurnsTheWheelAndTogglesTheKeyboard() async {
    let mouse = VirtualMouse()
    var events: [ControllerEvent] = []
    mouse.send = { events.append($0) }
    let source = InputSource.touch(.l)
    #expect(!mouse.press(.touch(.a), .click(.primary), hasConsumer: true))
    #expect(mouse.press(source, .wheel(.down), hasConsumer: true))
    #expect(events == [.scroll(dy: VirtualMouse.notch)])
    #expect(mouse.release(source))
    #expect(!mouse.release(source))
    #expect(mouse.press(.touch(.r), .wheel(.up), hasConsumer: true))
    #expect(events.last == .scroll(dy: -VirtualMouse.notch))
    _ = mouse.release(.touch(.r))

    #expect(mouse.press(.touch(.y), .showKeyboard, hasConsumer: true))
    #expect(mouse.showsKeyboard)
    _ = mouse.release(.touch(.y))
    _ = mouse.press(.touch(.y), .showKeyboard, hasConsumer: true)
    #expect(!mouse.showsKeyboard)
    _ = mouse.release(.touch(.y))

    // Without a consumer the press is taken but does nothing.
    let count = events.count
    #expect(mouse.press(source, .wheel(.down), hasConsumer: false))
    #expect(events.count == count)
}

@MainActor @Test func heldMouseButtonsMoveThePointer() async throws {
    let mouse = VirtualMouse()
    var moves: [CGPoint] = []
    mouse.send = { if case let .pointer(pointer) = $0 { moves.append(pointer.location) } }
    mouse.viewSize = CGSize(width: 1000, height: 1000)
    mouse.start()
    #expect(!mouse.isShown)
    #expect(mouse.press(.touch(.right), .mouseMove(.right), hasConsumer: true))
    try await Task.sleep(for: .milliseconds(200))
    _ = mouse.release(.touch(.right))
    try await Task.sleep(for: .milliseconds(50))
    let stopped = mouse.location
    try await Task.sleep(for: .milliseconds(100))
    #expect(mouse.isShown)
    #expect(stopped.x > 0.5 && stopped.y == 0.5)
    #expect(mouse.location == stopped)
    #expect(moves.last == stopped)

    // A stick moves it too, and a consumer change stops it.
    let pad = ObjectIdentifier(NSObject())
    mouse.moveStick(pad, .left, x: 0, y: -1, mapping: .mouse, hasConsumer: true)
    try await Task.sleep(for: .milliseconds(100))
    #expect(mouse.location.y > 0.5)
    mouse.releaseAll()
    let held = mouse.location
    try await Task.sleep(for: .milliseconds(100))
    #expect(mouse.location == held)
}

@MainActor @Test func aWheelKeepsTurningWhileAnyButtonHoldsIt() async throws {
    let mouse = VirtualMouse()
    var notches = 0
    mouse.send = { if case .scroll = $0 { notches += 1 } }
    _ = mouse.press(.touch(.l), .wheel(.down), hasConsumer: true)
    _ = mouse.press(.key(1), .wheel(.down), hasConsumer: true)
    _ = mouse.release(.key(1))
    try await Task.sleep(for: .milliseconds(700))
    // Two presses, then repeats from the button still held.
    #expect(notches > 2)
    _ = mouse.release(.touch(.l))
    let stopped = notches
    try await Task.sleep(for: .milliseconds(300))
    #expect(notches == stopped)
}
