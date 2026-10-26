import GameController
import VNCore

/// Gamepad buttons the hub reads. Directions come from the D-pad and the left stick.
nonisolated public enum GamepadElement: String, CaseIterable, Sendable {
    case a, b, x, y, leftShoulder, rightShoulder, leftTrigger, rightTrigger, menu, options, home

    func input(on pad: GCExtendedGamepad) -> GCControllerButtonInput? {
        switch self {
        case .a: pad.buttonA
        case .b: pad.buttonB
        case .x: pad.buttonX
        case .y: pad.buttonY
        case .leftShoulder: pad.leftShoulder
        case .rightShoulder: pad.rightShoulder
        case .leftTrigger: pad.leftTrigger
        case .rightTrigger: pad.rightTrigger
        case .menu: pad.buttonMenu
        case .options: pad.buttonOptions
        case .home: pad.buttonHome
        }
    }

    /// The remappable button this element is. Options and Home have none: they always open the VNDegen menu, so a
    /// mapping can never lock the player out of it.
    public var control: ControlInput? {
        switch self {
        case .a: .a
        case .b: .b
        case .x: .x
        case .y: .y
        case .leftShoulder: .l
        case .rightShoulder: .r
        case .leftTrigger: .l2
        case .rightTrigger: .r2
        case .menu: .menu
        case .options, .home: nil
        }
    }

    /// What the element does under `mapping`.
    public func action(in mapping: ButtonMapping) -> ControlAction {
        control.map(mapping.action) ?? .button(.hub)
    }
}

nonisolated public enum InputMapping {
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
