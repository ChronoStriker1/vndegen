import CoreGraphics
import VNCore

/// Input the engines receive. Keys are Windows virtual-key codes. Points are normalized over the view.
nonisolated enum SiglusInput: Equatable, Sendable {
    case key(Int32, down: Bool, repeat: Bool = false)
    /// 0 the left button, 1 the right one.
    case button(Int32, down: Bool)
    case pointer(CGPoint)
    /// Wheel notches, positive away from the player.
    case wheel(Int32)
}

nonisolated enum VirtualKey {
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
    static let delete: Int32 = 46

    /// The virtual-key code for a USB HID usage, the key code UIKit reports and VNDegen's macOS key table gives. Nil
    /// for a key the engines don't read.
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
        case 74: home
        case 75: pageUp
        case 76: delete
        case 77: end
        case 78: pageDown
        case 79: right
        case 80: left
        case 81: down
        case 82: up
        case 224, 228: control
        case 225, 229: shift
        case 226, 230: alt
        default: nil
        }
    }
}

nonisolated extension KeyboardKey {
    /// The virtual-key code for the key's `code`, such as `KeyA` or `ArrowUp`. Nil for a key the engines don't read.
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
        "PageDown": VirtualKey.pageDown, "Delete": VirtualKey.delete, "ArrowRight": VirtualKey.right,
        "ArrowLeft": VirtualKey.left, "ArrowDown": VirtualKey.down, "ArrowUp": VirtualKey.up,
        "ControlLeft": VirtualKey.control, "ControlRight": VirtualKey.control, "ShiftLeft": VirtualKey.shift,
        "ShiftRight": VirtualKey.shift, "AltLeft": VirtualKey.alt, "AltRight": VirtualKey.alt,
    ]
}

/// What each of VNDegen's buttons does in these games, as their PC versions bind the keyboard and mouse: Return reads on
/// and presses the focused choice, the arrows move between choices, Escape and the right click open the game's menu or
/// back out of one, Control held skips, and the wheel turned away opens the message history and turned toward the
/// player reads on.
nonisolated enum SiglusControl: Equatable, Sendable {
    case key(Int32)
    case rightClick
    case wheel(Int32)

    init?(_ button: VirtualButton) {
        switch button {
        case .up: self = .key(VirtualKey.up)
        case .down: self = .key(VirtualKey.down)
        case .left: self = .key(VirtualKey.left)
        case .right: self = .key(VirtualKey.right)
        case .confirm: self = .key(VirtualKey.return)
        case .cancel: self = .key(VirtualKey.escape)
        case .menu: self = .rightClick
        case .dash: self = .key(VirtualKey.control)
        case .pageUp: self = .wheel(1)
        case .pageDown: self = .wheel(-1)
        case .hub: return nil
        }
    }
}

/// Turns VNDegen's controller events and the keyboard the session view reads itself into engine input. Several buttons
/// can hold the same key, such as Confirm and a button remapped to Return, so it goes down with the first press and up
/// with the last release.
nonisolated struct SiglusInputRouter {
    /// Scroll distance in points that makes one wheel notch.
    static let pointsPerNotch = 40.0

    /// Presses held, by key: virtual keys, and -1 and -2 for the left and right mouse buttons.
    private var held: [Int32: Int] = [:]
    private var scrolled = 0.0

    mutating func translate(_ event: ControllerEvent) -> [SiglusInput] {
        switch event {
        case let .button(button, pressed):
            return SiglusControl(button).map { press($0, pressed) } ?? []
        case let .gamepad(input, pressed):
            return input.virtualButton.flatMap(SiglusControl.init).map { press($0, pressed) } ?? []
        case let .key(key, pressed):
            return key.virtualKey.map { hold($0, pressed) } ?? []
        case let .click(button, pressed):
            return hold(button == .primary ? Self.left : Self.right, pressed)
        case let .pointer(pointer):
            let button = pointer.button == .primary ? Self.left : Self.right
            switch pointer.phase {
            case .began: return [.pointer(pointer.location)] + hold(button, true)
            case .moved: return [.pointer(pointer.location)]
            case .ended, .cancelled: return [.pointer(pointer.location)] + hold(button, false)
            }
        case let .scroll(dy):
            return scroll(dy)
        }
    }

    /// A key the session view read from the keyboard. A repeat goes straight through and leaves the press count alone.
    mutating func hostKey(_ key: Int32, pressed: Bool, isRepeat: Bool) -> [SiglusInput] {
        if isRepeat { return pressed ? [.key(key, down: true, repeat: true)] : [] }
        return hold(key, pressed)
    }

    /// Scrolling by `dy` points, positive when the content should scroll down: the wheel turned toward the player.
    mutating func scroll(_ dy: Double) -> [SiglusInput] {
        scrolled += dy
        let notches = Int(scrolled / Self.pointsPerNotch)
        guard notches != 0 else { return [] }
        scrolled -= Double(notches) * Self.pointsPerNotch
        return [.wheel(Int32(-notches))]
    }

    /// Lets go of everything still held, for when the game stops getting input.
    mutating func releaseAll() -> [SiglusInput] {
        let releases = held.keys.sorted().map { Self.input($0, down: false) }
        held = [:]
        scrolled = 0
        return releases
    }

    private static let left: Int32 = -1
    private static let right: Int32 = -2

    private mutating func press(_ control: SiglusControl, _ pressed: Bool) -> [SiglusInput] {
        switch control {
        case let .key(key): hold(key, pressed)
        case .rightClick: hold(Self.right, pressed)
        case let .wheel(notches): pressed ? [.wheel(notches)] : []
        }
    }

    private mutating func hold(_ key: Int32, _ pressed: Bool) -> [SiglusInput] {
        let count = held[key, default: 0]
        guard pressed || count > 0 else { return [] }
        held[key] = pressed ? count + 1 : (count == 1 ? nil : count - 1)
        return (pressed ? count == 0 : count == 1) ? [Self.input(key, down: pressed)] : []
    }

    private static func input(_ key: Int32, down: Bool) -> SiglusInput {
        switch key {
        case left: .button(0, down: down)
        case right: .button(1, down: down)
        default: .key(key, down: down)
        }
    }
}
