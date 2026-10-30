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

    init() {
        super.init(frame: .zero)
        backgroundColor = .black
        isMultipleTouchEnabled = false
        metalLayer.contentsGravity = .resizeAspect
    }

    required init?(coder: NSCoder) { fatalError("not used") }

    override func didMoveToWindow() {
        super.didMoveToWindow()
        readyIfOnScreen()
        takeFocus()
    }

    override func layoutSubviews() {
        super.layoutSubviews()
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

    init() {
        super.init(frame: .zero)
        wantsLayer = true
        metalLayer.contentsGravity = .resizeAspect
        metalLayer.backgroundColor = NSColor.black.cgColor
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

    private static func virtualKey(_ keyCode: UInt16) -> Int32? {
        usages[keyCode].flatMap { VirtualKey.forUsage(Int($0)) }
    }

    /// USB HID usages for macOS virtual key codes (`kVK_*` in Carbon's Events.h), as KiriKiri's view maps them.
    private static let usages: [UInt16: Int32] = [
        0x00: 4, 0x0B: 5, 0x08: 6, 0x02: 7, 0x0E: 8, 0x03: 9, 0x05: 10, 0x04: 11, 0x22: 12, 0x26: 13, 0x28: 14,
        0x25: 15, 0x2E: 16, 0x2D: 17, 0x1F: 18, 0x23: 19, 0x0C: 20, 0x0F: 21, 0x01: 22, 0x11: 23, 0x20: 24, 0x09: 25,
        0x0D: 26, 0x07: 27, 0x10: 28, 0x06: 29, // A to Z
        0x12: 30, 0x13: 31, 0x14: 32, 0x15: 33, 0x17: 34, 0x16: 35, 0x1A: 36, 0x1C: 37, 0x19: 38, 0x1D: 39, // 1 to 0
        0x24: 40, 0x35: 41, 0x33: 42, 0x30: 43, 0x31: 44, // Return, Escape, Delete, Tab, Space
        0x7A: 58, 0x78: 59, 0x63: 60, 0x76: 61, 0x60: 62, 0x61: 63, 0x62: 64, 0x64: 65, 0x65: 66, 0x6D: 67, 0x67: 68,
        0x6F: 69, // F1 to F12
        0x72: 73, 0x73: 74, 0x74: 75, 0x75: 76, 0x77: 77, 0x79: 78, // Help as Insert, Home, Page Up, forward delete, End, Page Down
        0x7C: 79, 0x7B: 80, 0x7D: 81, 0x7E: 82, // arrows
        0x4B: 84, 0x43: 85, 0x4E: 86, 0x45: 87, 0x4C: 88, 0x53: 89, 0x54: 90, 0x55: 91, 0x56: 92, 0x57: 93,
        0x58: 94, 0x59: 95, 0x5B: 96, 0x5C: 97, 0x52: 98, 0x41: 99, // keypad
        0x3B: 224, 0x38: 225, 0x3A: 226, 0x3E: 228, 0x3C: 229, 0x3D: 230, // Control, Shift, Option, left then right
    ]
}
#endif
