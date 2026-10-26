import CoreGraphics
import VNCore

/// Input the engine receives. Keys are libretro key codes, which RGSS maps to its buttons the way RPG Maker does: Z,
/// Return and Space are C (confirm), X and Escape are B (cancel and menu), Shift is A (dash), Q and W are L and R.
/// Points are normalized over the view the game draws in.
nonisolated enum RGSSInput: Equatable, Sendable {
    case key(UInt32, down: Bool)
    case mouseMove(to: CGPoint)
    /// 0 left, 1 right.
    case mouseButton(Int32, down: Bool)
    /// Notches, positive turned away from the player.
    case wheel(Double)
}

/// Libretro key codes (`RETROK_*` in libretro.h). Letters, digits, and punctuation are their lowercase ASCII.
nonisolated enum RetroKey {
    static let backspace: UInt32 = 8
    static let tab: UInt32 = 9
    static let `return`: UInt32 = 13
    static let escape: UInt32 = 27
    static let space: UInt32 = 32
    static let delete: UInt32 = 127
    static let keypad0: UInt32 = 256
    static let keypadPeriod: UInt32 = 266
    static let keypadDivide: UInt32 = 267
    static let keypadMultiply: UInt32 = 268
    static let keypadMinus: UInt32 = 269
    static let keypadPlus: UInt32 = 270
    static let keypadEnter: UInt32 = 271
    static let keypadEquals: UInt32 = 272
    static let up: UInt32 = 273
    static let down: UInt32 = 274
    static let right: UInt32 = 275
    static let left: UInt32 = 276
    static let insert: UInt32 = 277
    static let home: UInt32 = 278
    static let end: UInt32 = 279
    static let pageUp: UInt32 = 280
    static let pageDown: UInt32 = 281
    static let f1: UInt32 = 282
    static let capsLock: UInt32 = 301
    static let rightShift: UInt32 = 303
    static let leftShift: UInt32 = 304
    static let rightControl: UInt32 = 305
    static let leftControl: UInt32 = 306
    static let rightAlt: UInt32 = 307
    static let leftAlt: UInt32 = 308
    static let rightMeta: UInt32 = 309
    static let leftMeta: UInt32 = 310

    static func ascii(_ character: Character) -> UInt32 { UInt32(character.asciiValue!) }

    /// The key for a USB HID keyboard usage, which UIKit reports as `UIKey.keyCode` and macOS key codes are mapped to.
    static func hidUsage(_ usage: Int) -> UInt32? {
        switch usage {
        case 4...29: return ascii("a") + UInt32(usage - 4)
        case 30...38: return ascii("1") + UInt32(usage - 30)
        case 39: return ascii("0")
        case 58...69: return f1 + UInt32(usage - 58)
        case 89...97: return keypad0 + 1 + UInt32(usage - 89)
        default: return namedHIDUsages[usage]
        }
    }

    private static let namedHIDUsages: [Int: UInt32] = [
        40: `return`, 41: escape, 42: backspace, 43: tab, 44: space,
        45: ascii("-"), 46: ascii("="), 47: ascii("["), 48: ascii("]"), 49: ascii("\\"), 51: ascii(";"), 52: ascii("'"),
        53: ascii("`"), 54: ascii(","), 55: ascii("."), 56: ascii("/"), 57: capsLock,
        73: insert, 74: home, 75: pageUp, 76: delete, 77: end, 78: pageDown, 79: right, 80: left, 81: down, 82: up,
        84: keypadDivide, 85: keypadMultiply, 86: keypadMinus, 87: keypadPlus, 88: keypadEnter, 98: keypad0,
        99: keypadPeriod, 103: keypadEquals,
        224: leftControl, 225: leftShift, 226: leftAlt, 227: leftMeta, 228: rightControl, 229: rightShift, 230: rightAlt,
        231: rightMeta,
    ]
}

nonisolated extension VirtualButton {
    /// RPG Maker's own keys for each button, the same as on its PC versions. Menu is X, which opens the menu in RPG
    /// Maker as Escape does, so keyboard plugins see what a keyboard player would press.
    var retroKey: UInt32? {
        switch self {
        case .up: RetroKey.up
        case .down: RetroKey.down
        case .left: RetroKey.left
        case .right: RetroKey.right
        case .confirm: RetroKey.return
        case .cancel: RetroKey.escape
        case .menu: RetroKey.ascii("x")
        case .dash: RetroKey.leftShift
        case .pageUp: RetroKey.ascii("q")
        case .pageDown: RetroKey.ascii("w")
        case .hub: nil
        }
    }
}

