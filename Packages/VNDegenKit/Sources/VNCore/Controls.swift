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

/// A button of an Xbox controller. Each is a button the player can remap, from the touch gamepad or a real controller,
/// and each is also what a remapped button can send, as games reading a standard gamepad see it.
/// The controller's Home (Guide) button is not here: it always opens the VNDegen menu.
public enum ControlInput: String, CaseIterable, Codable, CodingKeyRepresentable, Sendable {
    case a, b, x, y
    /// LB and RB.
    case l, r
    /// LT and RT.
    case l2, r2
    case view, menu
    /// Pressing the left and right sticks in, on controllers only.
    case l3, r3
    /// The D-pad.
    case up, down, left, right
    /// The sticks' directions, on controllers only.
    case leftStickUp, leftStickDown, leftStickLeft, leftStickRight
    case rightStickUp, rightStickDown, rightStickLeft, rightStickRight

    /// What the button does in an engine that reads no gamepad and turns VNDegen's engine-neutral actions into its own
    /// keys. Nil for buttons such engines leave unused.
    public var virtualButton: VirtualButton? {
        switch self {
        case .a: .confirm
        case .b: .cancel
        case .x: .dash
        case .y, .menu: .menu
        case .l: .pageUp
        case .r: .pageDown
        case .up, .leftStickUp: .up
        case .down, .leftStickDown: .down
        case .left, .leftStickLeft: .left
        case .right, .leftStickRight: .right
        case .l2, .r2, .view, .l3, .r3, .rightStickUp, .rightStickDown, .rightStickLeft, .rightStickRight: nil
        }
    }
}

/// What a remappable button does in a game.
public enum ControlAction: Hashable, Codable, Sendable {
    /// What buttons sent before they sent gamepad buttons. Only remaps saved by those versions hold it.
    case button(VirtualButton)
    case key(KeyboardKey)
    case click(PointerEvent.Button)
    case nothing
    case gamepad(ControlInput)
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
