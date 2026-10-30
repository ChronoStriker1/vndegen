import CoreGraphics
import VNCore

/// Input the engine receives. Keys and mouse buttons are Windows virtual-key codes, as Artemis games bind them. Points
/// are normalized over the view the game draws in.
nonisolated enum ArtemisInput: Equatable, Sendable {
    case key(Int32, down: Bool)
    case mouseButton(Int32, down: Bool, at: CGPoint)
    case mouseMove(to: CGPoint)
    /// A mouse button pressed wherever the pointer was last.
    case click(Int32, down: Bool)
}

nonisolated enum VirtualKey {
    static let leftButton: Int32 = 1
    static let rightButton: Int32 = 2
    static let backspace: Int32 = 8
    static let tab: Int32 = 9
    static let `return`: Int32 = 13
    static let shift: Int32 = 16
    static let control: Int32 = 17
    static let alt: Int32 = 18
    static let escape: Int32 = 27
    static let space: Int32 = 32
    static let pageUp: Int32 = 33
    static let pageDown: Int32 = 34
    static let end: Int32 = 35
    static let home: Int32 = 36
    static let left: Int32 = 37
    static let up: Int32 = 38
    static let right: Int32 = 39
    static let down: Int32 = 40
    static let insert: Int32 = 45
    static let delete: Int32 = 46
    /// The engine's codes for the wheel turned away from and toward the player, which games bind as keys.
    static let wheelUp: Int32 = 136
    static let wheelDown: Int32 = 137

    static func mouse(_ button: PointerEvent.Button) -> Int32 { button == .primary ? leftButton : rightButton }

    /// The virtual-key code for a USB HID usage, the key code UIKit reports and VNDegen's macOS key table gives. Nil
    /// for a key Artemis games don't bind.
    static func forUsage(_ usage: Int) -> Int32? {
        switch usage {
        case 4...29: Int32(65 + usage - 4) // A to Z
        case 30...38: Int32(49 + usage - 30) // 1 to 9
        case 39: 48 // 0
        case 40, 88: `return` // Return, keypad Enter
        case 41: escape
        case 42: backspace
        case 43: tab
        case 44: space
        case 58...69: Int32(112 + usage - 58) // F1 to F12
        case 73: insert
        case 74: home
        case 75: pageUp
        case 76: delete
        case 77: end
        case 78: pageDown
        case 79: right
        case 80: left
        case 81: down
        case 82: up
        case 89...97: Int32(97 + usage - 89) // keypad 1 to 9
        case 98: 96 // keypad 0
        case 84: 111 // keypad /
        case 85: 106 // keypad *
        case 86: 109 // keypad -
        case 87: 107 // keypad +
        case 99: 110 // keypad .
        case 224, 228: control
        case 225, 229: shift
        case 226, 230: alt
        default: nil
        }
    }
}

nonisolated extension VirtualButton {
    /// The keys Artemis games bind. Return reads on and presses the focused button, and the arrows move that focus.
    /// Escape backs out of menus, and the right click opens the game's menu. Holding Control skips. The wheel turned
    /// away opens the message history, and turned toward the player reads on.
    var artemisKey: Int32? {
        switch self {
        case .up: VirtualKey.up
        case .down: VirtualKey.down
        case .left: VirtualKey.left
        case .right: VirtualKey.right
        case .confirm: VirtualKey.return
        case .cancel: VirtualKey.escape
        case .menu: VirtualKey.rightButton
        case .dash: VirtualKey.control
        case .pageUp: VirtualKey.wheelUp
        case .pageDown: VirtualKey.wheelDown
        case .hub: nil
        }
    }
}

