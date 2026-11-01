import CoreGraphics
import VNCore

/// Input for the System 3 runtime, in vns3.h's terms: Windows virtual-key codes and mouse buttons. Points are
/// normalized over the frame.
nonisolated enum System3Input: Equatable, Sendable {
    case key(Int32, down: Bool)
    case mouseButton(Int32, down: Bool)
    case mouseMove(CGPoint)
    case wheel(Int)
}

/// Turns VNDegen's controller events and the session view's own keyboard into runtime input. Several buttons can hold
/// the same key, so a key goes down with the first press and up with the last release.
nonisolated struct System3InputRouter {
    static let pointsPerNotch = 40.0
    private var held: [Hold: Int] = [:]
    private var hostKeysDown: Set<Int> = []
    private var scrolled = 0.0

    enum Hold: Hashable {
        case key(Int32)
        case mouse(Int32)
    }

    /// `frame` maps a point normalized over the session view into the frame.
    mutating func translate(_ event: ControllerEvent, frame: (CGPoint) -> CGPoint) -> [System3Input] {
        switch event {
        case let .button(button, pressed):
            return control(button, pressed)
        case let .gamepad(input, pressed):
            guard let button = input.virtualButton else { return [] }
            return control(button, pressed)
        case let .key(key, pressed):
            guard (1...255).contains(key.keyCode) else { return [] }
            return press(.key(Int32(key.keyCode)), pressed)
        case let .click(button, pressed):
            return press(.mouse(button == .primary ? mouseLeft : mouseRight), pressed)
        case let .pointer(pointer):
            return self.pointer(pointer, at: frame(pointer.location))
        case let .scroll(dy):
            scrolled += dy
            let notches = Int(scrolled / Self.pointsPerNotch)
            guard notches != 0 else { return [] }
            scrolled -= Double(notches) * Self.pointsPerNotch
            return [.wheel(-notches)]
        }
    }

    /// A pointer event whose location is already in the frame, from the session view itself.
    mutating func pointer(_ pointer: PointerEvent, at location: CGPoint) -> [System3Input] {
        let button = pointer.button == .primary ? mouseLeft : mouseRight
        switch pointer.phase {
        case .began: return [.mouseMove(location)] + press(.mouse(button), true)
        case .moved: return [.mouseMove(location)]
        case .ended, .cancelled: return [.mouseMove(location)] + press(.mouse(button), false)
        }
    }

    /// System 3 games read Return or a left click to go on, Space or a right click to cancel or open a menu, and the
    /// arrows to move. Dash holds Control, which some games read to skip messages.
    private mutating func control(_ button: VirtualButton, _ pressed: Bool) -> [System3Input] {
        switch button {
        case .confirm: press(.key(0x0d), pressed)
        case .cancel, .menu: press(.mouse(mouseRight), pressed)
        case .up: press(.key(0x26), pressed)
        case .down: press(.key(0x28), pressed)
        case .left: press(.key(0x25), pressed)
        case .right: press(.key(0x27), pressed)
        case .pageUp: pressed ? [.wheel(1)] : []
        case .pageDown: pressed ? [.wheel(-1)] : []
        case .dash: press(.key(0x11), pressed)
        case .hub: []
        }
    }

    /// A key the session view read from a hardware keyboard, by USB HID usage. A held key repeats its down event, so
    /// a down for a key already down is ignored.
    mutating func hostKey(hidUsage: Int, pressed: Bool) -> [System3Input] {
        guard let code = Self.virtualKey(hidUsage: hidUsage) else { return [] }
        if pressed {
            guard hostKeysDown.insert(hidUsage).inserted else { return [] }
        } else {
            guard hostKeysDown.remove(hidUsage) != nil else { return [] }
        }
        return press(.key(code), pressed)
    }

    mutating func releaseAll() -> [System3Input] {
        let out = held.keys.map { hold -> System3Input in
            switch hold {
            case let .key(k): .key(k, down: false)
            case let .mouse(b): .mouseButton(b, down: false)
            }
        }
        held = [:]
        hostKeysDown = []
        return out
    }

    private mutating func press(_ hold: Hold, _ pressed: Bool) -> [System3Input] {
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

    /// Windows virtual-key codes from USB HID usages.
    static func virtualKey(hidUsage u: Int) -> Int32? {
        switch u {
        case 4...29: Int32(65 + u - 4)  // A to Z
        case 30...38: Int32(49 + u - 30)  // 1 to 9
        case 39: 48  // 0
        case 40, 88: 0x0d  // Return, keypad Enter
        case 41: 0x1b  // Escape
        case 42: 0x08  // Backspace
        case 43: 0x09  // Tab
        case 44: 0x20  // Space
        case 58...69: Int32(0x70 + u - 58)  // F1 to F12
        case 79: 0x27  // Right
        case 80: 0x25  // Left
        case 81: 0x28  // Down
        case 82: 0x26  // Up
        case 89...97: Int32(0x61 + u - 89)  // keypad 1 to 9
        case 98: 0x60  // keypad 0
        case 224, 228: 0x11  // Control
        case 225, 229: 0x10  // Shift
        default: nil
        }
    }
}

/// vns3.h's mouse buttons.
nonisolated private let mouseLeft: Int32 = 1
nonisolated let mouseRight: Int32 = 2
