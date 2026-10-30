import CWolfRPG
import VNCore

/// A key going down or up, in vnwolf.h's key codes.
nonisolated struct WolfRPGKey: Equatable, Sendable {
    var code: Int32
    var down: Bool
}

/// Turns VNDegen's controller events and the session view's own keyboard into the keys Wolf RPG Editor reads. Several
/// buttons can hold the same key, so a key goes down with the first press and up with the last release.
nonisolated struct WolfRPGInputRouter {
    private var held: [Int32: Int] = [:]
    private var hostKeysDown: Set<Int> = []

    mutating func translate(_ event: ControllerEvent) -> [WolfRPGKey] {
        switch event {
        case let .button(button, pressed):
            return control(button, pressed)
        case let .gamepad(input, pressed):
            guard let button = input.virtualButton else { return [] }
            return control(button, pressed)
        case let .key(key, pressed):
            guard let code = Self.wolfKey(windowsKeyCode: key.keyCode) else { return [] }
            return press(code, pressed)
        case .click, .pointer, .scroll:
            return []
        }
    }

    /// Wolf's buttons: Confirm is Z, Cancel and Menu are X (which opens the menu in most games), Dash is Shift (the sub
    /// key, which many games use to run), and the arrows move.
    private mutating func control(_ button: VirtualButton, _ pressed: Bool) -> [WolfRPGKey] {
        switch button {
        case .confirm: press(Int32(UInt8(ascii: "Z")), pressed)
        case .cancel, .menu: press(Int32(UInt8(ascii: "X")), pressed)
        case .dash: press(Int32(VNWOLF_KEY_SHIFT), pressed)
        case .up: press(Int32(VNWOLF_KEY_UP), pressed)
        case .down: press(Int32(VNWOLF_KEY_DOWN), pressed)
        case .left: press(Int32(VNWOLF_KEY_LEFT), pressed)
        case .right: press(Int32(VNWOLF_KEY_RIGHT), pressed)
        case .pageUp, .pageDown, .hub: []
        }
    }

    /// A key the session view read from a hardware keyboard, by USB HID usage. A held key's repeats are ignored.
    mutating func hostKey(hidUsage: Int, pressed: Bool) -> [WolfRPGKey] {
        guard let code = Self.wolfKey(hidUsage: hidUsage) else { return [] }
        if pressed {
            guard hostKeysDown.insert(hidUsage).inserted else { return [] }
        } else {
            guard hostKeysDown.remove(hidUsage) != nil else { return [] }
        }
        return press(code, pressed)
    }

    mutating func releaseAll() -> [WolfRPGKey] {
        let out = held.keys.map { WolfRPGKey(code: $0, down: false) }
        held = [:]
        hostKeysDown = []
        return out
    }

    private mutating func press(_ code: Int32, _ pressed: Bool) -> [WolfRPGKey] {
        let count = held[code, default: 0]
        if pressed {
            held[code] = count + 1
            guard count == 0 else { return [] }
        } else {
            guard count > 0 else { return [] }
            held[code] = count == 1 ? nil : count - 1
            guard count == 1 else { return [] }
        }
        return [WolfRPGKey(code: code, down: pressed)]
    }

    /// vnwolf.h key codes from Windows virtual-key codes, which remapped keys carry. Letters and digits are the same.
    static func wolfKey(windowsKeyCode vk: Int) -> Int32? {
        switch vk {
        case 8: Int32(VNWOLF_KEY_BACKSPACE)
        case 9: Int32(VNWOLF_KEY_TAB)
        case 13: Int32(VNWOLF_KEY_RETURN)
        case 27: Int32(VNWOLF_KEY_ESCAPE)
        case 32: Int32(VNWOLF_KEY_SPACE)
        case 37: Int32(VNWOLF_KEY_LEFT)
        case 38: Int32(VNWOLF_KEY_UP)
        case 39: Int32(VNWOLF_KEY_RIGHT)
        case 40: Int32(VNWOLF_KEY_DOWN)
        case 16: Int32(VNWOLF_KEY_SHIFT)
        case 17: Int32(VNWOLF_KEY_CTRL)
        case 48...57, 65...90: Int32(vk)
        case 98: 50  // number pad 2, 4, 6 and 8 move
        case 100: 52
        case 102: 54
        case 104: 56
        default: nil
        }
    }

    /// vnwolf.h key codes from USB HID usages.
    static func wolfKey(hidUsage u: Int) -> Int32? {
        switch u {
        case 4...29: Int32(65 + u - 4)  // A to Z
        case 30...38: Int32(49 + u - 30)  // 1 to 9
        case 39: 48  // 0
        case 40, 88: Int32(VNWOLF_KEY_RETURN)
        case 41: Int32(VNWOLF_KEY_ESCAPE)
        case 44: Int32(VNWOLF_KEY_SPACE)
        case 79: Int32(VNWOLF_KEY_RIGHT)
        case 80: Int32(VNWOLF_KEY_LEFT)
        case 81: Int32(VNWOLF_KEY_DOWN)
        case 82: Int32(VNWOLF_KEY_UP)
        case 90: 50  // number pad 2, 4, 6 and 8
        case 92: 52
        case 94: 54
        case 96: 56
        case 224, 228: Int32(VNWOLF_KEY_CTRL)
        case 225, 229: Int32(VNWOLF_KEY_SHIFT)
        default: nil
        }
    }
}
