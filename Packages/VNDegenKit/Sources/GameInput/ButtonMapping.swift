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

    /// A game's mapping as `Game.buttonMapping` stores it, where nil follows the engine's default layout.
    public init(stored: [ControlInput: ControlAction]?, default layout: ButtonMapping) {
        self = stored.map { ButtonMapping($0) } ?? layout
    }

    /// The value to store on `Game.buttonMapping` for a game whose engine defaults to `layout`: nil while the mapping is
    /// that layout, so the game keeps following it, and otherwise every remap, even none.
    public func stored(default layout: ButtonMapping) -> [ControlInput: ControlAction]? {
        self == layout ? nil : overrides
    }

    /// How many buttons do something other than in `other`.
    public func changes(from other: ButtonMapping) -> Int {
        ControlInput.allCases.count { action(for: $0) != other.action(for: $0) }
    }

    /// Whether every direction of `stick` moves the pointer the same way.
    public func movesPointer(_ stick: Stick) -> Bool {
        stick.directions.allSatisfy { action(for: $0) == .mouseMove($0.pointerDirection!) }
    }

    /// Makes `stick` move the pointer, or turns it back into the controller's stick.
    public mutating func setStick(_ stick: Stick, movesPointer: Bool) {
        for direction in stick.directions {
            set(movesPointer ? .mouseMove(direction.pointerDirection!) : .gamepad(direction), for: direction)
        }
    }

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

    /// Steam's Keyboard and Mouse controller template, which engines with no gamepad support start with. LB and RB turn
    /// the wheel down and up, LT right clicks and RT clicks, View is Tab and Menu Escape, the left stick is WASD and
    /// pressing it is Shift, the right stick moves the pointer and pressing it clicks, the D-pad is 1 to 4, and A, B, X,
    /// and Y are Space, E, R, and F.
    public static let keyboardAndMouse: ButtonMapping = {
        var mapping = ButtonMapping()
        let actions: [ControlInput: ControlAction] = [
            .l: .wheel(.down), .r: .wheel(.up), .l2: .click(.secondary), .r2: .click(.primary),
            .view: .key(.tab), .menu: .key(.escape), .l3: .key(.shift), .r3: .click(.primary),
            .leftStickUp: .key(.letter("W")), .leftStickDown: .key(.letter("S")),
            .leftStickLeft: .key(.letter("A")), .leftStickRight: .key(.letter("D")),
            .up: .key(.digit(1)), .down: .key(.digit(2)), .left: .key(.digit(3)), .right: .key(.digit(4)),
            .a: .key(.space), .b: .key(.letter("E")), .x: .key(.letter("R")), .y: .key(.letter("F")),
        ]
        for (input, action) in actions { mapping.set(action, for: input) }
        mapping.setStick(.right, movesPointer: true)
        return mapping
    }()

    /// A layout for playing with the pointer: A and RT click, X and LT right click, LB turns the wheel down and RB up, B
    /// and Menu press Escape, Y shows the on-screen keyboard, and both sticks move the pointer. The D-pad, View, and
    /// pressing the sticks in stay gamepad buttons.
    public static let mouse: ButtonMapping = {
        var mapping = ButtonMapping()
        let actions: [ControlInput: ControlAction] = [
            .a: .click(.primary), .r2: .click(.primary), .x: .click(.secondary), .l2: .click(.secondary),
            .l: .wheel(.down), .r: .wheel(.up), .b: .key(.escape), .menu: .key(.escape), .y: .showKeyboard,
        ]
        for (input, action) in actions { mapping.set(action, for: input) }
        for stick in Stick.allCases { mapping.setStick(stick, movesPointer: true) }
        return mapping
    }()

    /// The mapping a game follows until the player changes it, from its engine.
    @MainActor public static func defaultLayout(for game: Game, in library: LibraryStore) -> ButtonMapping {
        (library.plugin(for: game.engine)?.defaultControls ?? .gamepad).mapping
    }

    /// The mapping a game plays with.
    @MainActor public init(for game: Game, in library: LibraryStore) {
        self.init(stored: game.buttonMapping, default: Self.defaultLayout(for: game, in: library))
    }
}

