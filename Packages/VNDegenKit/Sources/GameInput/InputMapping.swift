import GameController
import VNCore

/// Controller buttons the hub reads. The D-pad and the sticks are read as directions.
nonisolated public enum GamepadElement: String, CaseIterable, Sendable {
    case a, b, x, y, leftShoulder, rightShoulder, leftTrigger, rightTrigger, options, menu
    case leftThumbstickButton, rightThumbstickButton, home

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
        case .options: pad.buttonOptions
        case .menu: pad.buttonMenu
        case .leftThumbstickButton: pad.leftThumbstickButton
        case .rightThumbstickButton: pad.rightThumbstickButton
        case .home: pad.buttonHome
        }
    }

    /// The remappable button this element is. Options is Xbox's View button. Home has none: it always opens the
    /// VNDegen menu, so a mapping can never lock the player out of it.
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
        case .options: .view
        case .menu: .menu
        case .leftThumbstickButton: .l3
        case .rightThumbstickButton: .r3
        case .home: nil
        }
    }

    /// What the element does under `mapping`, or on the hub's screens when it is nil.
    public func action(in mapping: ButtonMapping?) -> ControlAction {
        control?.action(in: mapping) ?? .button(.hub)
    }
}

/// A controller's analog stick, read as four directions.
nonisolated enum Stick: CaseIterable, Sendable {
    case left, right

    func input(on pad: GCExtendedGamepad) -> GCControllerDirectionPad {
        self == .left ? pad.leftThumbstick : pad.rightThumbstick
    }

    var up: ControlInput { self == .left ? .leftStickUp : .rightStickUp }
    var down: ControlInput { self == .left ? .leftStickDown : .rightStickDown }
    var left: ControlInput { self == .left ? .leftStickLeft : .rightStickLeft }
    var right: ControlInput { self == .left ? .leftStickRight : .rightStickRight }
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
