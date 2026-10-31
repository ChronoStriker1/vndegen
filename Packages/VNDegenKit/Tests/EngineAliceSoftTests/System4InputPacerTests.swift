import CoreGraphics
import Testing
@testable import EngineAliceSoft

@Suite struct System4InputPacerTests {
    private let a = CGPoint(x: 0.2, y: 0.2), b = CGPoint(x: 0.8, y: 0.8)

    /// Runs frames 1/60 s apart from `start` until the pacer has handed out `count` key events, returning everything.
    private func frames(_ pacer: inout System4InputPacer, from start: Double, upTo count: Int = 10, limit: Int = 120)
        -> [(Double, System4Input)] {
        var out: [(Double, System4Input)] = []
        for n in 1...limit {
            let now = start + Double(n) / 60
            out += pacer.nextFrame(now: now).map { (now, $0) }
            if out.filter({ if case .key = $0.1 { true } else { false } }).count >= count { break }
        }
        return out
    }

    @Test func tapWaitsForHoverThenHoldsItsPress() {
        var p = System4InputPacer()
        // A touch: move, press and release all at once.
        let now = p.add([.mouseMove(a), .key(1, down: true), .mouseMove(a), .key(1, down: false)], now: 0)
        #expect(now == [.mouseMove(a)])
        let later = frames(&p, from: 0, upTo: 2)
        let press = later.first { $0.1 == .key(1, down: true) }!, release = later.first { $0.1 == .key(1, down: false) }!
        #expect(press.0 >= 3.0 / 60 - 1e-9)
        #expect(release.0 - press.0 >= System4InputPacer.pressHold - 1e-9)
    }

    @Test func clickOnARestingPointerIsImmediate() {
        var p = System4InputPacer()
        _ = p.add([.mouseMove(a)], now: 0)
        _ = frames(&p, from: 0, upTo: 1, limit: 3)
        #expect(p.add([.mouseMove(a), .key(1, down: true)], now: 0.1) == [.mouseMove(a), .key(1, down: true)])
    }

    @Test func movesDuringADragCollapseAndNeverDelayTheRelease() {
        var p = System4InputPacer()
        _ = p.add([.mouseMove(a)], now: 0)
        _ = frames(&p, from: 0, upTo: 1, limit: 3)
        _ = p.add([.mouseMove(a), .key(1, down: true)], now: 0.05)
        // 120 moves a second for a second, then the release.
        var t = 0.05
        var applied: [System4Input] = []
        for i in 0..<120 {
            t += 1.0 / 120
            let x = 0.2 + 0.6 * Double(i) / 119
            applied += p.add([.mouseMove(CGPoint(x: x, y: 0.5))], now: t)
            if i % 2 == 1 { applied += p.nextFrame(now: t) }
        }
        applied += p.add([.mouseMove(b), .key(1, down: false)], now: t)
        // The release is due at once: the press was down far longer than its hold.
        #expect(applied.last == .key(1, down: false))
    }

    @Test func aQueuedTapStillWaitsForHover() {
        var p = System4InputPacer()
        _ = p.add([.mouseMove(a)], now: 0)
        _ = frames(&p, from: 0, upTo: 1, limit: 3)
        _ = p.add([.mouseMove(a), .key(1, down: true)], now: 0.05)
        // A second tap elsewhere while the first press still holds the gate.
        _ = p.add([.mouseMove(a), .key(1, down: false), .mouseMove(b), .key(1, down: true), .mouseMove(b), .key(1, down: false)],
                  now: 0.06)
        let later = frames(&p, from: 0.06, upTo: 3)
        let moveB = later.firstIndex { $0.1 == .mouseMove(b) }!
        let pressB = later.firstIndex { $0.1 == .key(1, down: true) }!
        #expect(later[pressB].0 - later[moveB].0 >= 3.0 / 60 - 1e-9)
    }

    @Test func aReleaseBeforeARepressStaysVisible() {
        // The router's recovery from a lost mouse-up sends up and down together.
        var p = System4InputPacer()
        _ = p.add([.mouseMove(a)], now: 0)
        _ = frames(&p, from: 0, upTo: 1, limit: 3)
        _ = p.add([.mouseMove(a), .key(1, down: false), .key(1, down: true)], now: 1)
        let later = frames(&p, from: 1, upTo: 1)
        #expect(later.first { $0.1 == .key(1, down: true) }!.0 - 1 >= System4InputPacer.releaseHold - 1e-9)
    }

    @Test func clearDropsWaitingInput() {
        var p = System4InputPacer()
        _ = p.add([.mouseMove(a), .key(1, down: true)], now: 0)
        p.clear()
        #expect(frames(&p, from: 0, upTo: 1, limit: 10).isEmpty)
    }
}
