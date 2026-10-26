import Foundation
import GameController
import Testing
import VNCore
@testable import GameInput

@Test func gamepadElementsFollowTheStandardLayout() {
    let standard = ButtonMapping()
    let expected: [GamepadElement: ControlAction] = [
        .a: .button(.confirm), .b: .button(.cancel), .x: .button(.dash), .y: .button(.menu),
        .leftShoulder: .button(.pageUp), .rightShoulder: .button(.pageDown),
        .leftTrigger: .nothing, .rightTrigger: .nothing,
        .menu: .button(.menu), .options: .button(.hub), .home: .button(.hub),
    ]
    for element in GamepadElement.allCases {
        #expect(element.action(in: standard) == expected[element])
    }
    for direction in [ControlInput.up, .down, .left, .right] {
        #expect(standard.action(for: direction) == .button(VirtualButton(rawValue: direction.rawValue)!))
    }
}

@Test func remappedButtonsSendTheirNewActions() {
    let z = KeyboardKey(key: "z", code: "KeyZ", keyCode: 90)
    let escape = KeyboardKey(key: "Escape", code: "Escape", keyCode: 27)
    var mapping = ButtonMapping()
    mapping.set(.key(escape), for: .y)
    mapping.set(.key(z), for: .a)
    mapping.set(.click(.secondary), for: .r2)
    mapping.set(.nothing, for: .x)
    #expect(GamepadElement.y.action(in: mapping).event(pressed: true) == .key(escape, pressed: true))
    #expect(GamepadElement.a.action(in: mapping).event(pressed: false) == .key(z, pressed: false))
    #expect(GamepadElement.rightTrigger.action(in: mapping).event(pressed: true) == .click(.secondary, pressed: true))
    #expect(GamepadElement.x.action(in: mapping).event(pressed: true) == nil)
    #expect(GamepadElement.b.action(in: mapping) == .button(.cancel))
}

@Test func optionsAndHomeAlwaysOpenTheVNDegenMenu() {
    var mapping = ButtonMapping()
    for input in ControlInput.allCases { mapping.set(.nothing, for: input) }
    #expect(GamepadElement.options.action(in: mapping) == .button(.hub))
    #expect(GamepadElement.home.action(in: mapping) == .button(.hub))
    #expect(GamepadElement.allCases.filter { $0.control == nil } == [.options, .home])
    #expect(!ControlAction.gamepadChoices.contains(.button(.hub)))
}

@Test func mappingStoresOnlyDifferencesFromTheStandardLayout() {
    var mapping = ButtonMapping()
    #expect(mapping.stored == nil)
    mapping.set(.button(.cancel), for: .a)
    #expect(mapping.stored == [.a: .button(.cancel)])
    mapping.set(.button(.confirm), for: .a)
    #expect(mapping.stored == nil)
    #expect(!mapping.isRemapped(.a))
}

@Test func keyChoicesAreUniqueAndLabelled() {
    let keys = KeyboardKey.choices
    #expect(keys.count == 26 + 10 + 16 + 12)
    #expect(Set(keys.map(\.code)).count == keys.count)
    #expect(Set(keys.map(\.keyCode)).count == keys.count)
    #expect(keys.first == KeyboardKey(key: "a", code: "KeyA", keyCode: 65))
    #expect(keys.first { $0.code == "KeyZ" }?.label == "Z")
    #expect(keys.first { $0.code == "Space" }?.label == "Space")
    #expect(keys.first { $0.code == "F12" }?.keyCode == 123)
    #expect(ControlAction.key(keys[25]).badge == "Z")
}

@Test func controlMethodPicksWhatTouchesDo() {
    #expect(ControlMethod.automatic.touchControls(lastInput: .touch, controllerConnected: false) == .gamepad)
    #expect(ControlMethod.automatic.touchControls(lastInput: .touch, controllerConnected: true) == .direct)
    #expect(ControlMethod.automatic.touchControls(lastInput: .gamepad, controllerConnected: false) == .direct)
    #expect(ControlMethod.gamepad.touchControls(lastInput: .gamepad, controllerConnected: true) == .gamepad)
    #expect(ControlMethod.mouse(.touch).touchControls(lastInput: .keyboardMouse, controllerConnected: false) == .direct)
    #expect(ControlMethod.mouse(.cursor).touchControls(lastInput: .keyboardMouse, controllerConnected: false) == .cursor)
    #expect(ControlMethod.controller(mouse: nil).touchControls(lastInput: .touch, controllerConnected: false) == .off)
    #expect(ControlMethod.controller(mouse: .touch).touchControls(lastInput: .gamepad, controllerConnected: true) == .direct)
    #expect(ControlMethod.controller(mouse: .cursor).touchControls(lastInput: .gamepad, controllerConnected: true) == .cursor)
}

