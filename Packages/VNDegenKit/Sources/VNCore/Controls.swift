/// A keyboard key as a web page's `KeyboardEvent` reports it, such as `z`, `KeyZ`, and 90.
public struct KeyboardKey: Hashable, Codable, Sendable {
    public var key: String
    public var code: String
    public var keyCode: Int

    public init(key: String, code: String, keyCode: Int) {
        self.key = key
        self.code = code
        self.keyCode = keyCode
    }
}

/// A button the player can remap. The touch overlay and game controllers share these, so one mapping drives both.
/// The controller's Options and Home buttons are not here: they always open the VNDegen menu.
public enum ControlInput: String, CaseIterable, Codable, CodingKeyRepresentable, Sendable {
    /// The D-pad. The left stick follows it.
    case up, down, left, right
    case a, b, x, y
    /// Shoulder buttons.
    case l, r
    /// Triggers, on controllers only.
    case l2, r2
    case menu
}

/// What a remappable button does in a game.
public enum ControlAction: Hashable, Codable, Sendable {
    case button(VirtualButton)
    case key(KeyboardKey)
    case click(PointerEvent.Button)
    case nothing
}

/// How touches work as a mouse.
public enum MouseStyle: String, CaseIterable, Codable, Sendable {
    /// Touches reach the game where they land.
    case touch
    /// Dragging moves a cursor, like a trackpad.
    case cursor
}

/// How a game is played on a touch device.
public enum ControlMethod: Hashable, Sendable {
    /// The touch gamepad while the last input was touch and no controller is connected.
    case automatic
    case gamepad
    case mouse(MouseStyle)
    /// No on-screen gamepad, for a Bluetooth controller or a keyboard. The game ignores touches unless `mouse` is set.
    case controller(mouse: MouseStyle?)
}

extension ControlMethod: CaseIterable, RawRepresentable, Codable {
    public static let allCases: [ControlMethod] = [
        .automatic, .gamepad, .mouse(.touch), .mouse(.cursor),
        .controller(mouse: nil), .controller(mouse: .touch), .controller(mouse: .cursor),
    ]

    public init?(rawValue: String) {
        guard let method = Self.allCases.first(where: { $0.rawValue == rawValue }) else { return nil }
        self = method
    }

    /// `touchMouse` and `controllerOnly` predate mouse styles, so they keep what they did then: a cursor, and a controller
    /// with touches reaching the game.
    public var rawValue: String {
        switch self {
        case .automatic: "automatic"
        case .gamepad: "touchGamepad"
        case .mouse(.touch): "touch"
        case .mouse(.cursor): "touchMouse"
        case .controller(mouse: nil): "controller"
        case .controller(mouse: .touch): "controllerOnly"
        case .controller(mouse: .cursor): "controllerCursor"
        }
    }
}
