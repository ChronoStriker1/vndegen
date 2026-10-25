import CoreGraphics

/// Engine-neutral controls. Gamepads, the touch overlay, and hub navigation all speak this vocabulary.
/// Engines map each button to their own keys (RPG Maker: confirm is Enter, cancel is Escape, and so on).
public enum VirtualButton: String, CaseIterable, Codable, Sendable {
    case up, down, left, right
    case confirm, cancel, menu, dash
    case pageUp, pageDown
    /// Opens the VNDegen in-game overlay (resume, quit to hub). Never forwarded to engines.
    case hub
}

public struct PointerEvent: Sendable, Equatable {
    public enum Phase: Sendable { case began, moved, ended, cancelled }
    public enum Button: Sendable { case primary, secondary }

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
}