@Test func keyboardMappingCoversNavigationKeys() {
    let expected: [(GCKeyCode, VirtualButton)] = [
        (.upArrow, .up), (.keyW, .up), (.downArrow, .down), (.keyS, .down),
        (.leftArrow, .left), (.keyA, .left), (.rightArrow, .right), (.keyD, .right),
        (.returnOrEnter, .confirm), (.spacebar, .confirm),
        (.escape, .cancel), (.deleteOrBackspace, .cancel),
        (.tab, .menu), (.keyQ, .pageUp), (.keyE, .pageDown),
        (InputMapping.hubKey, .hub),
    ]
    for (key, button) in expected {
        #expect(InputMapping.keyboard[key] == button)
    }
}

private let pad = ObjectIdentifier(NSObject())

@Test func twoSourcesForOneButtonMakeOnePress() {
    var router = PressRouter()
    let t = ContinuousClock.now
    #expect(router.press(.dpad(pad, .up), .button(.up), at: t, hasConsumer: true, repeats: false) == .button(.up, pressed: true))
    #expect(router.press(.stick(pad, .up), .button(.up), at: t, hasConsumer: true, repeats: false) == nil)
    #expect(router.release(.dpad(pad, .up)) == nil)
    #expect(router.release(.stick(pad, .up)) == .button(.up, pressed: false))
}

@Test func dpadDiagonalHoldsTwoDirections() {
    var router = PressRouter()
    let t = ContinuousClock.now
    #expect(router.press(.dpad(pad, .up), .button(.up), at: t, hasConsumer: true, repeats: false) == .button(.up, pressed: true))
    #expect(router.press(.dpad(pad, .right), .button(.right), at: t, hasConsumer: true, repeats: false) == .button(.right, pressed: true))
    #expect(router.release(.dpad(pad, .up)) == .button(.up, pressed: false))
    #expect(router.release(.dpad(pad, .right)) == .button(.right, pressed: false))
}

@Test func releaseSendsWhatThePressSentAfterARemap() {
    var router = PressRouter()
    let t = ContinuousClock.now
    let escape = KeyboardKey(key: "Escape", code: "Escape", keyCode: 27)
    #expect(router.press(.gamepad(pad, .y), .key(escape), at: t, hasConsumer: true, repeats: false) == .key(escape, pressed: true))
    #expect(router.release(.gamepad(pad, .y)) == .key(escape, pressed: false))
}

@Test func buttonBoundToNothingSendsNothing() {
    var router = PressRouter()
    let t = ContinuousClock.now
    #expect(router.press(.gamepad(pad, .x), .nothing, at: t, hasConsumer: true, repeats: true) == nil)
    #expect(router.release(.gamepad(pad, .x)) == nil)
}

@Test func pressWithoutConsumerIsDroppedWithItsRelease() {
    var router = PressRouter()
    let t = ContinuousClock.now
    #expect(router.press(.key(1), .button(.confirm), at: t, hasConsumer: false, repeats: false) == nil)
    #expect(router.release(.key(1)) == nil)
}

@Test func newConsumerNeverSeesTheOldConsumersHeldPress() {
    var router = PressRouter()
    let t = ContinuousClock.now
    _ = router.press(.gamepad(pad, .a), .button(.confirm), at: t, hasConsumer: true, repeats: true)
    _ = router.press(.dpad(pad, .down), .button(.down), at: t, hasConsumer: true, repeats: true)
    #expect(router.detachConsumer() == [.button(.down, pressed: false), .button(.confirm, pressed: false)])
    #expect(router.tick(at: t + .seconds(1), hasConsumer: true, repeats: true) == nil)
    #expect(router.release(.gamepad(pad, .a)) == nil)
    #expect(router.press(.gamepad(pad, .a), .button(.confirm), at: t, hasConsumer: true, repeats: true) == .button(.confirm, pressed: true))
}

