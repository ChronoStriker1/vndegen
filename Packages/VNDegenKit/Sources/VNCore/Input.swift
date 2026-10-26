import CoreGraphics

/// Engine-neutral controls. The hub's screens navigate with these, and engines without gamepad support map each one to
/// their own keys (RPG Maker: confirm is Enter, cancel is Escape, and so on).
public enum VirtualButton: String, CaseIterable, Codable, Sendable {
    case up, down, left, right
    case confirm, cancel, menu, dash
    case pageUp, pageDown
    /// Opens the VNDegen in-game overlay (resume, quit to hub). Never forwarded to engines.
    case hub
}

public struct PointerEvent: Sendable, Equatable {
    public enum Phase: Sendable { case began, moved, ended, cancelled }
    public enum Button: String, Codable, Sendable { case primary, secondary }

    public var phase: Phase
    /// Position in the session view, normalized so (0, 0) is top left and (1, 1) is bottom right.
    public var location: CGPoint
    public var button: Button

    public init(phase: Phase, location: CGPoint, button: Button = .primary) {
        self.phase = phase
        self.location = location
        self.button = button
    }
}

public enum ControllerEvent: Sendable, Equatable {
    case button(VirtualButton, pressed: Bool)
    case pointer(PointerEvent)
    case scroll(dy: Double)
    /// A keyboard key the player bound to a button, delivered to the engine as that key.
    case key(KeyboardKey, pressed: Bool)
    /// A mouse button the player bound to a button. It clicks wherever the pointer last was.
    case click(PointerEvent.Button, pressed: Bool)
    /// An Xbox controller button, which games that read a gamepad see as one. Engines without gamepad support turn it
    /// into their own keys, through `ControlInput.virtualButton`.
    case gamepad(ControlInput, pressed: Bool)
}
