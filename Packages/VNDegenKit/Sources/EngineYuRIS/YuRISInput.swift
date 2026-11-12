import CoreGraphics
import VNCore

nonisolated enum YuRISInput: Equatable, Sendable {
    case key(Int32, down: Bool)
    case mouseButton(Int32, down: Bool)
    case mouseMove(CGPoint)
    case wheel(Int)
}

nonisolated struct YuRISInputRouter {
    static let pointsPerNotch = 40.0
    private var held: [Hold: Int] = [:]
    private var hostKeysDown: Set<Int> = []
    private var scrolled = 0.0

    enum Hold: Hashable {
        case key(Int32)
        case mouse(Int32)
    }

    mutating func translate(_ event: ControllerEvent, frame: (CGPoint) -> CGPoint) -> [YuRISInput] {
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

    mutating func pointer(_ pointer: PointerEvent, at location: CGPoint) -> [YuRISInput] {
        let button = pointer.button == .primary ? mouseLeft : mouseRight
        switch pointer.phase {
        case .began: return [.mouseMove(location)] + press(.mouse(button), true)
        case .moved: return [.mouseMove(location)]
        case .ended, .cancelled: return [.mouseMove(location)] + press(.mouse(button), false)
        }
    }

    private mutating func control(_ button: VirtualButton, _ pressed: Bool) -> [YuRISInput] {
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

    mutating func hostKey(hidUsage: Int, pressed: Bool) -> [YuRISInput] {
        guard let code = Self.virtualKey(hidUsage: hidUsage) else { return [] }
        if pressed {
            guard hostKeysDown.insert(hidUsage).inserted else { return [] }
        } else {
            guard hostKeysDown.remove(hidUsage) != nil else { return [] }
        }
        return press(.key(code), pressed)
    }

    mutating func releaseAll() -> [YuRISInput] {
        hostKeysDown = []
        let out = held.keys.map { hold -> YuRISInput in
            switch hold {
            case let .key(k): .key(k, down: false)
            case let .mouse(b): .mouseButton(b, down: false)
            }
        }
        held = [:]
        return out
    }

    private mutating func press(_ hold: Hold, _ pressed: Bool) -> [YuRISInput] {
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

    static func virtualKey(hidUsage u: Int) -> Int32? {
        switch u {
        case 4...29: Int32(65 + u - 4)
        case 30...38: Int32(49 + u - 30)
        case 39: 48
        case 40, 88: 0x0d
        case 41: 0x1b
        case 42: 0x08
        case 43: 0x09
        case 44: 0x20
        case 58...69: Int32(0x70 + u - 58)
        case 79: 0x27
        case 80: 0x25
        case 81: 0x28
        case 82: 0x26
        case 89...97: Int32(0x61 + u - 89)
        case 98: 0x60
        case 224, 228: 0x11
        case 225, 229: 0x10
        default: nil
        }
    }
}

nonisolated private let mouseLeft: Int32 = 1
nonisolated private let mouseRight: Int32 = 2