@Test func heldDirectionRepeatsAfterDelayThenAtInterval() {
    var router = PressRouter()
    let t = ContinuousClock.now
    let press = ControllerEvent.button(.right, pressed: true)
    #expect(router.press(.key(7), .button(.right), at: t, hasConsumer: true, repeats: true) == press)
    #expect(router.tick(at: t + .milliseconds(399), hasConsumer: true, repeats: true) == nil)
    #expect(router.tick(at: t + .milliseconds(400), hasConsumer: true, repeats: true) == press)
    #expect(router.tick(at: t + .milliseconds(450), hasConsumer: true, repeats: true) == nil)
    #expect(router.tick(at: t + .milliseconds(500), hasConsumer: true, repeats: true) == press)
    #expect(router.tick(at: t + .milliseconds(600), hasConsumer: true, repeats: true) == press)
    #expect(router.release(.key(7)) == .button(.right, pressed: false))
    #expect(router.tick(at: t + .milliseconds(700), hasConsumer: true, repeats: true) == nil)
}

@Test func directionsDoNotRepeatWhenRepeatIsOff() {
    var router = PressRouter()
    let t = ContinuousClock.now
    _ = router.press(.key(7), .button(.left), at: t, hasConsumer: true, repeats: false)
    #expect(router.tick(at: t + .seconds(2), hasConsumer: true, repeats: false) == nil)
}

@Test func buttonsOtherThanDirectionsNeverRepeat() {
    var router = PressRouter()
    let t = ContinuousClock.now
    _ = router.press(.gamepad(pad, .a), .button(.confirm), at: t, hasConsumer: true, repeats: true)
    #expect(router.tick(at: t + .seconds(2), hasConsumer: true, repeats: true) == nil)
}

@Test func stickIgnoresDeadZoneAndWobble() {
    var stick = StickDirections()
    #expect(stick.update(x: 0.2, y: -0.25).isEmpty)
    #expect(stick.update(x: 0.6, y: 0).map(\.0) == [.right])
    // Wobbling between the release and press thresholds keeps the one press.
    #expect(stick.update(x: 0.4, y: 0).isEmpty)
    #expect(stick.update(x: 0.55, y: 0).isEmpty)
    let released = stick.update(x: 0.1, y: 0)
    #expect(released.count == 1 && released[0].0 == .right && !released[0].pressed)
}

@Test func stickDiagonalPressesBothAndFlickReleasesFirst() {
    var stick = StickDirections()
    #expect(Set(stick.update(x: 0.7, y: 0.7).map(\.0)) == [.right, .up])
    let flick = stick.update(x: -0.9, y: 0)
    #expect(flick.map(\.0) == [.right, .up, .left])
    #expect(flick.map(\.pressed) == [false, false, true])
}

@Test func dpadPressesOneDirectionOnAnArmAndTwoOnADiagonal() {
    #expect(dpadDirections(dx: 2, dy: 1, deadZone: 5).isEmpty)
    #expect(dpadDirections(dx: 0, dy: -40, deadZone: 5) == [.up])
    #expect(dpadDirections(dx: 40, dy: 8, deadZone: 5) == [.right])
    #expect(dpadDirections(dx: 30, dy: 30, deadZone: 5) == [.right, .down])
    #expect(dpadDirections(dx: -30, dy: -28, deadZone: 5) == [.left, .up])
}

@Test func heldSourceStaysSilentAfterAConsumerChangeWithoutBlockingOthers() {
    var router = PressRouter()
    let t = ContinuousClock.now
    _ = router.press(.gamepad(pad, .a), .button(.confirm), at: t, hasConsumer: true, repeats: false)
    #expect(router.detachConsumer() == [.button(.confirm, pressed: false)])
    // A still held. Return also confirms, and the new consumer must see its press and its release.
    #expect(router.press(.key(40), .button(.confirm), at: t, hasConsumer: true, repeats: false) == .button(.confirm, pressed: true))
    #expect(router.release(.gamepad(pad, .a)) == nil)
    #expect(router.release(.key(40)) == .button(.confirm, pressed: false))
}

