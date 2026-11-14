import FrameHost
import QuartzCore
import VNCore
#if canImport(UIKit)
import UIKit

/// The view the engine draws the game in with Metal, fit to the view with bars beside or above it. It reads touches as
/// the mouse, and a hardware keyboard, taking keyboard focus whenever it is on screen.
final class ArtemisHostView: UIView {
    /// Called once, after the view is first in a window at a usable size.
    var onReady: (@MainActor () -> Void)?
    /// A virtual-key code going down or up, and whether it is a key repeat.
    var onKey: ((Int32, Bool, Bool) -> Void)?
    /// Touches, as the mouse.
    var onEvent: ((ControllerEvent) -> Void)?

    override class var layerClass: AnyClass { CAMetalLayer.self }
    var metalLayer: CAMetalLayer { layer as! CAMetalLayer }
    /// A fullscreen movie, over the game.
    let movieLayer = ArtemisHostView.makeMovieLayer()

    init() {
        super.init(frame: .zero)
        backgroundColor = .black
        isMultipleTouchEnabled = false
        metalLayer.contentsGravity = .resizeAspect
        metalLayer.addSublayer(movieLayer)
    }

    required init?(coder: NSCoder) { fatalError("not used") }

    override func didMoveToWindow() {
        super.didMoveToWindow()
        readyIfOnScreen()
        takeFocus()
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        CATransaction.performWithoutAnimation { movieLayer.frame = bounds }
        readyIfOnScreen()
    }

    private func readyIfOnScreen() {
        guard window != nil, bounds.width >= 1, bounds.height >= 1, let onReady else { return }
        self.onReady = nil
        RunLoop.main.perform(inModes: [.common]) { MainActor.assumeIsolated(onReady) }
    }

    func takeFocus() {
        if window != nil { becomeFirstResponder() }
    }

    override var canBecomeFirstResponder: Bool { true }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) { pointer(.began, touches) }
    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) { pointer(.moved, touches) }
    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) { pointer(.ended, touches) }
    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) { pointer(.cancelled, touches) }

    private func pointer(_ phase: PointerEvent.Phase, _ touches: Set<UITouch>) {
        guard let touch = touches.first, bounds.width > 0, bounds.height > 0 else { return }
        let point = touch.location(in: self)
        let location = CGPoint(x: point.x / bounds.width, y: point.y / bounds.height)
        onEvent?(.pointer(PointerEvent(phase: phase, location: location, button: .primary)))
    }

    override func pressesBegan(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        let rest = forward(presses, pressed: true)
        if !rest.isEmpty { super.pressesBegan(rest, with: event) }
    }

    override func pressesEnded(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        let rest = forward(presses, pressed: false)
        if !rest.isEmpty { super.pressesEnded(rest, with: event) }
    }

    override func pressesCancelled(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        let rest = forward(presses, pressed: false)
        if !rest.isEmpty { super.pressesCancelled(rest, with: event) }
    }

    /// Sends keyboard presses to the game. UIKit's key codes are USB HID usages. Returns the presses that are not keys.
    private func forward(_ presses: Set<UIPress>, pressed: Bool) -> Set<UIPress> {
        presses.filter { press in
            guard let key = press.key else { return true }
            if let virtualKey = VirtualKey.forUsage(key.keyCode.rawValue) { onKey?(virtualKey, pressed, false) }
            return false
        }
    }
}
#else
import AppKit

/// The view the engine draws the game in with Metal, fit to the view with bars beside or above it. It reads the mouse
/// and keyboard for the game, and takes keyboard focus whenever it is on screen.
final class ArtemisHostView: NSView {
    /// Called once, after the view is first in a window at a usable size.
    var onReady: (@MainActor () -> Void)?
    /// A virtual-key code going down or up, and whether it is a key repeat.
    var onKey: ((Int32, Bool, Bool) -> Void)?
    /// The mouse, as pointer and scroll events.
    var onEvent: ((ControllerEvent) -> Void)?
    /// Modifier keys down, by macOS key code. AppKit reports them only as changed flags.
    private var heldModifiers: Set<UInt16> = []

    let metalLayer = CAMetalLayer()
    /// A fullscreen movie, over the game.
    let movieLayer = ArtemisHostView.makeMovieLayer()

