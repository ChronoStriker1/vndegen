import GameController
import Testing
import VNCore
@testable import GameInput

@Test func gamepadMappingMatchesTheDefaultLayout() {
    #expect(InputMapping.gamepad == [
        .a: .confirm, .b: .cancel, .x: .dash, .y: .menu,
        .leftShoulder: .pageUp, .rightShoulder: .pageDown,
        .menu: .menu, .options: .hub, .home: .hub,
    ])
    #expect(Set(InputMapping.gamepad.keys) == Set(GamepadElement.allCases))
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
    #expect(router.update(.dpad(pad), .up, pressed: true, at: t, hasConsumer: true, repeats: false) == .button(.up, pressed: true))
    #expect(router.update(.stick(pad), .up, pressed: true, at: t, hasConsumer: true, repeats: false) == nil)
    #expect(router.update(.dpad(pad), .up, pressed: false, at: t, hasConsumer: true, repeats: false) == nil)
    #expect(router.update(.stick(pad), .up, pressed: false, at: t, hasConsumer: true, repeats: false) == .button(.up, pressed: false))
}

@Test func pressWithoutConsumerIsDroppedWithItsRelease() {
    var router = PressRouter()
    let t = ContinuousClock.now
    #expect(router.update(.key(1), .confirm, pressed: true, at: t, hasConsumer: false, repeats: false) == nil)
    #expect(router.update(.key(1), .confirm, pressed: false, at: t, hasConsumer: true, repeats: false) == nil)
}

@Test func newConsumerNeverSeesTheOldConsumersHeldPress() {
    var router = PressRouter()
    let t = ContinuousClock.now
    _ = router.update(.gamepad(pad, .a), .confirm, pressed: true, at: t, hasConsumer: true, repeats: true)
    _ = router.update(.dpad(pad), .down, pressed: true, at: t, hasConsumer: true, repeats: true)
    #expect(router.detachConsumer() == [.button(.down, pressed: false), .button(.confirm, pressed: false)])
    #expect(router.tick(at: t + .seconds(1), hasConsumer: true, repeats: true) == nil)
    #expect(router.update(.gamepad(pad, .a), .confirm, pressed: false, at: t, hasConsumer: true, repeats: true) == nil)
    #expect(router.update(.gamepad(pad, .a), .confirm, pressed: true, at: t, hasConsumer: true, repeats: true) == .button(.confirm, pressed: true))
}

@Test func heldDirectionRepeatsAfterDelayThenAtInterval() {
    var router = PressRouter()
    let t = ContinuousClock.now
    let press = ControllerEvent.button(.right, pressed: true)
    #expect(router.update(.key(7), .right, pressed: true, at: t, hasConsumer: true, repeats: true) == press)
    #expect(router.tick(at: t + .milliseconds(399), hasConsumer: true, repeats: true) == nil)
    #expect(router.tick(at: t + .milliseconds(400), hasConsumer: true, repeats: true) == press)
    #expect(router.tick(at: t + .milliseconds(450), hasConsumer: true, repeats: true) == nil)
    #expect(router.tick(at: t + .milliseconds(500), hasConsumer: true, repeats: true) == press)
    #expect(router.tick(at: t + .milliseconds(600), hasConsumer: true, repeats: true) == press)
    #expect(router.update(.key(7), .right, pressed: false, at: t + .milliseconds(650), hasConsumer: true, repeats: true)
        == .button(.right, pressed: false))
    #expect(router.tick(at: t + .milliseconds(700), hasConsumer: true, repeats: true) == nil)
}

@Test func directionsDoNotRepeatWhenRepeatIsOff() {
    var router = PressRouter()
    let t = ContinuousClock.now
    _ = router.update(.key(7), .left, pressed: true, at: t, hasConsumer: true, repeats: false)
    #expect(router.tick(at: t + .seconds(2), hasConsumer: true, repeats: false) == nil)
}

@Test func buttonsOtherThanDirectionsNeverRepeat() {
    var router = PressRouter()
    let t = ContinuousClock.now
    _ = router.update(.gamepad(pad, .a), .confirm, pressed: true, at: t, hasConsumer: true, repeats: true)
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