@Test func mutedSourcePressedAgainIsANewPress() {
    var router = PressRouter()
    let t = ContinuousClock.now
    _ = router.press(.touch(.a), .button(.confirm), at: t, hasConsumer: true, repeats: false)
    _ = router.detachConsumer()
    // The touch button went away mid-press and never released.
    #expect(router.press(.touch(.a), .button(.confirm), at: t, hasConsumer: true, repeats: false) == .button(.confirm, pressed: true))
    #expect(router.release(.touch(.a)) == .button(.confirm, pressed: false))
}

private let control = KeyboardKey(key: "Control", code: "ControlLeft", keyCode: 17)
private let enter = KeyboardKey(key: "Enter", code: "Enter", keyCode: 13)

/// Records what a consumer receives.
@MainActor
private final class Consumer {
    var events: [ControllerEvent] = []
    var receive: (ControllerEvent) -> Void { { [unowned self] in events.append($0) } }
}

@Test @MainActor func switchingConsumersReleasesWhatTheGameHolds() {
    let hub = InputHub()
    let game = Consumer(), menu = Consumer()
    hub.onEvent = game.receive
    var mapping = ButtonMapping()
    mapping.set(.key(control), for: .a)
    hub.buttonMapping = mapping
    hub.touch(.a, pressed: true)
    hub.onEvent = menu.receive
    #expect(game.events == [.key(control, pressed: true), .key(control, pressed: false)])
    hub.touch(.a, pressed: false)
    hub.onEvent = game.receive
    #expect(menu.events.isEmpty)
    #expect(game.events.count == 2)
}

@Test @MainActor func twoTouchButtonsForOneKeyHoldItUntilBothLift() {
    let hub = InputHub()
    let game = Consumer()
    hub.onEvent = game.receive
    var mapping = ButtonMapping()
    mapping.set(.key(enter), for: .a)
    mapping.set(.key(enter), for: .b)
    hub.buttonMapping = mapping
    hub.touch(.a, pressed: true)
    hub.touch(.b, pressed: true)
    hub.touch(.a, pressed: false)
    #expect(game.events == [.key(enter, pressed: true)])
    hub.touch(.b, pressed: false)
    #expect(game.events == [.key(enter, pressed: true), .key(enter, pressed: false)])
}

@Test @MainActor func touchHomeOpensTheMenuWhateverTheMapping() {
    let hub = InputHub()
    let game = Consumer()
    hub.onEvent = game.receive
    var mapping = ButtonMapping()
    for input in ControlInput.allCases { mapping.set(.nothing, for: input) }
    hub.buttonMapping = mapping
    hub.touch(nil, pressed: true)
    hub.touch(nil, pressed: false)
    #expect(game.events == [.button(.hub, pressed: true), .button(.hub, pressed: false)])
}

@Test func touchControlsSettingMigratesToTheDefaultControlMethod() throws {
    let suite = "vndegen-migration-\(UUID().uuidString)"
    let defaults = try #require(UserDefaults(suiteName: suite))
    defer { defaults.removePersistentDomain(forName: suite) }
    for (legacy, method) in [("mouse", ControlMethod.mouse(.cursor)), ("hidden", .controller(mouse: .touch)), ("gamepad", .gamepad)] {
        defaults.removeObject(forKey: ControlMethod.storageKey)
        defaults.set(legacy, forKey: "touchControlsMode")
        ControlMethod.migrateTouchControlsSetting(in: defaults)
        #expect(defaults.string(forKey: ControlMethod.storageKey) == method.rawValue)
        #expect(defaults.object(forKey: "touchControlsMode") == nil)
    }
    // A method the player already chose wins, and the old setting still goes.
    defaults.set(ControlMethod.automatic.rawValue, forKey: ControlMethod.storageKey)
    defaults.set("mouse", forKey: "touchControlsMode")
    ControlMethod.migrateTouchControlsSetting(in: defaults)
    #expect(defaults.string(forKey: ControlMethod.storageKey) == ControlMethod.automatic.rawValue)
    #expect(defaults.object(forKey: "touchControlsMode") == nil)
}
