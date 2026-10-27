import CGodot
import CoreGraphics
import VNCore

/// Input the engine receives. Keys are USB HID usages, points are normalized over the view the game draws in, and the
/// controller is the Xbox controller `vngd.h` presents.
nonisolated enum GodotInput: Equatable, Sendable {
    /// A key, with the character it types going down, or 0.
    case key(Int, down: Bool, character: UInt32 = 0)
    case text(String)
    case mouseMove(to: CGPoint)
    /// 1 left, 2 right.
    case mouseButton(Int32, down: Bool)
    /// Notches, positive turned away from the player.
    case wheel(Double)
    case joyButton(Int32, down: Bool)
    case joyAxis(Int32, Float)
}

extension GodotEngine {
    func send(_ input: GodotInput) {
        switch input {
        case let .key(usage, down, character): key(Int32(usage), down, character)
        case let .text(string): string.withCString(text)
        case let .mouseMove(to): mouseMove(Float(to.x), Float(to.y))
        case let .mouseButton(button, down): mouseButton(button, down)
        case let .wheel(notches): wheel(Float(notches))
        case let .joyButton(button, down): joyButton(button, down)
        case let .joyAxis(axis, value): joyAxis(axis, value)
        }
    }
}

/// USB HID keyboard usages for the keys VNDegen sends.
nonisolated enum HIDKey {
    static let `return` = 40, escape = 41, backspace = 42, tab = 43, space = 44
    static let right = 79, left = 80, down = 81, up = 82
    static let pageUp = 75, pageDown = 78, leftShift = 225

    /// The usage for a key's web `code`, such as `KeyZ` or `ArrowUp`. Nil for a key VNDegen doesn't map.
    static func usage(code: String) -> Int? {
        if code.hasPrefix("Key"), code.count == 4, let letter = code.last?.asciiValue, (65...90).contains(letter) {
            return 4 + Int(letter - 65)
        }
        if code.hasPrefix("Digit"), code.count == 6, let digit = code.last?.wholeNumberValue {
            return digit == 0 ? 39 : 29 + digit
        }
        if code.hasPrefix("Numpad"), code.count == 7, let digit = code.last?.wholeNumberValue {
            return digit == 0 ? 98 : 88 + digit
        }
        if code.hasPrefix("F"), let number = Int(code.dropFirst()), (1...12).contains(number) { return 57 + number }
        return named[code]
    }

    private static let named: [String: Int] = [
        "Enter": 40, "Escape": 41, "Backspace": 42, "Tab": 43, "Space": 44, "Minus": 45, "Equal": 46,
        "BracketLeft": 47, "BracketRight": 48, "Backslash": 49, "Semicolon": 51, "Quote": 52, "Backquote": 53,
        "Comma": 54, "Period": 55, "Slash": 56, "CapsLock": 57, "Insert": 73, "Home": 74, "PageUp": 75, "Delete": 76,
        "End": 77, "PageDown": 78, "ArrowRight": 79, "ArrowLeft": 80, "ArrowDown": 81, "ArrowUp": 82,
        "NumpadEnter": 88, "ControlLeft": 224, "ShiftLeft": 225, "AltLeft": 226, "MetaLeft": 227,
        "ControlRight": 228, "ShiftRight": 229, "AltRight": 230, "MetaRight": 231,
    ]
}

nonisolated extension VirtualButton {
    /// The key for each of VNDegen's buttons, as a keyboard player would press it in a Godot game: Godot's default
    /// `ui_accept` is Enter and Space, `ui_cancel` is Escape.
    var hidUsage: Int? {
        switch self {
        case .up: HIDKey.up
        case .down: HIDKey.down
        case .left: HIDKey.left
        case .right: HIDKey.right
        case .confirm: HIDKey.return
        case .cancel, .menu: HIDKey.escape
        case .dash: HIDKey.leftShift
        case .pageUp: HIDKey.pageUp
        case .pageDown: HIDKey.pageDown
        case .hub: nil
        }
    }
}

