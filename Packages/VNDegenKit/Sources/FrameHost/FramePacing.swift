import Foundation
import QuartzCore

#if DEBUG
public final class FramePacing {
    public enum Rate {
        case ticks
        case drawnFrames
    }

    private let tag: String
    private let rate: Rate
    private var last: CFTimeInterval?
    private var start: CFTimeInterval = 0
    private var intervals: [Double] = []
    private var drawn = 0
    private var lastDrawn: CFTimeInterval?
    private var drawnGaps: [Double] = []
    private var worstStep = 0.0
    private var work = 0.0

    public init?(tag: String, rate: Rate = .ticks) {
        guard UserDefaults.standard.bool(forKey: "frameMonitor") else { return nil }
        self.tag = tag
        self.rate = rate
    }

    public func reset() {
        last = nil
        lastDrawn = nil
        clearWindow()
    }

    public func note(_ link: CADisplayLink, presented: Bool? = nil, began: CFTimeInterval? = nil, target: @autoclosure () -> Double? = nil,
                     progress: @autoclosure () -> String? = nil) {
        let stepEnd = CACurrentMediaTime()
        let tick = link.timestamp
        defer { last = tick }
        guard let last else {
            start = tick
            return
        }
        intervals.append(tick - last)
        worstStep = max(worstStep, stepEnd - tick)
        if let began { work += stepEnd - began }
        if presented == true {
            drawn += 1
            if let lastDrawn { drawnGaps.append(tick - lastDrawn) }
            lastDrawn = tick
        }
        let elapsed = tick - start
        guard elapsed >= 10 else { return }
        let counted = rate == .ticks ? intervals : drawnGaps
        let slowest = counted.sorted(by: >).prefix(max(1, counted.count / 100))
        let low = slowest.isEmpty ? 0 : Double(slowest.count) / slowest.reduce(0, +)
        var line = String(format: "%@ seconds=%.1f average=%.1f low1=%.1f", tag, elapsed, Double(counted.count) / elapsed, low)
        switch rate {
        case .ticks: if presented != nil { line += String(format: " drawn=%.1f", Double(drawn) / elapsed) }
        case .drawnFrames: line += String(format: " ticks=%.1f", Double(intervals.count) / elapsed)
        }
        line += String(format: " worstStep=%.1fms", worstStep * 1000)
        if began != nil { line += String(format: " avgWork=%.1fms", work / Double(max(1, intervals.count)) * 1000) }
        line += String(format: " target=%.1f", target() ?? 1 / (link.targetTimestamp - link.timestamp))
        if let progress = progress() { line += " " + progress }
        NSLog("%@", line)
        clearWindow()
        start = tick
    }

    private func clearWindow() {
        intervals.removeAll(keepingCapacity: true)
        drawnGaps.removeAll(keepingCapacity: true)
        drawn = 0
        worstStep = 0
        work = 0
    }
}
#endif
