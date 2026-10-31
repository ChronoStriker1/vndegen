import CoreGraphics
import Testing
@testable import EngineAliceSoft

/// Drives a pacer as the session does: `add` when input arrives, `step` before each game step. Every applied input is
/// recorded with the game step that first sees it.
private struct Harness {
    var pacer = System4InputPacer()
    var steps = 0  // game steps run so far
    var now = 0.0
    var applied: [(seenBy: Int, input: System4Input)] = []

    /// Input arriving between steps: the next step sees what applies.
    mutating func add(_ inputs: [System4Input]) {
        applied += pacer.add(inputs, now: now).map { (steps + 1, $0) }
    }
    /// One game step, `dt` seconds after the last.
    mutating func step(_ dt: Double = 1.0 / 60) {
        now += dt
        steps += 1
        applied += pacer.nextFrame(now: now).map { (steps, $0) }
    }
    mutating func steps(_ n: Int, _ dt: Double = 1.0 / 60) { for _ in 0..<n { step(dt) } }
    func seen(_ input: System4Input) -> [Int] { applied.filter { $0.input == input }.map(\.seenBy) }
    func time(_ step: Int) -> Double { Double(step) / 60 }
}

@Suite struct System4InputPacerTests {
    private let a = CGPoint(x: 0.2, y: 0.2), b = CGPoint(x: 0.8, y: 0.8)
    private let press = System4Input.key(1, down: true), release = System4Input.key(1, down: false)

    @Test func tapWaitsForHoverThenHoldsItsPress() {
        var h = Harness()
        h.add([.mouseMove(a), press, .mouseMove(a), release])  // a touch: all at once
        h.steps(30)
        let moved = h.seen(.mouseMove(a))[0], pressed = h.seen(press)[0], released = h.seen(release)[0]
        #expect(pressed - moved >= System4InputPacer.hoverFrames)
        #expect(released > pressed)
        #expect(h.time(released) - h.time(pressed) >= System4InputPacer.pressHold - 1e-9)
    }

    @Test func clickOnARestingPointerIsImmediate() {
        var h = Harness()
        h.add([.mouseMove(a)])
        h.steps(3)
        h.add([.mouseMove(a), press])
        #expect(h.seen(press) == [4])
    }

    @Test func aDelayedPressIsSeenWhereItWasMadeAndLaterMovesCollapse() {
        var h = Harness()
        h.add([.mouseMove(a), press])
        // The finger slides away before the hover wait is over: many moves queue behind the press.
        for i in 1...50 { h.add([.mouseMove(CGPoint(x: 0.2 + 0.012 * Double(i), y: 0.5))]) }
        h.add([.mouseMove(b)])
        h.steps(10)
        let pressed = h.seen(press)[0]
        let movesAfter = h.applied.filter { if case .mouseMove = $0.input { $0.seenBy >= pressed } else { false } }
        #expect(movesAfter.count == 1)                       // collapsed
        #expect(movesAfter.first?.input == .mouseMove(b))
        #expect(movesAfter.first!.seenBy > pressed)          // a step sees the press at a first
    }

    @Test func aPressWaitsForHoverEvenWithEveryOtherGateOpen() {
        var h = Harness()
        h.add([.mouseMove(a), press, release])
        h.steps(60)  // a second: every time gate long open
        h.add([.mouseMove(b), press, release])
        h.steps(30)
        let movedB = h.seen(.mouseMove(b))[0], pressedB = h.seen(press)[1]
        #expect(pressedB - movedB >= System4InputPacer.hoverFrames)
    }

    @Test func aReleaseBeforeARepressIsSeenByAStepEvenWhenFramesAreSlow() {
        var h = Harness()
        h.add([.mouseMove(a)])
        h.steps(3)
        h.add([press])
        h.steps(10)
        // The router's recovery from a lost mouse-up: release and press together, then a 200 ms frame.
        h.add([release, press])
        h.step(0.2)
        h.step(0.2)
        let released = h.seen(release)[0], repressed = h.seen(press)[1]
        #expect(repressed > released)  // at least one step runs with the button up
    }

    @Test func pauseReleasesWhatTheGameHolds() {
        var h = Harness()
        h.add([.mouseMove(a)])
        h.steps(3)
        h.add([press])              // applied
        h.add([release])            // waits behind the press's hold
        #expect(h.seen(release).isEmpty)
        #expect(h.pacer.clear() == [release])
        h.steps(10)
        #expect(h.seen(release).isEmpty)  // the queued release was dropped, not replayed
        #expect(h.pacer.down.isEmpty)
    }

    @Test func aDragsReleaseIsNotStuckBehindItsMoves() {
        var h = Harness()
        h.add([.mouseMove(a)])
        h.steps(3)
        h.add([press])
        // 120 moves a second for a second, two per frame.
        for i in 0..<120 {
            h.add([.mouseMove(CGPoint(x: 0.2 + 0.005 * Double(i), y: 0.5))])
            if i % 2 == 1 { h.step() }
        }
        h.add([.mouseMove(b), release])
        let arrived = h.steps
        h.steps(3)
        #expect(h.seen(release).first.map { $0 - arrived <= 1 } == true)
    }
}