nonisolated extension KeyboardKey {
    /// The virtual-key code for the key's `code`, such as `KeyA` or `ArrowUp`. Nil for a key the engine has no use for.
    var virtualKey: Int32? {
        if code.hasPrefix("Key"), code.count == 4, let letter = code.unicodeScalars.last, ("A"..."Z").contains(letter) {
            return Int32(letter.value)
        }
        if code.hasPrefix("Digit"), code.count == 6, let digit = Int32(code.suffix(1)) { return 48 + digit }
        if code.hasPrefix("F"), let number = Int32(code.dropFirst()), (1...12).contains(number) { return 111 + number }
        return Self.namedVirtualKeys[code]
    }

    private static let namedVirtualKeys: [String: Int32] = [
        "Enter": VirtualKey.return, "Escape": VirtualKey.escape, "Backspace": VirtualKey.backspace, "Tab": VirtualKey.tab,
        "Space": VirtualKey.space, "Home": VirtualKey.home, "PageUp": VirtualKey.pageUp, "End": VirtualKey.end,
        "PageDown": VirtualKey.pageDown, "Insert": VirtualKey.insert, "Delete": VirtualKey.delete,
        "ArrowRight": VirtualKey.right, "ArrowLeft": VirtualKey.left, "ArrowDown": VirtualKey.down, "ArrowUp": VirtualKey.up,
        "ControlLeft": VirtualKey.control, "ControlRight": VirtualKey.control, "ShiftLeft": VirtualKey.shift,
        "ShiftRight": VirtualKey.shift, "AltLeft": VirtualKey.alt, "AltRight": VirtualKey.alt,
    ]
}

/// Turns VNDegen's controller events and the keyboard and mouse the session view reads itself into engine input.
/// Several buttons can hold the same key, such as Confirm and a button remapped to Return, so it goes down with the
/// first press and up with the last release.
nonisolated struct ArtemisInputRouter {
    /// Scroll distance in points that makes one wheel notch.
    static let pointsPerNotch = 40.0

    private var held: [Int32: Int] = [:]
    private var scrolled = 0.0

    mutating func translate(_ event: ControllerEvent) -> [ArtemisInput] {
        switch event {
        case let .button(button, pressed):
            return button.artemisKey.map { hold($0, pressed) } ?? []
        case let .gamepad(input, pressed):
            return input.virtualButton?.artemisKey.map { hold($0, pressed) } ?? []
        case let .key(key, pressed):
            return key.virtualKey.map { hold($0, pressed) } ?? []
        case let .click(button, pressed):
            return hold(VirtualKey.mouse(button), pressed)
        case let .pointer(pointer):
            let button = VirtualKey.mouse(pointer.button)
            switch pointer.phase {
            case .began: return [.mouseMove(to: pointer.location), .mouseButton(button, down: true, at: pointer.location)]
            case .moved: return [.mouseMove(to: pointer.location)]
            case .ended, .cancelled: return [.mouseButton(button, down: false, at: pointer.location)]
            }
        case let .scroll(dy):
            return scroll(dy)
        }
    }

    /// A key the session view read from the keyboard. A repeat goes straight through and leaves the press count alone.
    mutating func hostKey(_ key: Int32, pressed: Bool, isRepeat: Bool) -> [ArtemisInput] {
        if isRepeat { return pressed ? [.key(key, down: true)] : [] }
        return hold(key, pressed)
    }

    /// Scrolling by `dy` points, positive when the content should scroll down: the wheel turned toward the player.
    /// Each notch presses and releases the engine's wheel key.
    mutating func scroll(_ dy: Double) -> [ArtemisInput] {
        scrolled += dy
        let notches = Int(scrolled / Self.pointsPerNotch)
        guard notches != 0 else { return [] }
        scrolled -= Double(notches) * Self.pointsPerNotch
        let key = notches > 0 ? VirtualKey.wheelDown : VirtualKey.wheelUp
        return Array(repeating: [ArtemisInput.key(key, down: true), .key(key, down: false)], count: abs(notches)).flatMap { $0 }
    }

    /// Lets go of everything still held, for when the game stops getting input.
    mutating func releaseAll() -> [ArtemisInput] {
        let releases = held.keys.map { Self.input($0, down: false) }
        held = [:]
        scrolled = 0
        return releases
    }

    private mutating func hold(_ key: Int32, _ pressed: Bool) -> [ArtemisInput] {
        let count = held[key, default: 0]
        guard pressed || count > 0 else { return [] }
        held[key] = pressed ? count + 1 : (count == 1 ? nil : count - 1)
        return (pressed ? count == 0 : count == 1) ? [Self.input(key, down: pressed)] : []
    }

    private static func input(_ key: Int32, down: Bool) -> ArtemisInput {
        key == VirtualKey.leftButton || key == VirtualKey.rightButton ? .click(key, down: down) : .key(key, down: down)
    }
}
