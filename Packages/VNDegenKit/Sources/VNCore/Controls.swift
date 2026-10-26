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

/// How a game is played on a touch device.
public enum ControlMethod: String, CaseIterable, Codable, Sendable {
    /// The touch gamepad while the last input was touch and no controller is connected.
    case automatic
    case touchGamepad
    case touchMouse
    /// No overlay, for a Bluetooth controller or a keyboard.
    case controllerOnly
}
