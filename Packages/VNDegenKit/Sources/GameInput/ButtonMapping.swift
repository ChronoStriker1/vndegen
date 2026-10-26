import Foundation
import VNCore

/// A game's button layout: every button sends the same Xbox button, except the ones the player remapped. Only the
/// remapped buttons are stored, so a game keeps following the default layout for every button the player left alone.
nonisolated public struct ButtonMapping: Equatable, Sendable {
    public private(set) var overrides: [ControlInput: ControlAction]

    public init(_ overrides: [ControlInput: ControlAction]? = nil) {
        self.overrides = overrides ?? [:]
    }

    public func action(for input: ControlInput) -> ControlAction {
        overrides[input] ?? .gamepad(input)
    }

    public func isRemapped(_ input: ControlInput) -> Bool { overrides[input] != nil }

    public mutating func set(_ action: ControlAction, for input: ControlInput) {
        overrides[input] = action == .gamepad(input) ? nil : action
    }

    /// The value to store on `Game.buttonMapping`, nil when nothing is remapped.
    public var stored: [ControlInput: ControlAction]? { overrides.isEmpty ? nil : overrides }

    /// The keys RPG Maker games read, as VNDegen sent them before it sent gamepad buttons. Return confirms in every RPG
    /// Maker, where Z is the A button in XP. Escape and X both cancel and open the menu in all of them. Shift dashes,
    /// and Q and W turn pages. The left stick moves like the D-pad. The other buttons stay gamepad buttons.
    public static let rpgMaker: ButtonMapping = {
        var mapping = ButtonMapping()
        let keys: [ControlInput: KeyboardKey] = [
            .a: .enter, .b: .escape, .x: .shift, .y: .x, .menu: .x, .l: .q, .r: .w,
            .up: .arrowUp, .down: .arrowDown, .left: .arrowLeft, .right: .arrowRight,
            .leftStickUp: .arrowUp, .leftStickDown: .arrowDown, .leftStickLeft: .arrowLeft, .leftStickRight: .arrowRight,
        ]
        for (input, key) in keys { mapping.set(.key(key), for: input) }
        return mapping
    }()
}

nonisolated extension ControlInput {
    /// What the button does on the hub's own screens and in the VNDegen menu, whatever the game's mapping. View opens
    /// the VNDegen menu there, as Home does.
    public var navigationAction: ControlAction {
        if self == .view { return .button(.hub) }
        return virtualButton.map(ControlAction.button) ?? .nothing
    }

    /// What the button does under `mapping`, or on the hub's screens when it is nil.
    public func action(in mapping: ButtonMapping?) -> ControlAction {
        mapping?.action(for: self) ?? navigationAction
    }

    public var title: String {
        switch self {
        case .a: "A"
        case .b: "B"
        case .x: "X"
        case .y: "Y"
        case .l: "LB"
        case .r: "RB"
        case .l2: "LT"
        case .r2: "RT"
        case .view: "View"
        case .menu: "Menu"
        case .l3: "L3"
        case .r3: "R3"
        case .up: "D-pad up"
        case .down: "D-pad down"
        case .left: "D-pad left"
        case .right: "D-pad right"
        case .leftStickUp: "Left stick up"
        case .leftStickDown: "Left stick down"
        case .leftStickLeft: "Left stick left"
        case .leftStickRight: "Left stick right"
        case .rightStickUp: "Right stick up"
        case .rightStickDown: "Right stick down"
        case .rightStickLeft: "Right stick left"
        case .rightStickRight: "Right stick right"
        }
    }

    /// A label short enough for a touch gamepad badge.
    var shortTitle: String {
        switch self {
        case .up: "D↑"
        case .down: "D↓"
        case .left: "D←"
        case .right: "D→"
        case .leftStickUp: "L↑"
        case .leftStickDown: "L↓"
        case .leftStickLeft: "L←"
        case .leftStickRight: "L→"
        case .rightStickUp: "R↑"
        case .rightStickDown: "R↓"
        case .rightStickLeft: "R←"
        case .rightStickRight: "R→"
        default: title
        }
    }

    /// The touch gamepad has every button except the sticks and pressing them in.
    public var isOnTouchGamepad: Bool {
        switch self {
        case .l3, .r3, .leftStickUp, .leftStickDown, .leftStickLeft, .leftStickRight,
             .rightStickUp, .rightStickDown, .rightStickLeft, .rightStickRight: false
        default: true
        }
    }

    /// The left stick direction that matches a D-pad direction.
    var leftStick: ControlInput? {
        switch self {
        case .up: .leftStickUp
        case .down: .leftStickDown
        case .left: .leftStickLeft
        case .right: .leftStickRight
        default: nil
        }
    }
}

