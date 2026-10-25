import GameController
import VNCore

/// Gamepad buttons the hub maps. Directions come from the D-pad and the left stick.
nonisolated public enum GamepadElement: String, CaseIterable, Sendable {
    case a, b, x, y, leftShoulder, rightShoulder, menu, options, home

    func input(on pad: GCExtendedGamepad) -> GCControllerButtonInput? {
        switch self {
        case .a: pad.buttonA
        case .b: pad.buttonB
        case .x: pad.buttonX
        case .y: pad.buttonY
        case .leftShoulder: pad.leftShoulder
        case .rightShoulder: pad.rightShoulder
        case .menu: pad.buttonMenu
        case .options: pad.buttonOptions
        case .home: pad.buttonHome
        }
    }
}

nonisolated public enum InputMapping {
    public static let gamepad: [GamepadElement: VirtualButton] = [
        .a: .confirm,
        .b: .cancel,
        .x: .dash,
        .y: .menu,
        .leftShoulder: .pageUp,
        .rightShoulder: .pageDown,
        .menu: .menu,
        .options: .hub,
        .home: .hub,
    ]

    /// Opens the hub overlay even while a game reads the keyboard itself. Games almost never bind the backtick,
    /// and unlike F1 it exists on keyboards without a function row, such as most iPad keyboards.
    public static let hubKey: GCKeyCode = .graveAccentAndTilde

    /// W is taken by WASD, so the page keys are Q and E, plus the real Page Up and Page Down keys.
    public static let keyboard: [GCKeyCode: VirtualButton] = [
        .upArrow: .up, .keyW: .up,
        .downArrow: .down, .keyS: .down,
        .leftArrow: .left, .keyA: .left,
        .rightArrow: .right, .keyD: .right,
        .returnOrEnter: .confirm, .keypadEnter: .confirm, .spacebar: .confirm,
        .escape: .cancel, .deleteOrBackspace: .cancel,
        .tab: .menu,
        .keyQ: .pageUp, .pageUp: .pageUp,
        .keyE: .pageDown, .pageDown: .pageDown,
        hubKey: .hub,
    ]
}
