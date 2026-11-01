import CWolfRPG
import CoreGraphics
import VNCore

/// Input for the runtime, in vnwolf.h's terms. Points are normalized over the frame.
nonisolated enum WolfRPGInput: Equatable, Sendable {
    case key(Int32, down: Bool)
    case mouseButton(Int32, down: Bool)
    case mouseMove(CGPoint)
    case wheel(Int)
}

/// Turns VNDegen's controller events and the session view's own input into what Wolf RPG Editor reads. Several buttons
/// can hold the same key or mouse button, so it goes down with the first press and up with the last release.
nonisolated struct WolfRPGInputRouter {
    /// Scroll distance in points that makes one wheel notch, as for the other mouse-driven engines.
    static let pointsPerNotch = 40.0
    private enum Hold: Hashable {
        case key(Int32)
        case mouse(Int32)
    }
    private var held: [Hold: Int] = [:]
    private var hostKeysDown: Set<Int> = []
    private var scrolled = 0.0

    /// `frame` maps a point normalized over the session view into the frame.
    mutating func translate(_ event: ControllerEvent, frame: (CGPoint) -> CGPoint) -> [WolfRPGInput] {
        switch event {
        case let .button(button, pressed):
            return control(button, pressed)
        case let .gamepad(input, pressed):
            guard let button = input.virtualButton else { return [] }
            return control(button, pressed)
        case let .key(key, pressed):
            guard let code = Self.wolfKey(windowsKeyCode: key.keyCode) else { return [] }
            return press(.key(code), pressed)
        case let .click(button, pressed):
            return press(.mouse(button == .primary ? 0 : 1), pressed)
        case let .pointer(pointer):
            return self.pointer(pointer, at: frame(pointer.location))
        case let .scroll(dy):
            scrolled += dy
            let notches = Int(scrolled / Self.pointsPerNotch)
            guard notches != 0 else { return [] }
            scrolled -= Double(notches) * Self.pointsPerNotch
            // Content scrolling down is the wheel turned toward the player, which Wolf counts as negative.
            return [.wheel(-notches)]
        }
    }

    /// A pointer event whose location is already in the frame, from the session view itself.
    mutating func pointer(_ pointer: PointerEvent, at location: CGPoint) -> [WolfRPGInput] {
        let button: Int32 = pointer.button == .primary ? 0 : 1
        switch pointer.phase {
        case .began: return [.mouseMove(location)] + press(.mouse(button), true)
        case .moved: return [.mouseMove(location)]
        case .ended, .cancelled: return [.mouseMove(location)] + press(.mouse(button), false)
        }
    }

    /// Wolf's buttons: Confirm is Z, Cancel and Menu are X (which opens the menu in most games), Dash is Shift (the sub
    /// key, which many games use to run), the arrows move, and page up and down turn the wheel.
    private mutating func control(_ button: VirtualButton, _ pressed: Bool) -> [WolfRPGInput] {
        switch button {
        case .confirm: press(.key(Int32(UInt8(ascii: "Z"))), pressed)
        case .cancel, .menu: press(.key(Int32(UInt8(ascii: "X"))), pressed)
        case .dash: press(.key(Int32(VNWOLF_KEY_SHIFT)), pressed)
        case .up: press(.key(Int32(VNWOLF_KEY_UP)), pressed)
        case .down: press(.key(Int32(VNWOLF_KEY_DOWN)), pressed)
        case .left: press(.key(Int32(VNWOLF_KEY_LEFT)), pressed)
        case .right: press(.key(Int32(VNWOLF_KEY_RIGHT)), pressed)
        case .pageUp: pressed ? [.wheel(1)] : []
        case .pageDown: pressed ? [.wheel(-1)] : []
        case .hub: []
        }
    }

    /// A key pressed and let go at once, as typing on the on-screen keyboard does.
    func tap(_ code: Int32) -> [WolfRPGInput] {
        [.key(code, down: true), .key(code, down: false)]
    }

    /// A key the session view read from a hardware keyboard, by USB HID usage. A held key's repeats are ignored.
    mutating func hostKey(hidUsage: Int, pressed: Bool) -> [WolfRPGInput] {
        guard let code = Self.wolfKey(hidUsage: hidUsage) else { return [] }
        if pressed {
            guard hostKeysDown.insert(hidUsage).inserted else { return [] }
        } else {
            guard hostKeysDown.remove(hidUsage) != nil else { return [] }
        }
        return press(.key(code), pressed)
    }

    mutating func releaseAll() -> [WolfRPGInput] {
        let out = held.keys.map { hold -> WolfRPGInput in
            switch hold {
            case let .key(code): .key(code, down: false)
            case let .mouse(button): .mouseButton(button, down: false)
            }
        }
        held = [:]
        hostKeysDown = []
        return out
    }

    private mutating func press(_ hold: Hold, _ pressed: Bool) -> [WolfRPGInput] {
        let count = held[hold, default: 0]
        if pressed {
            held[hold] = count + 1
            guard count == 0 else { return [] }
        } else {
            guard count > 0 else { return [] }
            held[hold] = count == 1 ? nil : count - 1
            guard count == 1 else { return [] }
        }
        switch hold {
        case let .key(code): return [.key(code, down: pressed)]
        case let .mouse(button): return [.mouseButton(button, down: pressed)]
        }
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
        case 42: Int32(VNWOLF_KEY_BACKSPACE)
        case 43: Int32(VNWOLF_KEY_TAB)
        case 44: Int32(VNWOLF_KEY_SPACE)
        case 58...69: Int32(VNWOLF_KEY_F1) + Int32(u - 58)
        case 75: Int32(VNWOLF_KEY_PAGEUP)
        case 78: Int32(VNWOLF_KEY_PAGEDOWN)
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
