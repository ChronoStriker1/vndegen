import CNScripter
import CoreGraphics
import VNCore

/// Input for the runtime, in vnns.h's terms. Points are normalized over the frame.
nonisolated enum NScripterInput: Equatable, Sendable {
    case key(Int32, down: Bool)
    case mouseButton(Int32, down: Bool)
    case mouseMove(CGPoint)
    case wheel(Int)
    case text(String)
}

/// Turns VNDegen's controller events and the session view's own keyboard into runtime input. Several buttons can hold
/// the same key, such as Confirm and a button remapped to Return, so a key goes down with the first press and up with
/// the last release.
nonisolated struct NScripterInputRouter {
    /// Scroll distance in points that makes one wheel notch, as for the other mouse-driven engines.
    static let pointsPerNotch = 40.0
    private var held: [Hold: Int] = [:]
    private var hostKeysDown: Set<Int> = []
    private var scrolled = 0.0

    enum Hold: Hashable {
        case key(Int32)
        case mouse(Int32)
    }

    /// `frame` maps a point normalized over the session view into the frame.
    mutating func translate(_ event: ControllerEvent, frame: (CGPoint) -> CGPoint) -> [NScripterInput] {
        switch event {
        case let .button(button, pressed):
            return control(button, pressed)
        case let .gamepad(input, pressed):
            guard let button = input.virtualButton else { return [] }
            return control(button, pressed)
        case let .key(key, pressed):
            guard let code = Self.vnnsKey(windowsKeyCode: key.keyCode) else { return [] }
            return press(.key(code), pressed)
        case let .click(button, pressed):
            return press(.mouse(button == .primary ? 1 : 3), pressed)
        case let .pointer(pointer):
            return self.pointer(pointer, at: frame(pointer.location))
        case let .scroll(dy):
            scrolled += dy
            let notches = Int(scrolled / Self.pointsPerNotch)
            guard notches != 0 else { return [] }
            scrolled -= Double(notches) * Self.pointsPerNotch
            // Content scrolling down is the wheel turned toward the player, which NScripter counts as negative.
            return [.wheel(-notches)]
        }
    }

    /// A pointer event whose location is already in the frame, from the session view itself.
    mutating func pointer(_ pointer: PointerEvent, at location: CGPoint) -> [NScripterInput] {
        let button: Int32 = pointer.button == .primary ? 1 : 3
        switch pointer.phase {
        case .began: return [.mouseMove(location)] + press(.mouse(button), true)
        case .moved: return [.mouseMove(location)]
        case .ended, .cancelled: return [.mouseMove(location)] + press(.mouse(button), false)
        }
    }

    /// The controls NScripter games share: Confirm clicks on, Cancel and Menu right click (the game's menu), the
    /// arrows move, page up and down turn the wheel (the message history), and Dash holds Control (skip).
    private mutating func control(_ button: VirtualButton, _ pressed: Bool) -> [NScripterInput] {
        switch button {
        case .confirm: press(.key(Int32(VNNS_KEY_RETURN)), pressed)
        case .cancel, .menu: press(.mouse(3), pressed)
        case .up: press(.key(Int32(VNNS_KEY_UP)), pressed)
        case .down: press(.key(Int32(VNNS_KEY_DOWN)), pressed)
        case .left: press(.key(Int32(VNNS_KEY_LEFT)), pressed)
        case .right: press(.key(Int32(VNNS_KEY_RIGHT)), pressed)
        case .pageUp: pressed ? [.wheel(1)] : []
        case .pageDown: pressed ? [.wheel(-1)] : []
        case .dash: press(.key(Int32(VNNS_KEY_CTRL)), pressed)
        case .hub: []
        }
    }

    /// A key the session view read from a hardware keyboard, by USB HID usage.
    /// The keyboard repeats a held key's down event, so a down for a key already down is ignored here, and each
    /// physical key counts once toward the key it holds.
    mutating func hostKey(hidUsage: Int, pressed: Bool) -> [NScripterInput] {
        guard let code = Self.vnnsKey(hidUsage: hidUsage) else { return [] }
        if pressed {
            guard hostKeysDown.insert(hidUsage).inserted else { return [] }
        } else {
            guard hostKeysDown.remove(hidUsage) != nil else { return [] }
        }
        return press(.key(code), pressed)
    }

    mutating func releaseAll() -> [NScripterInput] {
        let out = held.keys.map { hold -> NScripterInput in
            switch hold {
            case let .key(k): .key(k, down: false)
            case let .mouse(b): .mouseButton(b, down: false)
            }
        }
        held = [:]
        hostKeysDown = []
        return out
    }

    private mutating func press(_ hold: Hold, _ pressed: Bool) -> [NScripterInput] {
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
        case let .key(k): return [.key(k, down: pressed)]
        case let .mouse(b): return [.mouseButton(b, down: pressed)]
        }
    }

    /// vnns.h key codes from Windows virtual-key codes, which remapped keys carry. Letters and digits are the same.
    static func vnnsKey(windowsKeyCode vk: Int) -> Int32? {
        switch vk {
        case 8: Int32(VNNS_KEY_BACKSPACE)
        case 9: Int32(VNNS_KEY_TAB)
        case 13: Int32(VNNS_KEY_RETURN)
        case 27: Int32(VNNS_KEY_ESCAPE)
        case 32: Int32(VNNS_KEY_SPACE)
        case 33: Int32(VNNS_KEY_PAGEUP)
        case 34: Int32(VNNS_KEY_PAGEDOWN)
        case 37: Int32(VNNS_KEY_LEFT)
        case 38: Int32(VNNS_KEY_UP)
        case 39: Int32(VNNS_KEY_RIGHT)
        case 40: Int32(VNNS_KEY_DOWN)
        case 45: Int32(VNNS_KEY_INSERT)
        case 16: Int32(VNNS_KEY_SHIFT)
        case 17: Int32(VNNS_KEY_CTRL)
        case 48...57, 65...90: Int32(vk)
        case 112...123: Int32(VNNS_KEY_F1) + Int32(vk - 112)
        default: nil
        }
    }

    /// vnns.h key codes from USB HID usages.
    static func vnnsKey(hidUsage u: Int) -> Int32? {
        switch u {
        case 4...29: Int32(65 + u - 4)  // A to Z
        case 30...38: Int32(49 + u - 30)  // 1 to 9
        case 39: 48  // 0
        case 40, 88: Int32(VNNS_KEY_RETURN)
        case 41: Int32(VNNS_KEY_ESCAPE)
        case 42: Int32(VNNS_KEY_BACKSPACE)
        case 43: Int32(VNNS_KEY_TAB)
        case 44: Int32(VNNS_KEY_SPACE)
        case 58...69: Int32(VNNS_KEY_F1) + Int32(u - 58)
        case 73: Int32(VNNS_KEY_INSERT)
        case 75: Int32(VNNS_KEY_PAGEUP)
        case 78: Int32(VNNS_KEY_PAGEDOWN)
        case 79: Int32(VNNS_KEY_RIGHT)
        case 80: Int32(VNNS_KEY_LEFT)
        case 81: Int32(VNNS_KEY_DOWN)
        case 82: Int32(VNNS_KEY_UP)
        case 224, 228: Int32(VNNS_KEY_CTRL)
        case 225, 229: Int32(VNNS_KEY_SHIFT)
        default: nil
        }
    }
}
