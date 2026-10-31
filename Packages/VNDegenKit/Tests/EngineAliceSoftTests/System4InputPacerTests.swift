import CoreGraphics
import Testing
@testable import EngineAliceSoft

/// Drives a pacer as the session does: `add` when input arrives, `poll` before each game step, and the game finishing
/// one frame per step. Every applied input is recorded with the frames done and the time when it applied.
private struct Harness {
    var pacer = System4InputPacer()
    var done = 0
    var now = 0.0
    var applied: [(done: Int, time: Double, input: System4Input)] = []

    mutating func add(_ inputs: [System4Input]) {
        applied += pacer.add(inputs, now: now, done: done).map { (done, now, $0) }
    }
    /// One game step `dt` after the last: what is due applies, then the game finishes a frame.
    mutating func step(_ dt: Double = 1.0 / 60, finishes: Bool = true) {
        now += dt
        applied += pacer.poll(now: now, done: done).map { (done, now, $0) }
        if finishes { done += 1 }
    }
    mutating func steps(_ n: Int, _ dt: Double = 1.0 / 60) { for _ in 0..<n { step(dt) } }
    /// The game finishes frames with no poll in between, and time passes.
    mutating func idle(frames: Int, _ dt: Double) {
        done += frames
        now += dt
    }
    func at(_ input: System4Input) -> [(done: Int, time: Double)] {
        applied.filter { $0.input == input }.map { ($0.done, $0.time) }
    }
}

@Suite struct System4InputPacerTests {
    private let a = CGPoint(x: 0.2, y: 0.2), b = CGPoint(x: 0.8, y: 0.8)
    private let press = System4Input.key(1, down: true), release = System4Input.key(1, down: false)
    private let enter = System4Input.key(13, down: true), enterUp = System4Input.key(13, down: false)

    @Test func tapWaitsForHoverThenHoldsItsPress() {
        var h = Harness()
        h.add([.mouseMove(a), press, .mouseMove(a), release])  // a touch: all at once
        h.steps(30)
        let moved = h.at(.mouseMove(a))[0], pressed = h.at(press)[0], released = h.at(release)[0]
        // Three whole frames with the pointer there: frames 2 to 4 after a move at 0 (frame 1 may have started).
        #expect(pressed.done - moved.done >= 1 + System4InputPacer.hoverFrames)
        #expect(released.done - pressed.done >= 2)
        #expect(released.time - pressed.time >= System4InputPacer.pressHold - 1e-9)
    }

    @Test func clickOnARestingPointerIsImmediate() {
        var h = Harness()
        h.add([.mouseMove(a)])
        h.steps(4)
        h.add([.mouseMove(a), press])
        #expect(h.at(press).count == 1)
    }

    @Test func aDelayedPressIsReadWhereItWasMadeAndLaterMovesCollapse() {
        var h = Harness()
        h.add([.mouseMove(a), press])
        // The finger slides away before the hover wait is over: many moves queue behind the press.
        for i in 1...50 { h.add([.mouseMove(CGPoint(x: 0.2 + 0.012 * Double(i), y: 0.5))]) }
        h.add([.mouseMove(b)])
        h.steps(12)
        let pressed = h.at(press)[0]
        let after = h.applied.filter { if case .mouseMove = $0.input { $0.time >= pressed.time } else { false } }
        #expect(after.count == 1)                          // collapsed
        #expect(after.first?.input == .mouseMove(b))
        #expect(after.first!.done >= pressed.done + 2)     // the game read the press at a first
    }

    @Test func aPressWaitsForHoverEvenWithEveryOtherGateOpen() {
        var h = Harness()
        h.add([.mouseMove(a), press, release])
        h.steps(60)  // a second: every other gate long open
        h.add([.mouseMove(b), press, release])
        h.steps(30)
        let movedB = h.at(.mouseMove(b))[0], pressedB = h.at(press)[1]
        #expect(pressedB.done - movedB.done >= 1 + System4InputPacer.hoverFrames)
    }

    @Test func aReleaseHoldsFiftyMillisecondsBeforeARepress() {
        var h = Harness()
        h.add([enter])
        h.steps(20)
        h.add([enterUp])
        // The game reads the release at once (many frames done), but only 20 ms pass: the repress must wait.
        h.idle(frames: 5, 0.02)
        h.add([enter])
        #expect(h.at(enter).count == 1)
        h.idle(frames: 5, 0.02)
        h.add([])
        #expect(h.at(enter).count == 1)  // 40 ms
        h.idle(frames: 1, 0.015)
        h.add([])
        let released = h.at(enterUp)[0], repressed = h.at(enter)[1]
        #expect(repressed.time - released.time >= System4InputPacer.releaseHold - 1e-9)
    }

    @Test func aReleaseIsReadBeforeARepressWhileTheGameIsBrieflyBusy() {
        var h = Harness()
        h.add([enter])
        h.steps(20)
        h.add([enterUp, enter])
        // Busy for 0.4 s: steps time out without finishing a frame.
        for _ in 0..<4 { h.step(0.1, finishes: false) }
        #expect(h.at(enter).count == 1)
        h.steps(3)
        let released = h.at(enterUp)[0], repressed = h.at(enter)[1]
        #expect(repressed.done - released.done >= 2)
    }

    @Test func aBusyGameHoldsAReleaseAtMostHalfASecond() {
        var h = Harness()
        h.add([enter])
        h.steps(20)
        h.add([enterUp, enter])  // applies the release; then the game never finishes another frame
        h.add([])
        let released = h.at(enterUp)[0]
        for _ in 0..<10 { h.step(0.1, finishes: false) }
        // Nothing waits on a game that has stopped reading input for more than the limit.
        #expect(h.at(enter).count == 2)
        #expect(h.at(enter)[1].time - released.time <= System4InputPacer.readWaitLimit + 0.1 + 1e-9)
    }

    @Test func aPressIsntStuckBehindAGameThatStoppedReading() {
        var h = Harness()
        h.add([enter])
        // The game loops without finishing frames, waiting for the key to come up.
        for _ in 0..<3 { h.step(0.1, finishes: false) }
        h.add([enterUp])
        for _ in 0..<5 { h.step(0.1, finishes: false) }
        #expect(h.at(enterUp).count == 1)
    }

    @Test func pauseReleasesWhatTheGameHoldsAndTheyAreReadBeforeAnythingElse() {
        var h = Harness()
        h.add([.mouseMove(a)])
        h.steps(4)
        h.add([press])            // applied
        h.steps(30)               // every gate from the press long expired
        let released = h.pacer.clear(now: h.now, done: h.done)
        #expect(released == [release])
        #expect(h.pacer.down.isEmpty)
        // Resume: a new press before the game has run a frame waits until it has read the release.
        let pausedAt = h.done
        h.add([.mouseMove(a), press])
        #expect(h.at(press).count == 1)
        h.steps(5)
        #expect(h.at(press).count == 2)
        #expect(h.at(press)[1].done >= pausedAt + 2)
        #expect(h.at(release).isEmpty)  // only pause let go of it
    }

    @Test func aDragsReleaseIsNotStuckBehindItsMoves() {
        var h = Harness()
        h.add([.mouseMove(a)])
        h.steps(4)
        h.add([press])
        for i in 0..<120 {  // 120 moves a second for a second, two per frame
            h.add([.mouseMove(CGPoint(x: 0.2 + 0.005 * Double(i), y: 0.5))])
            if i % 2 == 1 { h.step() }
        }
        let arrived = h.done
        h.add([.mouseMove(b), release])
        h.steps(3)
        #expect(h.at(release).first.map { $0.done - arrived <= 1 } == true)
    }
}
