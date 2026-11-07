import Foundation
import Testing
import FrameHost

/// How far the milliseconds a clock passes on over `seconds` of refreshes at `hertz` fall from the time those refreshes
/// cover. The timestamps start far from zero, as the display link's do.
private func drift(hertz: Double, seconds: Double, jitter: (Int) -> Double = { _ in 0 }) -> Double {
    var clock = MillisecondClock()
    let start = 86_400.123
    _ = clock.tick(at: start)
    let ticks = Int(hertz * seconds)
    let passed = (1...ticks).reduce(0) { sum, n in
        sum + clock.tick(at: start + Double(n) / hertz + jitter(n))
    }
    return Double(passed) - (Double(ticks) / hertz + jitter(ticks)) * 1000
}

@Test(arguments: [120.0, 60.0, 144.0, 59.94, 30.0])
func stepsAddUpToTheRealTime(hertz: Double) {
    // Ten seconds of refreshes come to ten seconds of game time, less the fraction still carried.
    #expect(abs(drift(hertz: hertz, seconds: 10)) < 1)
}

@Test func jitterInTheTimestampsDoesNotAddUp() {
    // ±0.4 ms of jitter on each refresh still comes to the real time over the run.
    #expect(abs(drift(hertz: 120, seconds: 10) { n in n.isMultiple(of: 2) ? 0.0004 : -0.0004 }) < 1)
}

@Test func the120HzStepsAreEightsAndNines() {
    var clock = MillisecondClock()
    _ = clock.tick(at: 1000)
    let steps = (1...120).map { clock.tick(at: 1000 + Double($0) / 120) }
    #expect(steps.allSatisfy { $0 == 8 || $0 == 9 })
    #expect(steps.filter { $0 == 9 }.count >= 39)
}

@Test func theFirstTickAndOneAfterAResetAreSixteen() {
    var clock = MillisecondClock()
    #expect(clock.tick(at: 50) == 16)
    #expect(clock.tick(at: 50.0105) == 10)
    clock.reset()
    // A pause of a minute doesn't come through as one step, and the half millisecond carried before it is gone.
    #expect(clock.tick(at: 110) == 16)
    #expect(clock.tick(at: 110.0105) == 10)
}

@Test func anEarlierTimestampIsIgnored() {
    var clock = MillisecondClock()
    _ = clock.tick(at: 1)
    #expect(clock.tick(at: 1.015625) == 15)
    #expect(clock.tick(at: 1.0078125) == 0)
    // 31.25 ms in all since the first tick, 0.25 still carried.
    #expect(clock.tick(at: 1.03125) == 16)
}
