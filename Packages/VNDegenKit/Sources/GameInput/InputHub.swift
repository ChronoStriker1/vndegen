import Foundation
import Observation
import VNCore

public enum InputKind: Sendable { case touch, keyboardMouse, gamepad }

/// Merges game controllers, and the hardware keyboard while in the hub, into `ControllerEvent`s.
@MainActor
@Observable
public final class InputHub {
    /// The current consumer: hub navigation or the running game. Set by whoever owns the screen.
    @ObservationIgnored public var onEvent: ((ControllerEvent) -> Void)?
    /// When false, keyboard keys are left to the focused view (a running web game reads them itself).
    public var translatesKeyboard = true
    public private(set) var lastInputKind: InputKind = .touch
    public private(set) var controllerNames: [String] = []

    public init() {}
    public func start() {}
}
