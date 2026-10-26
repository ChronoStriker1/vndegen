import VNCore
#if canImport(UIKit)
import UIKit

/// The view the game draws in. ANGLE adds a Metal layer inside the view's layer and keeps it the view's size. Touches
/// reach the game through the hub's touch controls. A hardware keyboard reaches the game through this view, which takes
/// keyboard focus whenever it is on screen, as the web engines' view does.
final class RGSSHostView: UIView {
    /// Called once, after the view is first in a window at a usable size.
    var onReady: (() -> Void)?
    /// A libretro key going down or up, and whether it is a key repeat.
    var onKey: ((UInt32, Bool, Bool) -> Void)?

    init() {
        super.init(frame: .zero)
        backgroundColor = .black
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

    /// Starts the game after the current layout pass, so its first frame is drawn at the view's size.
    private func readyIfOnScreen() {
        guard window != nil, bounds.width >= 1, bounds.height >= 1, let onReady else { return }
        self.onReady = nil
        Task { onReady() }
    }

    func takeFocus() {
        if window != nil { becomeFirstResponder() }
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

    /// Sends keyboard presses to the game. UIKit's key codes are USB HID usages. Returns the presses that are not keys
    /// the game reads, such as the backtick, which opens the VNDegen menu.
    private func forward(_ presses: Set<UIPress>, pressed: Bool) -> Set<UIPress> {
        presses.filter { press in
            guard let usage = press.key?.keyCode.rawValue, usage != 53, let key = RetroKey.hidUsage(usage) else { return true }
            onKey?(key, pressed, false)
            return false
        }
    }
}
#else
import AppKit

/// The view the game draws in. ANGLE adds a Metal layer inside the view's layer and keeps it the view's size. The view
/// reads the mouse and keyboard for the game, as the web engines' view does, and takes keyboard focus whenever it is on
/// screen.
final class RGSSHostView: NSView {
    /// Called once, after the view is first in a window at a usable size.
    var onReady: (() -> Void)?
    /// A libretro key going down or up, and whether it is a key repeat.
    var onKey: ((UInt32, Bool, Bool) -> Void)?
    /// The mouse, as pointer and scroll events.
    var onEvent: ((ControllerEvent) -> Void)?
    /// Modifier keys down, by virtual key code. AppKit reports them only as changed flags.
    private var heldModifiers: Set<UInt16> = []

    init() {
        super.init(frame: .zero)
        wantsLayer = true
        layer?.backgroundColor = NSColor.black.cgColor
    }

    required init?(coder: NSCoder) { fatalError("not used") }

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

    /// Starts the game after the current layout pass, so its first frame is drawn at the view's size.
    private func readyIfOnScreen() {
        guard window != nil, bounds.width >= 1, bounds.height >= 1, let onReady else { return }
        self.onReady = nil
        Task { onReady() }
    }

    /// ANGLE sizes its Metal layer in pixels from the layer's scale, which a layer-backed view's layer does not follow
    /// on its own.
    override func viewDidChangeBackingProperties() {
        super.viewDidChangeBackingProperties()
        layer?.contentsScale = window?.backingScaleFactor ?? 2
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
        let points = event.hasPreciseScrollingDeltas ? 1 : RGSSInputRouter.pointsPerNotch
        onEvent?(.scroll(dy: -event.scrollingDeltaY * points))
    }

    override func keyDown(with event: NSEvent) {
        guard let key = Self.key(event.keyCode) else { return }
        onKey?(key, true, event.isARepeat)
    }

    override func keyUp(with event: NSEvent) {
        guard let key = Self.key(event.keyCode) else { return }
        onKey?(key, false, false)
    }

    override func flagsChanged(with event: NSEvent) {
        guard let key = Self.key(event.keyCode), key != RetroKey.capsLock else { return }
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

    /// The libretro key for a macOS virtual key code (`kVK_*` in Carbon's Events.h), through its USB HID usage. The
    /// backtick, which opens the VNDegen menu, is not the game's.
    nonisolated static func key(_ keyCode: UInt16) -> UInt32? {
        guard keyCode != 0x32, let usage = hidUsages[keyCode] else { return nil }
        return RetroKey.hidUsage(usage)
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
