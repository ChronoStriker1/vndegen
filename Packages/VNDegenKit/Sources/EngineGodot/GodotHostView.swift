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
    /// A key going down or up, as its USB HID usage, with the text it types.
    var onKey: ((Int, Bool, String?) -> Void)?
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
    /// get, such as the backtick, which opens the VNDegen menu. Keys with Command or Control held type nothing.
    private func forward(_ presses: Set<UIPress>, pressed: Bool) -> Set<UIPress> {
        presses.filter { press in
            guard let key = press.key, key.keyCode.rawValue != 53 else { return true }
            let types = key.modifierFlags.isDisjoint(with: [.command, .control])
            onKey?(key.keyCode.rawValue, pressed, types ? key.characters : nil)
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
    var onKey: ((Int, Bool, String?) -> Void)?
    var onEvent: ((ControllerEvent) -> Void)?
    private var reportedSize = CGSize.zero
    /// The modifier keys down, by key code, since AppKit reports them only as a change in flags.
    private var heldModifiers: Set<UInt16> = []

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

    /// A trackpad scrolls by points and a mouse wheel by lines, one line to a notch. Both report how the content should
    /// move, positive for down, so the sign flips for VNDegen's scroll event.
    override func scrollWheel(with event: NSEvent) {
        let points = event.hasPreciseScrollingDeltas ? 1 : GodotInputRouter.pointsPerNotch
        onEvent?(.scroll(dy: -event.scrollingDeltaY * points))
    }

    /// Key repeats are left out, since each press must have one release.
    override func keyDown(with event: NSEvent) {
        guard !event.isARepeat, let usage = Self.usage(event.keyCode) else { return }
        let types = event.modifierFlags.isDisjoint(with: [.command, .control])
        onKey?(usage, true, types ? event.characters : nil)
    }

    override func keyUp(with event: NSEvent) {
        guard let usage = Self.usage(event.keyCode) else { return }
        onKey?(usage, false, nil)
    }

    override func flagsChanged(with event: NSEvent) {
        guard let usage = Self.usage(event.keyCode), usage != 57 else { return } // Caps Lock only toggles.
        let pressed = heldModifiers.insert(event.keyCode).inserted
        if !pressed { heldModifiers.remove(event.keyCode) }
        onKey?(usage, pressed, nil)
    }

    private func pointer(_ phase: PointerEvent.Phase, _ event: NSEvent, _ button: PointerEvent.Button) {
        guard bounds.width > 0, bounds.height > 0 else { return }
        let point = convert(event.locationInWindow, from: nil)
        onEvent?(.pointer(PointerEvent(phase: phase, location: CGPoint(x: point.x / bounds.width, y: point.y / bounds.height), button: button)))
    }

    /// The USB HID usage for a macOS virtual key code (`kVK_*` in Carbon's Events.h). The backtick, which opens the
    /// VNDegen menu, is not the game's.
    nonisolated static func usage(_ keyCode: UInt16) -> Int? {
        keyCode == 0x32 ? nil : hidUsages[keyCode]
    }

    nonisolated private static let hidUsages: [UInt16: Int] = [
        0x00: 4, 0x0B: 5, 0x08: 6, 0x02: 7, 0x0E: 8, 0x03: 9, 0x05: 10, 0x04: 11, 0x22: 12, 0x26: 13, 0x28: 14,
        0x25: 15, 0x2E: 16, 0x2D: 17, 0x1F: 18, 0x23: 19, 0x0C: 20, 0x0F: 21, 0x01: 22, 0x11: 23, 0x20: 24, 0x09: 25,
        0x0D: 26, 0x07: 27, 0x10: 28, 0x06: 29, // A to Z
        0x12: 30, 0x13: 31, 0x14: 32, 0x15: 33, 0x17: 34, 0x16: 35, 0x1A: 36, 0x1C: 37, 0x19: 38, 0x1D: 39, // 1 to 0
        0x24: 40, 0x35: 41, 0x33: 42, 0x30: 43, 0x31: 44, // Return, Escape, Delete, Tab, Space
        // - = [ ] \ ; ' , . /
        0x1B: 45, 0x18: 46, 0x21: 47, 0x1E: 48, 0x2A: 49, 0x29: 51, 0x27: 52, 0x2B: 54, 0x2F: 55, 0x2C: 56,
        0x39: 57, // Caps Lock
        0x7A: 58, 0x78: 59, 0x63: 60, 0x76: 61, 0x60: 62, 0x61: 63, 0x62: 64, 0x64: 65, 0x65: 66, 0x6D: 67, 0x67: 68,
        0x6F: 69, // F1 to F12
        0x72: 73, 0x73: 74, 0x74: 75, 0x75: 76, 0x77: 77, 0x79: 78, // Help as Insert, Home, Page Up, forward delete, End, Page Down
        0x7C: 79, 0x7B: 80, 0x7D: 81, 0x7E: 82, // arrows
        0x4B: 84, 0x43: 85, 0x4E: 86, 0x45: 87, 0x4C: 88, 0x53: 89, 0x54: 90, 0x55: 91, 0x56: 92, 0x57: 93,
        0x58: 94, 0x59: 95, 0x5B: 96, 0x5C: 97, 0x52: 98, 0x41: 99, 0x51: 103, // keypad
        0x3B: 224, 0x38: 225, 0x3A: 226, 0x37: 227, 0x3E: 228, 0x3C: 229, 0x3D: 230, 0x36: 231, // modifiers, left then right
    ]
}
#endif
