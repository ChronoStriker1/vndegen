import CoreGraphics
import Testing
import VNCore
@testable import EngineAliceSoft

@Suite struct System4InputRouterTests {
    private let at = CGPoint(x: 0.5, y: 0.5)
    private func event(_ phase: PointerEvent.Phase, _ button: PointerEvent.Button = .primary) -> PointerEvent {
        PointerEvent(phase: phase, location: at, button: button)
    }

    @Test func clickPressesAndReleasesTheLeftButton() {
        var router = System4InputRouter()
        #expect(router.pointer(event(.began), at: at) == [.mouseMove(at), .key(1, down: true)])
        #expect(router.pointer(event(.ended), at: at) == [.mouseMove(at), .key(1, down: false)])
    }

    @Test func pressAfterALostReleaseIsANewClick() {
        // AppKit can drop a mouse-up; the next press must still reach the game as a press.
        var router = System4InputRouter()
        _ = router.pointer(event(.began), at: at)
        #expect(router.pointer(event(.began), at: at) == [.mouseMove(at), .key(1, down: false), .key(1, down: true)])
        #expect(router.pointer(event(.ended), at: at) == [.mouseMove(at), .key(1, down: false)])
    }

    @Test func releaseWithoutAPressIsIgnored() {
        var router = System4InputRouter()
        #expect(router.pointer(event(.ended), at: at) == [.mouseMove(at)])
    }

    @Test func pointerReleasedLetsGoOnce() {
        var router = System4InputRouter()
        _ = router.pointer(event(.began, .secondary), at: at)
        #expect(router.pointerButtons == [2])
        #expect(router.pointerReleased(2) == [.key(2, down: false)])
        #expect(router.pointerReleased(2) == [])
        #expect(router.pointer(event(.ended, .secondary), at: at) == [.mouseMove(at)])
    }

    @Test func aTapOfAKeyHeldElsewhereSendsNothing() {
        var router = System4InputRouter()
        #expect(router.tap(13) == [.key(13, down: true), .key(13, down: false)])
        _ = router.translate(.button(.confirm, pressed: true)) { $0 }  // Confirm holds Return
        #expect(router.tap(13).isEmpty)                                // still held
        #expect(router.translate(.button(.confirm, pressed: false)) { $0 } == [.key(13, down: false)])
    }

    @Test func aControllerReleaseAfterFocusLossLeavesAKeyboardPressAlone() {
        var router = System4InputRouter()
        _ = router.translate(.button(.confirm, pressed: true)) { $0 }  // Confirm holds Return
        _ = router.releaseAll()                                        // the app lost focus
        #expect(router.hostKey(hidUsage: 40, pressed: true) == [.key(13, down: true)])  // Return on the keyboard
        #expect(router.translate(.button(.confirm, pressed: false)) { $0 }.isEmpty)     // the button comes up late
        #expect(router.hostKey(hidUsage: 40, pressed: false) == [.key(13, down: false)])
    }

    @Test func aModifierFollowsTheSystemNotAStaleToggle() {
        // Control was let go in another app; the view's toggle then reports its next press as a release.
        var router = System4InputRouter()
        _ = router.hostKey(hidUsage: 224, pressed: true)
        _ = router.releaseAll()  // focus lost
        let down = System4InputRouter.modifierDown(hidUsage: 224, control: true, shift: false, option: false, command: false)
        #expect(down == true)
        #expect(router.hostKey(hidUsage: 224, pressed: down!) == [.key(17, down: true)])
        let up = System4InputRouter.modifierDown(hidUsage: 224, control: false, shift: false, option: false, command: false)
        #expect(router.hostKey(hidUsage: 224, pressed: up!) == [.key(17, down: false)])
        #expect(System4InputRouter.modifierDown(hidUsage: 40, control: true, shift: true, option: true, command: true) == nil)
    }
}
