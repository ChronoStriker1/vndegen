import VNCore

nonisolated struct SmileGameKey: Equatable, Sendable {
    var code: Int32
    var down: Bool
}

nonisolated struct SmileGameInputRouter {
    static let enter: Int32 = 0x0d
    static let escape: Int32 = 0x1b
    static let shift: Int32 = 0x10
    static let left: Int32 = 0x25
    static let up: Int32 = 0x26
    static let right: Int32 = 0x27
    static let down: Int32 = 0x28
    static let menu = Int32(UInt8(ascii: "X"))

    private var held: [Int32: Int] = [:]
    private var hostKeysDown: Set<Int> = []

    mutating func translate(_ event: ControllerEvent) -> [SmileGameKey] {
        switch event {
        case let .button(button, pressed):
            return Self.key(for: button).map { press($0, pressed) } ?? []
        case let .gamepad(input, pressed):
            return input.virtualButton.flatMap(Self.key(for:)).map { press($0, pressed) } ?? []
        case let .key(key, pressed):
            guard (1...255).contains(key.keyCode) else { return [] }
            return press(Int32(key.keyCode), pressed)
        case .click, .pointer, .scroll:
            return []
        }
    }

    static func key(for button: VirtualButton) -> Int32? {
        switch button {
        case .confirm: enter
        case .cancel: escape
        case .menu: menu
        case .dash: shift
        case .up: up
        case .down: down
        case .left: left
        case .right: right
        case .pageUp, .pageDown, .hub: nil
        }
    }

    mutating func hostKey(hidUsage: Int, pressed: Bool) -> [SmileGameKey] {
        guard let code = Self.virtualKey(hidUsage: hidUsage) else { return [] }
        if pressed {
            guard hostKeysDown.insert(hidUsage).inserted else { return [] }
        } else {
            guard hostKeysDown.remove(hidUsage) != nil else { return [] }
        }
        return press(code, pressed)
    }

    mutating func releaseAll() -> [SmileGameKey] {
        hostKeysDown = []
        let out = held.keys.sorted().map { SmileGameKey(code: $0, down: false) }
        held = [:]
        return out
    }

    private mutating func press(_ code: Int32, _ pressed: Bool) -> [SmileGameKey] {
        let count = held[code, default: 0]
        if pressed {
            held[code] = count + 1
            guard count == 0 else { return [] }
        } else {
            guard count > 0 else { return [] }
            held[code] = count == 1 ? nil : count - 1
            guard count == 1 else { return [] }
        }
        return [SmileGameKey(code: code, down: pressed)]
    }

    static func virtualKey(hidUsage u: Int) -> Int32? {
        switch u {
        case 4...29: Int32(65 + u - 4)
        case 30...38: Int32(49 + u - 30)
        case 39: 48
        case 40, 88: enter
        case 41: escape
        case 44: 0x20
        case 79: right
        case 80: left
        case 81: down
        case 82: up
        case 225, 229: shift
        default: nil
        }
    }
}
