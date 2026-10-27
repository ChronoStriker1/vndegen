import QuartzCore
import VNCore
#if canImport(UIKit)
import UIKit

/// The view the game draws in, backed by the CAMetalLayer the engine renders to. Touches that reach the view, when the
/// hub shows no touch gamepad over it, work as the mouse, so a tap clicks where it lands. A hardware keyboard reaches
/// the game through this view, which takes keyboard focus whenever it is on screen.
final class GodotHostView: UIView {
    /// Called once, after the view is first in a window at a usable size.
    var onReady: (() -> Void)?
    /// Called when the view's size or scale changes after that.
    var onResize: ((CGSize, CGFloat) -> Void)?
    /// A key going down or up, as its USB HID usage.
    var onKey: ((Int, Bool) -> Void)?
    /// Touches, as pointer events.
    var onEvent: ((ControllerEvent) -> Void)?
    /// The touch that works as the mouse. Others are ignored until it lifts.
    private weak var pointerTouch: UITouch?
    private var reportedSize = CGSize.zero

    override class var layerClass: AnyClass { CAMetalLayer.self }
    var metalLayer: CAMetalLayer { layer as! CAMetalLayer }

    init() {
        super.init(frame: .zero)
        backgroundColor = .black
        metalLayer.pixelFormat = .bgra8Unorm
        metalLayer.framebufferOnly = true
    }

    required init?(coder: NSCoder) { fatalError("not used") }

    var scale: CGFloat { window?.screen.scale ?? traitCollection.displayScale }

    override func didMoveToWindow() {
        super.didMoveToWindow()
        metalLayer.contentsScale = scale
        readyIfOnScreen()
        takeFocus()
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        if onReady == nil, bounds.size != reportedSize, bounds.width >= 1, bounds.height >= 1 {
            reportedSize = bounds.size
            onResize?(bounds.size, scale)
        }
        readyIfOnScreen()
    }

    /// Starts the game after the current layout pass, so its first frame is drawn at the view's size.
    private func readyIfOnScreen() {
        guard window != nil, bounds.width >= 1, bounds.height >= 1, let onReady else { return }
        self.onReady = nil
        reportedSize = bounds.size
        Task { onReady() }
    }

    func takeFocus() {
        if window != nil { becomeFirstResponder() }
    }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard pointerTouch == nil, let touch = touches.first else { return }
        pointerTouch = touch
        pointer(.began, touch)
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        if let touch = pointerTouch, touches.contains(touch) { pointer(.moved, touch) }
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let touch = pointerTouch, touches.contains(touch) else { return }
        pointerTouch = nil
        pointer(.ended, touch)
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let touch = pointerTouch, touches.contains(touch) else { return }
        pointerTouch = nil
        pointer(.cancelled, touch)
    }

    private func pointer(_ phase: PointerEvent.Phase, _ touch: UITouch) {
        guard bounds.width > 0, bounds.height > 0 else { return }
        let point = touch.location(in: self)
        onEvent?(.pointer(PointerEvent(phase: phase, location: CGPoint(x: point.x / bounds.width, y: point.y / bounds.height))))
    }

    override var canBecomeFirstResponder: Bool { true }

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

    /// Sends keyboard presses to the game. UIKit's key codes are USB HID usages. Returns the presses the game doesn't
    /// get, such as the backtick, which opens the VNDegen menu.
    private func forward(_ presses: Set<UIPress>, pressed: Bool) -> Set<UIPress> {
        presses.filter { press in
            guard let usage = press.key?.keyCode.rawValue, usage != 53 else { return true }
            onKey?(usage, pressed)
            return false
        }
    }
}
#else
import AppKit

/// The view the game draws in, backed by the CAMetalLayer the engine renders to. It reads the mouse and keyboard for
/// the game and takes keyboard focus whenever it is on screen.
final class GodotHostView: NSView {
    var onReady: (() -> Void)?
    var onResize: ((CGSize, CGFloat) -> Void)?
    var onKey: ((Int, Bool) -> Void)?
    var onEvent: ((ControllerEvent) -> Void)?
    private var reportedSize = CGSize.zero

    let metalLayer = CAMetalLayer()

    init() {
        super.init(frame: .zero)
        wantsLayer = true
        metalLayer.pixelFormat = .bgra8Unorm
        metalLayer.framebufferOnly = true
        metalLayer.backgroundColor = NSColor.black.cgColor
    }

    required init?(coder: NSCoder) { fatalError("not used") }

    override func makeBackingLayer() -> CALayer { metalLayer }
    override var isFlipped: Bool { true }
    override var acceptsFirstResponder: Bool { true }
    override func acceptsFirstMouse(for event: NSEvent?) -> Bool { true }

    var scale: CGFloat { window?.backingScaleFactor ?? 2 }

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        metalLayer.contentsScale = scale
        readyIfOnScreen()
        takeFocus()
    }

    override func setFrameSize(_ newSize: NSSize) {
        super.setFrameSize(newSize)
        if onReady == nil, bounds.size != reportedSize, bounds.width >= 1, bounds.height >= 1 {
            reportedSize = bounds.size
            onResize?(bounds.size, scale)
        }
        readyIfOnScreen()
    }

    override func viewDidChangeBackingProperties() {
        super.viewDidChangeBackingProperties()
        metalLayer.contentsScale = scale
        if onReady == nil { onResize?(bounds.size, scale) }
    }

    private func readyIfOnScreen() {
        guard window != nil, bounds.width >= 1, bounds.height >= 1, let onReady else { return }
        self.onReady = nil
        reportedSize = bounds.size
        Task { onReady() }
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

    private func pointer(_ phase: PointerEvent.Phase, _ event: NSEvent, _ button: PointerEvent.Button) {
        guard bounds.width > 0, bounds.height > 0 else { return }
        let point = convert(event.locationInWindow, from: nil)
        onEvent?(.pointer(PointerEvent(phase: phase, location: CGPoint(x: point.x / bounds.width, y: point.y / bounds.height), button: button)))
    }
}
#endif
