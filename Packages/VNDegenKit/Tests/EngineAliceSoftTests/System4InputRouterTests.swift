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
}