/// Turns VNDegen's controller events into engine input. Several buttons can hold the same key or controller button,
/// such as Confirm and a button remapped to Return, so it goes down with the first press and up with the last release.
nonisolated struct GodotInputRouter {
    /// Scroll distance in points that makes one wheel notch.
    static let pointsPerNotch = 40.0

    private enum Held: Hashable {
        case key(Int)
        case button(Int32)
        /// A stick direction or trigger, as axis and sign.
        case axis(Int32, Float)
    }

    private var held: [Held: Int] = [:]
    private var pointer = CGPoint(x: 0.5, y: 0.5)

    mutating func translate(_ event: ControllerEvent) -> [GodotInput] {
        switch event {
        case let .button(button, pressed):
            return button.hidUsage.map { hold(.key($0), pressed) } ?? []
        case let .key(key, pressed):
            return HIDKey.usage(code: key.code).map { hold(.key($0), pressed) } ?? []
        case let .gamepad(input, pressed):
            return hold(Self.held(for: input), pressed)
        case let .pointer(pointer):
            self.pointer = pointer.location
            let button: Int32 = pointer.button == .primary ? 1 : 2
            switch pointer.phase {
            case .began: return [.mouseMove(to: pointer.location), .mouseButton(button, down: true)]
            case .moved: return [.mouseMove(to: pointer.location)]
            case .ended, .cancelled: return [.mouseMove(to: pointer.location), .mouseButton(button, down: false)]
            }
        case let .click(button, pressed):
            return [.mouseMove(to: pointer), .mouseButton(button == .primary ? 1 : 2, down: pressed)]
        case let .scroll(dy):
            // VNDegen's scroll is positive for Wheel Down, and Godot's wheel positive for up.
            return [.wheel(-dy / Self.pointsPerNotch)]
        }
    }

    /// A key from the hardware keyboard, with the text it types, which a text field in the game inserts.
    mutating func hostKey(_ usage: Int, pressed: Bool, text: String? = nil) -> [GodotInput] {
        hold(.key(usage), pressed).map { input in
            guard case let .key(usage, true, _) = input else { return input }
            return .key(usage, down: true, character: Self.character(text))
        }
    }

    /// The one printable character `text` holds, or 0 for none, a control character, or the private-use characters
    /// AppKit gives function and arrow keys.
    static func character(_ text: String?) -> UInt32 {
        guard let scalars = text?.unicodeScalars, scalars.count == 1, let scalar = scalars.first,
              scalar.value >= 0x20, scalar.value != 0x7F, !(0xF700...0xF8FF).contains(scalar.value) else { return 0 }
        return scalar.value
    }

    func type(_ typed: TypedText) -> [GodotInput] {
        switch typed {
        case let .text(text): [.text(text)]
        case .newline: [.key(HIDKey.return, down: true), .key(HIDKey.return, down: false)]
        case .deleteBackward: [.key(HIDKey.backspace, down: true), .key(HIDKey.backspace, down: false)]
        }
    }

    /// Releases everything held, such as when the hub's menu takes the keyboard and the controller.
    mutating func releaseAll() -> [GodotInput] {
        let items = held.keys
        held = [:]
        return items.flatMap { input(for: $0, down: false) }
    }

    private static func held(for input: ControlInput) -> Held {
        switch input {
        case .a: .button(Int32(VNGD_JOY_A))
        case .b: .button(Int32(VNGD_JOY_B))
        case .x: .button(Int32(VNGD_JOY_X))
        case .y: .button(Int32(VNGD_JOY_Y))
        case .l: .button(Int32(VNGD_JOY_LEFT_SHOULDER))
        case .r: .button(Int32(VNGD_JOY_RIGHT_SHOULDER))
        case .l2: .axis(Int32(VNGD_AXIS_TRIGGER_LEFT), 1)
        case .r2: .axis(Int32(VNGD_AXIS_TRIGGER_RIGHT), 1)
        case .view: .button(Int32(VNGD_JOY_BACK))
        case .menu: .button(Int32(VNGD_JOY_START))
        case .l3: .button(Int32(VNGD_JOY_LEFT_STICK))
        case .r3: .button(Int32(VNGD_JOY_RIGHT_STICK))
        case .up: .button(Int32(VNGD_JOY_DPAD_UP))
        case .down: .button(Int32(VNGD_JOY_DPAD_DOWN))
        case .left: .button(Int32(VNGD_JOY_DPAD_LEFT))
        case .right: .button(Int32(VNGD_JOY_DPAD_RIGHT))
        case .leftStickUp: .axis(Int32(VNGD_AXIS_LEFT_Y), -1)
        case .leftStickDown: .axis(Int32(VNGD_AXIS_LEFT_Y), 1)
        case .leftStickLeft: .axis(Int32(VNGD_AXIS_LEFT_X), -1)
        case .leftStickRight: .axis(Int32(VNGD_AXIS_LEFT_X), 1)
        case .rightStickUp: .axis(Int32(VNGD_AXIS_RIGHT_Y), -1)
        case .rightStickDown: .axis(Int32(VNGD_AXIS_RIGHT_Y), 1)
        case .rightStickLeft: .axis(Int32(VNGD_AXIS_RIGHT_X), -1)
        case .rightStickRight: .axis(Int32(VNGD_AXIS_RIGHT_X), 1)
        }
    }

    /// The input for `item` going down or up, after `held` has changed.
    private func input(for item: Held, down: Bool) -> [GodotInput] {
        switch item {
        case let .key(usage): [.key(usage, down: down)]
        case let .button(button): [.joyButton(button, down: down)]
        case let .axis(axis, _): [.joyAxis(axis, value(of: axis))]
        }
    }

    /// The sum of the axis's directions held, so opposite directions cancel and releasing one leaves the other.
    private func value(of axis: Int32) -> Float {
        var value: Float = 0
        for case let .axis(other, sign) in held.keys where other == axis {
            value += sign
        }
        return value
    }

    private mutating func hold(_ item: Held, _ pressed: Bool) -> [GodotInput] {
        let count = held[item, default: 0]
        if pressed {
            held[item] = count + 1
            return count == 0 ? input(for: item, down: true) : []
        }
        guard count > 0 else { return [] }
        held[item] = count == 1 ? nil : count - 1
        return count == 1 ? input(for: item, down: false) : []
    }
}