nonisolated extension KeyboardKey {
    /// The libretro key for the key's web `code`, such as `KeyZ` or `ArrowUp`. Nil for a key the engine has no use for.
    var retroKey: UInt32? {
        if code.hasPrefix("Key"), code.count == 4, let letter = code.last?.lowercased().first, letter.isLetter {
            return RetroKey.ascii(letter)
        }
        if code.hasPrefix("Digit"), code.count == 6, let digit = code.last, digit.isNumber { return RetroKey.ascii(digit) }
        if code.hasPrefix("Numpad"), code.count == 7, let digit = code.last?.wholeNumberValue { return RetroKey.keypad0 + UInt32(digit) }
        if code.hasPrefix("F"), let number = UInt32(code.dropFirst()), (1...15).contains(number) { return RetroKey.f1 + number - 1 }
        return Self.namedRetroKeys[code]
    }

    private static let namedRetroKeys: [String: UInt32] = [
        "Enter": RetroKey.return, "Escape": RetroKey.escape, "Backspace": RetroKey.backspace, "Tab": RetroKey.tab,
        "Space": RetroKey.space, "Delete": RetroKey.delete, "Insert": RetroKey.insert, "Home": RetroKey.home,
        "End": RetroKey.end, "PageUp": RetroKey.pageUp, "PageDown": RetroKey.pageDown, "ArrowUp": RetroKey.up,
        "ArrowDown": RetroKey.down, "ArrowLeft": RetroKey.left, "ArrowRight": RetroKey.right,
        "ShiftLeft": RetroKey.leftShift, "ShiftRight": RetroKey.rightShift, "ControlLeft": RetroKey.leftControl,
        "ControlRight": RetroKey.rightControl, "AltLeft": RetroKey.leftAlt, "AltRight": RetroKey.rightAlt,
        "Minus": RetroKey.ascii("-"), "Equal": RetroKey.ascii("="), "BracketLeft": RetroKey.ascii("["),
        "BracketRight": RetroKey.ascii("]"), "Backslash": RetroKey.ascii("\\"), "Semicolon": RetroKey.ascii(";"),
        "Quote": RetroKey.ascii("'"), "Backquote": RetroKey.ascii("`"), "Comma": RetroKey.ascii(","),
        "Period": RetroKey.ascii("."), "Slash": RetroKey.ascii("/"), "NumpadEnter": RetroKey.keypadEnter,
    ]
}

/// Turns VNDegen's controller events into engine input. Several buttons can hold the same key, such as Confirm and a
/// button remapped to Return, so a key goes down with the first press and up with the last release. Keys the session
/// view reads from a hardware keyboard pass through `hostKey` and share the same count.
nonisolated struct RGSSInputRouter {
    /// Scroll distance in points that makes one wheel notch.
    static let pointsPerNotch = 40.0

    private var held: [UInt32: Int] = [:]
    private var pointer = CGPoint(x: 0.5, y: 0.5)

    mutating func translate(_ event: ControllerEvent) -> [RGSSInput] {
        switch event {
        case let .button(button, pressed):
            return button.retroKey.map { hold($0, pressed) } ?? []
        case let .key(key, pressed):
            return key.retroKey.map { hold($0, pressed) } ?? []
        // RGSS reads no gamepad of its own, so Xbox buttons press RPG Maker's keys for VNDegen's actions: A confirms,
        // B cancels, X dashes, Y and Menu open the menu, LB and RB are L and R, and the D-pad and left stick move.
        case let .gamepad(input, pressed):
            return input.virtualButton?.retroKey.map { hold($0, pressed) } ?? []
        case let .pointer(pointer):
            self.pointer = pointer.location
            let button: Int32 = pointer.button == .primary ? 0 : 1
            switch pointer.phase {
            case .began: return [.mouseMove(to: pointer.location), .mouseButton(button, down: true)]
            case .moved: return [.mouseMove(to: pointer.location)]
            case .ended, .cancelled: return [.mouseMove(to: pointer.location), .mouseButton(button, down: false)]
            }
        case let .click(button, pressed):
            return [.mouseMove(to: pointer), .mouseButton(button == .primary ? 0 : 1, down: pressed)]
        case let .scroll(dy):
            return [.wheel(dy / Self.pointsPerNotch)]
        }
    }

    /// A key from the hardware keyboard. Repeats are dropped, because RGSS repeats held keys itself.
    mutating func hostKey(_ key: UInt32, pressed: Bool, isRepeat: Bool) -> [RGSSInput] {
        isRepeat ? [] : hold(key, pressed)
    }

    /// Releases every held key, such as when the hub's menu takes the keyboard.
    mutating func releaseAll() -> [RGSSInput] {
        defer { held = [:] }
        return held.keys.sorted().map { .key($0, down: false) }
    }

    private mutating func hold(_ key: UInt32, _ pressed: Bool) -> [RGSSInput] {
        let count = held[key, default: 0]
        if pressed {
            held[key] = count + 1
            return count == 0 ? [.key(key, down: true)] : []
        }
        guard count > 0 else { return [] }
        held[key] = count == 1 ? nil : count - 1
        return count == 1 ? [.key(key, down: false)] : []
    }
}
