import FrameHost
import VNCore
#if canImport(UIKit)
import UIKit

/// Holds the view SDL draws the game in. SDL reads touches there itself. A hardware keyboard reaches the game through
/// this view, which takes keyboard focus whenever it is on screen, as the web engines' view does.
final class KiriKiriHostView: UIView {
    /// Called once, after the view is first in a window at a usable size.
    var onReady: (@MainActor () -> Void)?
    /// An SDL scancode going down or up, and whether it is a key repeat.
    var onKey: ((Int32, Bool, Bool) -> Void)?
    /// The characters a key going down typed, which reach the game after the key.
    var onText: ((String) -> Void)?
    /// Unused on iOS, where touches reach SDL's view directly.
    var onEvent: ((ControllerEvent) -> Void)?

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

    private func readyIfOnScreen() {
        guard window != nil, bounds.width >= 1, bounds.height >= 1, let onReady else { return }
        self.onReady = nil
        Self.startAfterLayout(onReady)
    }

    func embed(_ engineView: UIView) {
        engineView.frame = bounds
        engineView.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        addSubview(engineView)
        // SDL made its own, now hidden, window key when it showed it.
        window?.makeKey()
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

    /// Sends keyboard presses to the game. UIKit's key codes are USB HID usages, which SDL's scancodes are too.
    /// Returns the presses that are not keys.
    private func forward(_ presses: Set<UIPress>, pressed: Bool) -> Set<UIPress> {
        presses.filter { press in
            guard let key = press.key else { return true }
            onKey?(Int32(key.keyCode.rawValue), pressed, false)
            if pressed, !key.modifierFlags.contains(.command) { onText?(key.characters) }
            return false
        }
    }
}
#else
import AppKit

/// The view the engine draws the game in, filling it. It reads the mouse and keyboard for the game, as the web
/// engines' view does, and takes keyboard focus whenever it is on screen.
final class KiriKiriHostView: NSView {
    /// Called once, after the view is first in a window at a usable size.
    var onReady: (@MainActor () -> Void)?
    /// An SDL scancode going down or up, and whether it is a key repeat.
    var onKey: ((Int32, Bool, Bool) -> Void)?
    /// The characters a key going down typed, which reach the game after the key.
    var onText: ((String) -> Void)?
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

    private func readyIfOnScreen() {
        guard window != nil, bounds.width >= 1, bounds.height >= 1, let onReady else { return }
        self.onReady = nil
        Self.startAfterLayout(onReady)
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
        let points = event.hasPreciseScrollingDeltas ? 1 : KiriKiriInputRouter.pointsPerNotch
        onEvent?(.scroll(dy: -event.scrollingDeltaY * points))
    }

    override func keyDown(with event: NSEvent) {
        guard let scancode = Self.scancode(event.keyCode) else { return }
        onKey?(scancode, true, event.isARepeat)
        if !event.modifierFlags.contains(.command), let characters = event.characters { onText?(characters) }
    }

    override func keyUp(with event: NSEvent) {
        guard let scancode = Self.scancode(event.keyCode) else { return }
        onKey?(scancode, false, false)
    }

    override func flagsChanged(with event: NSEvent) {
        guard let scancode = Self.scancode(event.keyCode), scancode != Self.capsLock else { return }
        let pressed = heldModifiers.insert(event.keyCode).inserted
        if !pressed { heldModifiers.remove(event.keyCode) }
        onKey?(scancode, pressed, false)
    }

    private func pointer(_ phase: PointerEvent.Phase, _ event: NSEvent, _ button: PointerEvent.Button) {
        guard bounds.width > 0, bounds.height > 0 else { return }
        let point = convert(event.locationInWindow, from: nil)
        let location = CGPoint(x: point.x / bounds.width, y: point.y / bounds.height)
        onEvent?(.pointer(PointerEvent(phase: phase, location: location, button: button)))
    }

    private static let capsLock: Int32 = 57

    /// SDL scancodes, which are USB HID usages, for macOS virtual key codes (`kVK_*` in Carbon's Events.h).
    nonisolated static func scancode(_ keyCode: UInt16) -> Int32? {
        MacKeyCodes.hidUsage(keyCode).map(Int32.init)
    }
}
#endif

extension KiriKiriHostView {
    /// Starts the game after the current layout pass, since starting creates SDL's window and draws a frame. The start
    /// runs from the run loop rather than in a main-actor task: a dialog the game shows while it starts runs the app's
    /// events in a loop of its own, and while a main-actor task is on the stack the main queue runs no other, so the
    /// app's quit, which ends the game and so closes that dialog, would wait for the dialog forever.
    static func startAfterLayout(_ start: @escaping @MainActor () -> Void) {
        RunLoop.main.perform(inModes: [.common]) { MainActor.assumeIsolated(start) }
    }
}