nonisolated extension ControlAction {
    /// The event a press or release of the button sends. Nothing sends none.
    public func event(pressed: Bool) -> ControllerEvent? {
        switch self {
        case let .button(button): .button(button, pressed: pressed)
        case let .gamepad(input): .gamepad(input, pressed: pressed)
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
        case let .gamepad(input): "Gamepad \(input.title)"
        case let .key(key): "\(key.label) key"
        case .click(.primary): "Click"
        case .click(.secondary): "Right click"
        case .nothing: "Nothing"
        }
    }

    /// The short label the touch gamepad shows on a remapped button.
    public var badge: String? {
        switch self {
        case let .gamepad(input): input.shortTitle
        case let .key(key): key.label
        case .click(.primary): "Click"
        case .click(.secondary): "R-Click"
        case .button, .nothing: nil
        }
    }

    /// Every action a button can take, in the order the mapping editor lists them.
    public static let gamepadChoices: [ControlAction] = ControlInput.allCases.map(ControlAction.gamepad)
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

    /// The button that sent this action in the old standard layout. Ren'Py gave a remapped button that button's
    /// bindings, and TyranoScript turns these buttons back into the same actions.
    var legacyInput: ControlInput? {
        switch self {
        case .up: .up
        case .down: .down
        case .left: .left
        case .right: .right
        case .confirm: .a
        case .cancel: .b
        case .dash: .x
        case .menu: .menu
        case .pageUp: .l
        case .pageDown: .r
        case .hub: nil
        }
    }
}

nonisolated extension ButtonMapping {
    /// Converts a mapping saved by a version whose buttons sent engine-neutral actions, such as Confirm, into what each
    /// action sent in the game's engine, through `legacy`, so a remapped game works the same. Back then the left stick
    /// followed the D-pad's mapping, so a remapped direction also remaps its left stick direction. Nil when nothing
    /// changes.
    static func converting(_ stored: [ControlInput: ControlAction],
                           legacy: (VirtualButton) -> ControlAction?) -> ButtonMapping? {
        func convert(_ action: ControlAction, stick: Bool) -> ControlAction {
            guard case let .button(button) = action else { return action }
            if let converted = legacy(button) { return converted }
            guard let input = button.legacyInput else { return .nothing }
            return .gamepad(stick ? input.leftStick ?? input : input)
        }
        var mapping = ButtonMapping()
        for (input, action) in stored {
            mapping.set(convert(action, stick: false), for: input)
            if let stick = input.leftStick, stored[stick] == nil { mapping.set(convert(action, stick: true), for: stick) }
        }
        return mapping.stored == stored ? nil : mapping
    }

    /// Converts the mappings of every game the library remembers, once, when the app first runs a version whose buttons
    /// send gamepad buttons. Later runs leave mappings alone, since a newer mapping can remap the D-pad without its stick.
    @MainActor public static func convertLegacyMappings(in library: LibraryStore, defaults: UserDefaults = .standard) {
        let key = "convertedButtonMappings"
        guard !defaults.bool(forKey: key) else { return }
        for game in library.allGames {
            guard let stored = game.buttonMapping else { continue }
            let plugin = library.plugin(for: game.engine)
            guard let mapping = converting(stored, legacy: { plugin?.legacyAction(for: $0) }) else { continue }
            library.update(game.id) { $0.buttonMapping = mapping.stored }
        }
        defaults.set(true, forKey: key)
    }
}

nonisolated extension KeyboardKey {
    static let enter = KeyboardKey(key: "Enter", code: "Enter", keyCode: 13)
    static let escape = KeyboardKey(key: "Escape", code: "Escape", keyCode: 27)
    static let shift = KeyboardKey(key: "Shift", code: "ShiftLeft", keyCode: 16)
    static let arrowUp = KeyboardKey(key: "ArrowUp", code: "ArrowUp", keyCode: 38)
    static let arrowDown = KeyboardKey(key: "ArrowDown", code: "ArrowDown", keyCode: 40)
    static let arrowLeft = KeyboardKey(key: "ArrowLeft", code: "ArrowLeft", keyCode: 37)
    static let arrowRight = KeyboardKey(key: "ArrowRight", code: "ArrowRight", keyCode: 39)
    static let q = KeyboardKey(key: "q", code: "KeyQ", keyCode: 81)
    static let w = KeyboardKey(key: "w", code: "KeyW", keyCode: 87)
    static let x = KeyboardKey(key: "x", code: "KeyX", keyCode: 88)

    /// The keys a button can be bound to.
    public static let choices: [KeyboardKey] = {
        let letters = (UInt8(ascii: "A")...UInt8(ascii: "Z")).map { ascii in
            let letter = String(UnicodeScalar(ascii))
            return KeyboardKey(key: letter.lowercased(), code: "Key\(letter)", keyCode: Int(ascii))
        }
        let digits = (0...9).map { KeyboardKey(key: "\($0)", code: "Digit\($0)", keyCode: 48 + $0) }
        let named = [
            arrowUp, arrowDown, arrowLeft, arrowRight, enter, escape,
            KeyboardKey(key: " ", code: "Space", keyCode: 32),
            shift,
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
