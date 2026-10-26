import Foundation
import VNCore

/// A game's button layout: the standard layout plus the buttons the player remapped. Only differences from the standard
/// layout are stored, so a game keeps following the standard layout for every button the player left alone.
nonisolated public struct ButtonMapping: Equatable, Sendable {
    public private(set) var overrides: [ControlInput: ControlAction]

    public init(_ overrides: [ControlInput: ControlAction]? = nil) {
        self.overrides = overrides ?? [:]
    }

    public func action(for input: ControlInput) -> ControlAction {
        overrides[input] ?? input.standardAction
    }

    public func isRemapped(_ input: ControlInput) -> Bool { overrides[input] != nil }

    public mutating func set(_ action: ControlAction, for input: ControlInput) {
        overrides[input] = action == input.standardAction ? nil : action
    }

    /// The value to store on `Game.buttonMapping`, nil when nothing is remapped.
    public var stored: [ControlInput: ControlAction]? { overrides.isEmpty ? nil : overrides }
}

nonisolated extension ControlInput {
    /// The layout every engine maps to its own keys, and the one the hub's own screens always use.
    public var standardAction: ControlAction {
        switch self {
        case .up: .button(.up)
        case .down: .button(.down)
        case .left: .button(.left)
        case .right: .button(.right)
        case .a: .button(.confirm)
        case .b: .button(.cancel)
        case .x: .button(.dash)
        case .y: .button(.menu)
        case .l: .button(.pageUp)
        case .r: .button(.pageDown)
        case .l2, .r2: .nothing
        case .menu: .button(.menu)
        }
    }

    public var title: String {
        switch self {
        case .up: "D-pad up"
        case .down: "D-pad down"
        case .left: "D-pad left"
        case .right: "D-pad right"
        case .a: "A"
        case .b: "B"
        case .x: "X"
        case .y: "Y"
        case .l: "L"
        case .r: "R"
        case .l2: "L2"
        case .r2: "R2"
        case .menu: "Menu"
        }
    }

    /// The touch gamepad has every button except the triggers.
    public var isOnTouchGamepad: Bool { self != .l2 && self != .r2 }
}

nonisolated extension ControlAction {
    /// The event a press or release of the button sends. Nothing sends none.
    public func event(pressed: Bool) -> ControllerEvent? {
        switch self {
        case let .button(button): .button(button, pressed: pressed)
        case let .key(key): .key(key, pressed: pressed)
        case let .click(button): .click(button, pressed: pressed)
        case .nothing: nil
        }
    }

    var isDirection: Bool {
        if case let .button(button) = self { button.isDirection } else { false }
    }

    public var title: String {
        switch self {
        case let .button(button): button.title
        case let .key(key): "\(key.label) key"
        case .click(.primary): "Click"
        case .click(.secondary): "Right click"
        case .nothing: "Nothing"
        }
    }

    /// The short label the touch gamepad shows on a button bound to a key or a click.
    public var badge: String? {
        switch self {
        case let .key(key): key.label
        case .click(.primary): "Click"
        case .click(.secondary): "R-Click"
        case .button, .nothing: nil
        }
    }

    /// Every action a button can take, in the order the mapping editor lists them.
    public static let gamepadChoices: [ControlAction] = VirtualButton.allCases.filter { $0 != .hub }.map(ControlAction.button)
    public static let mouseChoices: [ControlAction] = [.click(.primary), .click(.secondary)]
    public static let keyChoices: [ControlAction] = KeyboardKey.choices.map(ControlAction.key)
}

nonisolated extension VirtualButton {
    public var title: String {
        switch self {
        case .up: "Up"
        case .down: "Down"
        case .left: "Left"
        case .right: "Right"
        case .confirm: "Confirm"
        case .cancel: "Cancel"
        case .menu: "Game menu"
        case .dash: "Dash"
        case .pageUp: "Page up"
        case .pageDown: "Page down"
        case .hub: "VNDegen menu"
        }
    }
}

