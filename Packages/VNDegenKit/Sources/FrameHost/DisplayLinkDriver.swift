import QuartzCore
#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

public final class DisplayLinkDriver {
    private var link: CADisplayLink?

    public init() {}

    public func start(in view: FrameHostBaseView, step: @escaping (CADisplayLink) -> Void) {
        stop()
        let target = Target(step)
        #if canImport(UIKit)
        let link = CADisplayLink(target: target, selector: #selector(Target.step(_:)))
        #else
        let link = view.displayLink(target: target, selector: #selector(Target.step(_:)))
        #endif
        link.add(to: .main, forMode: .common)
        self.link = link
    }

    public func stop() {
        link?.invalidate()
        link = nil
    }
}

private final class Target: NSObject {
    private let action: (CADisplayLink) -> Void

    init(_ action: @escaping (CADisplayLink) -> Void) {
        self.action = action
    }

    @objc func step(_ link: CADisplayLink) { action(link) }
}
