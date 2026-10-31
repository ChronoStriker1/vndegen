import Foundation
import QuartzCore

#if DEBUG
/// With `-frameMonitor YES`, a session logs its pacing every 10 seconds under its tag ("S3FRAMES", "S4FRAMES" in the
/// app log): display link ticks per second on average and over the slowest 1% of ticks, frames the runtime drew, and
/// the slowest step.
final class FramePacing {
    private let tag: String
    private var last: CFTimeInterval?
    private var start: CFTimeInterval = 0
    private var intervals: [Double] = []
    private var presented = 0
    private var worstStep = 0.0

    init(tag: String) { self.tag = tag }

    /// Starts a new window, so the gap across a pause doesn't count as a slow frame.
    func reset() {
        last = nil
        intervals.removeAll(keepingCapacity: true)
        presented = 0
        worstStep = 0
    }

    func note(tick: CFTimeInterval, presented drew: Bool, stepEnd: CFTimeInterval, target: CFTimeInterval) {
        defer { last = tick }
        guard let last else {
            start = tick
            return
        }
        intervals.append(tick - last)
        if drew { presented += 1 }
        worstStep = max(worstStep, stepEnd - tick)
        guard tick - start >= 10 else { return }
        let slowest = intervals.sorted(by: >).prefix(max(1, intervals.count / 100))
        let low = Double(slowest.count) / slowest.reduce(0, +)
        NSLog("%@ seconds=%.1f average=%.1f low1=%.1f drawn=%.1f worstStep=%.1fms target=%.1f", tag, tick - start,
              Double(intervals.count) / (tick - start), low, Double(presented) / (tick - start), worstStep * 1000, 1 / target)
        intervals.removeAll(keepingCapacity: true)
        presented = 0
        worstStep = 0
        start = tick
    }
}
#endif