nonisolated extension KeyboardKey {
    /// The keys a button can be bound to.
    public static let choices: [KeyboardKey] = {
        let letters = (UInt8(ascii: "A")...UInt8(ascii: "Z")).map { ascii in
            let letter = String(UnicodeScalar(ascii))
            return KeyboardKey(key: letter.lowercased(), code: "Key\(letter)", keyCode: Int(ascii))
        }
        let digits = (0...9).map { KeyboardKey(key: "\($0)", code: "Digit\($0)", keyCode: 48 + $0) }
        let named = [
            KeyboardKey(key: "ArrowUp", code: "ArrowUp", keyCode: 38),
            KeyboardKey(key: "ArrowDown", code: "ArrowDown", keyCode: 40),
            KeyboardKey(key: "ArrowLeft", code: "ArrowLeft", keyCode: 37),
            KeyboardKey(key: "ArrowRight", code: "ArrowRight", keyCode: 39),
            KeyboardKey(key: "Enter", code: "Enter", keyCode: 13),
            KeyboardKey(key: "Escape", code: "Escape", keyCode: 27),
            KeyboardKey(key: " ", code: "Space", keyCode: 32),
            KeyboardKey(key: "Shift", code: "ShiftLeft", keyCode: 16),
            KeyboardKey(key: "Control", code: "ControlLeft", keyCode: 17),
            KeyboardKey(key: "Alt", code: "AltLeft", keyCode: 18),
            KeyboardKey(key: "Tab", code: "Tab", keyCode: 9),
            KeyboardKey(key: "Backspace", code: "Backspace", keyCode: 8),
            KeyboardKey(key: "PageUp", code: "PageUp", keyCode: 33),
            KeyboardKey(key: "PageDown", code: "PageDown", keyCode: 34),
            KeyboardKey(key: "Home", code: "Home", keyCode: 36),
            KeyboardKey(key: "End", code: "End", keyCode: 35),
        ]
        let function = (1...12).map { KeyboardKey(key: "F\($0)", code: "F\($0)", keyCode: 111 + $0) }
        return letters + digits + named + function
    }()

    /// A label short enough for a touch gamepad button.
    public var label: String {
        switch key {
        case "ArrowUp": "↑"
        case "ArrowDown": "↓"
        case "ArrowLeft": "←"
        case "ArrowRight": "→"
        case " ": "Space"
        case "Escape": "Esc"
        case "Control": "Ctrl"
        case "Backspace": "⌫"
        case "PageUp": "PgUp"
        case "PageDown": "PgDn"
        default: key.count == 1 ? key.uppercased() : key
        }
    }
}

nonisolated extension ControlMethod {
    /// Where Settings keeps the default method.
    public static let storageKey = "controlMethod"

    /// Carries over the Touch controls setting from before control methods existed, then removes it.
    public static func migrateTouchControlsSetting(in defaults: UserDefaults = .standard) {
        let legacyKey = "touchControlsMode"
        guard let legacy = defaults.string(forKey: legacyKey) else { return }
        let migrated: ControlMethod? = switch legacy {
        case "gamepad": .gamepad
        case "mouse": .mouse(.cursor)
        case "hidden": .controller(mouse: .touch)
        default: nil
        }
        if let migrated, defaults.string(forKey: storageKey) == nil { defaults.set(migrated.rawValue, forKey: storageKey) }
        defaults.removeObject(forKey: legacyKey)
    }

    /// What touches do on a touch device under this method.
    public func touchControls(lastInput: InputKind, controllerConnected: Bool) -> TouchControlsMode {
        switch self {
        case .automatic: lastInput == .touch && !controllerConnected ? .gamepad : .direct
        case .gamepad: .gamepad
        case .mouse(.touch), .controller(mouse: .touch): .direct
        case .mouse(.cursor), .controller(mouse: .cursor): .cursor
        case .controller(mouse: nil): .off
        }
    }
}
