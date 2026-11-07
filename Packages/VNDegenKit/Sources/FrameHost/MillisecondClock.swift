import Foundation

/// Turns display refresh timestamps into whole-millisecond steps for a runtime whose clock counts milliseconds. Each
/// step carries the fraction it leaves to the next, so the steps add up to the real time. Rounding each refresh on its
/// own ran games and movies 4% slow at 120 Hz (8.33 ms passed as 8) and 2% fast at 60 Hz (16.67 ms passed as 17).
nonisolated public struct MillisecondClock {
    private var last: TimeInterval?
    private var carry = 0.0  // the part of a millisecond the last step didn't pass on

    public init() {}

    /// The whole milliseconds since the last tick, or 16 on the first tick after a start or `reset()`. A timestamp
    /// earlier than the last is a step of 0 and is otherwise ignored, so the time after it isn't counted twice. Callers
    /// cap the step themselves; time above the cap is dropped, not carried.
    public mutating func tick(at timestamp: TimeInterval) -> Int {
        guard let last else {
            self.last = timestamp
            return 16
        }
        guard timestamp >= last else { return 0 }
        self.last = timestamp
        let elapsed = (timestamp - last) * 1000 + carry
        let ms = elapsed.rounded(.down)
        carry = elapsed - ms
        return Int(ms)
    }

    /// Forgets the last tick, so the time a pause lasted isn't passed on as one step.
    public mutating func reset() {
        last = nil
        carry = 0
    }
}