    init() {
        super.init(frame: .zero)
        wantsLayer = true
        metalLayer.contentsGravity = .resizeAspect
        metalLayer.backgroundColor = NSColor.black.cgColor
        metalLayer.addSublayer(movieLayer)
    }

    required init?(coder: NSCoder) { fatalError("not used") }

    override func makeBackingLayer() -> CALayer { metalLayer }
    override var isFlipped: Bool { true }
    override var acceptsFirstResponder: Bool { true }
    override func acceptsFirstMouse(for event: NSEvent?) -> Bool { true }

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        readyIfOnScreen()
        takeFocus()
    }

    override func setFrameSize(_ newSize: NSSize) {
        super.setFrameSize(newSize)
        CATransaction.performWithoutAnimation { movieLayer.frame = bounds }
        readyIfOnScreen()
    }

    private func readyIfOnScreen() {
        guard window != nil, bounds.width >= 1, bounds.height >= 1, let onReady else { return }
        self.onReady = nil
        RunLoop.main.perform(inModes: [.common]) { MainActor.assumeIsolated(onReady) }
    }

    func takeFocus() {
        window?.makeFirstResponder(self)
    }

    override func updateTrackingAreas() {
        super.updateTrackingAreas()
        trackingAreas.forEach(removeTrackingArea)
        addTrackingArea(NSTrackingArea(rect: .zero, options: [.mouseMoved, .activeInKeyWindow, .inVisibleRect], owner: self))
    }

    override func mouseDown(with event: NSEvent) { pointer(.began, event, .primary) }
    override func mouseDragged(with event: NSEvent) { pointer(.moved, event, .primary) }
    override func mouseUp(with event: NSEvent) { pointer(.ended, event, .primary) }
    override func mouseMoved(with event: NSEvent) { pointer(.moved, event, .primary) }
    override func rightMouseDown(with event: NSEvent) { pointer(.began, event, .secondary) }
    override func rightMouseDragged(with event: NSEvent) { pointer(.moved, event, .secondary) }
    override func rightMouseUp(with event: NSEvent) { pointer(.ended, event, .secondary) }

    /// A trackpad scrolls by points and a mouse wheel by lines, one line to a notch. Both report how the content should
    /// move, positive for down, so the sign flips for VNDegen's scroll event.
    override func scrollWheel(with event: NSEvent) {
        let points = event.hasPreciseScrollingDeltas ? 1 : ArtemisInputRouter.pointsPerNotch
        onEvent?(.scroll(dy: -event.scrollingDeltaY * points))
    }

    override func keyDown(with event: NSEvent) {
        guard let key = Self.virtualKey(event.keyCode) else { return }
        onKey?(key, true, event.isARepeat)
    }

    override func keyUp(with event: NSEvent) {
        guard let key = Self.virtualKey(event.keyCode) else { return }
        onKey?(key, false, false)
    }

    override func flagsChanged(with event: NSEvent) {
        guard let key = Self.virtualKey(event.keyCode) else { return }
        let pressed = heldModifiers.insert(event.keyCode).inserted
        if !pressed { heldModifiers.remove(event.keyCode) }
        onKey?(key, pressed, false)
    }

    private func pointer(_ phase: PointerEvent.Phase, _ event: NSEvent, _ button: PointerEvent.Button) {
        guard bounds.width > 0, bounds.height > 0 else { return }
        let point = convert(event.locationInWindow, from: nil)
        let location = CGPoint(x: point.x / bounds.width, y: point.y / bounds.height)
        onEvent?(.pointer(PointerEvent(phase: phase, location: location, button: button)))
    }

    static func virtualKey(_ keyCode: UInt16) -> Int32? {
        MacKeyCodes.hidUsage(keyCode).flatMap(VirtualKey.forUsage)
    }

}
#endif

extension ArtemisHostView {
    /// A black layer that fits a movie's frames to the view, as the game is, and starts hidden.
    fileprivate static func makeMovieLayer() -> CALayer {
        let layer = CALayer()
        layer.contentsGravity = .resizeAspect
        layer.backgroundColor = CGColor(gray: 0, alpha: 1)
        layer.isHidden = true
        layer.actions = ["contents": NSNull(), "hidden": NSNull(), "bounds": NSNull(), "position": NSNull()]
        return layer
    }
}

extension CATransaction {
    fileprivate static func performWithoutAnimation(_ body: () -> Void) {
        begin()
        setDisableActions(true)
        body()
        commit()
    }
}