nonisolated extension ControlLayout {
    public var mapping: ButtonMapping {
        switch self {
        case .gamepad: ButtonMapping()
        case .keyboardAndMouse: .keyboardAndMouse
        }
    }
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

    /// The pointer direction of a D-pad or stick direction.
    var pointerDirection: PointerDirection? {
        switch self {
        case .up, .leftStickUp, .rightStickUp: .up
        case .down, .leftStickDown, .rightStickDown: .down
        case .left, .leftStickLeft, .rightStickLeft: .left
        case .right, .leftStickRight, .rightStickRight: .right
        default: nil
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
        // `VirtualMouse` does these itself.
        case .nothing, .mouseMove, .wheel, .showKeyboard: nil
        }
    }

    /// Whether `VirtualMouse` does this action rather than the game receiving it as a press.
    var isVirtualMouse: Bool {
        switch self {
        case .mouseMove, .wheel, .showKeyboard: true
        case .button, .key, .click, .nothing, .gamepad: false
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
        case .click(.primary): "Left click"
        case .click(.secondary): "Right click"
        case .nothing: "Nothing"
        case .mouseMove(.up): "Mouse up"
        case .mouseMove(.down): "Mouse down"
        case .mouseMove(.left): "Mouse left"
        case .mouseMove(.right): "Mouse right"
        case .wheel(.up): "Scroll wheel up"
        case .wheel(.down): "Scroll wheel down"
        case .showKeyboard: "Show keyboard"
        }
    }

    /// The short label the touch gamepad shows on a remapped button.
    public var badge: String? {
        switch self {
        case let .gamepad(input): input.shortTitle
        case let .key(key): key.label
        case .click(.primary): "Left click"
        case .click(.secondary): "Right click"
        case .mouseMove(.up): "Mouse ↑"
        case .mouseMove(.down): "Mouse ↓"
        case .mouseMove(.left): "Mouse ←"
        case .mouseMove(.right): "Mouse →"
        case .wheel(.up): "Wheel ↑"
        case .wheel(.down): "Wheel ↓"
        case .showKeyboard: "Keyboard"
        case .button, .nothing: nil
        }
    }

    /// Every action a button can take, in the order the mapping editor lists them.
    public static let gamepadChoices: [ControlAction] = ControlInput.allCases.map(ControlAction.gamepad)
    public static let mouseChoices: [ControlAction] = [.click(.primary), .click(.secondary), .wheel(.up), .wheel(.down)]
        + PointerDirection.allCases.map(ControlAction.mouseMove)
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
            // Kept even when empty: the game had a mapping, so it stays on the Xbox layout rather than its engine's default.
            library.update(game.id) { $0.buttonMapping = mapping.overrides }
        }
        defaults.set(true, forKey: key)
    }
}

nonisolated extension KeyboardKey {
    static let enter = KeyboardKey(key: "Enter", code: "Enter", keyCode: 13)
    static let escape = KeyboardKey(key: "Escape", code: "Escape", keyCode: 27)
    static let backspace = KeyboardKey(key: "Backspace", code: "Backspace", keyCode: 8)
    static let shift = KeyboardKey(key: "Shift", code: "ShiftLeft", keyCode: 16)
    static let tab = KeyboardKey(key: "Tab", code: "Tab", keyCode: 9)
    static let space = KeyboardKey(key: " ", code: "Space", keyCode: 32)
    static let arrowUp = KeyboardKey(key: "ArrowUp", code: "ArrowUp", keyCode: 38)
    static let arrowDown = KeyboardKey(key: "ArrowDown", code: "ArrowDown", keyCode: 40)
    static let arrowLeft = KeyboardKey(key: "ArrowLeft", code: "ArrowLeft", keyCode: 37)
    static let arrowRight = KeyboardKey(key: "ArrowRight", code: "ArrowRight", keyCode: 39)
    static let q = letter("Q")
    static let w = letter("W")
    static let x = letter("X")

    /// A letter key, from its capital.
    static func letter(_ capital: Character) -> KeyboardKey {
        KeyboardKey(key: capital.lowercased(), code: "Key\(capital)", keyCode: Int(capital.asciiValue!))
    }

    static func digit(_ digit: Int) -> KeyboardKey {
        KeyboardKey(key: "\(digit)", code: "Digit\(digit)", keyCode: 48 + digit)
    }

    /// The keys a button can be bound to.
    public static let choices: [KeyboardKey] = {
        let letters = (UInt8(ascii: "A")...UInt8(ascii: "Z")).map { letter(Character(UnicodeScalar($0))) }
        let digits = (0...9).map(digit)
        let named = [
            arrowUp, arrowDown, arrowLeft, arrowRight, enter, escape, space, shift,
            KeyboardKey(key: "Control", code: "ControlLeft", keyCode: 17),
            KeyboardKey(key: "Alt", code: "AltLeft", keyCode: 18),
            tab,
            backspace,
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
