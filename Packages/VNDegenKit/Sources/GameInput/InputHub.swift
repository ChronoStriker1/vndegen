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
    /// When true, a held direction repeats its press event, for menu navigation. Games want raw presses, so they set it false.
    public var repeatsDirections = true
    public private(set) var lastInputKind: InputKind = .touch
    public private(set) var controllerNames: [String] = []

    public init() {}
    public func start() {}
}

import SwiftUI

public enum TouchControlsMode: String, CaseIterable, Codable, Sendable {
    case gamepad, mouse, hidden
}

/// On-screen controls drawn over a running game. Gamepad mode sends buttons. Mouse mode moves a virtual cursor and sends pointer events.
public struct TouchControlsOverlay: View {
    @Binding var mode: TouchControlsMode
    let send: (ControllerEvent) -> Void

    public init(mode: Binding<TouchControlsMode>, send: @escaping (ControllerEvent) -> Void) {
        self._mode = mode
        self.send = send
    }

    public var body: some View { Color.clear }
}
