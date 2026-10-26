import CoreGraphics
import VNCore

/// Input the engine receives, in SDL's terms. Points are normalized over the view the game draws in. Mouse buttons
/// use SDL's numbering, 1 for left and 3 for right.
nonisolated enum KiriKiriInput: Equatable, Sendable {
    case key(scancode: Int32, down: Bool)
    case mouseButton(Int32, down: Bool, at: CGPoint)
    case mouseMove(to: CGPoint)
    /// A mouse button pressed wherever the pointer was last.
    case click(Int32, down: Bool)
    /// Notches, positive turned away from the player.
    case wheel(notches: Int)
}

/// What a button does in a KiriKiri game.
nonisolated enum KiriKiriControl: Hashable, Sendable {
    /// A key or a mouse button, held as long as the button is.
    case hold(KiriKiriHold)
    /// Wheel notches, sent once per press.
    case wheel(Int)
}

nonisolated enum KiriKiriHold: Hashable, Sendable {
    case key(Int32)
    /// A mouse button, pressed wherever the pointer was last.
    case mouse(Int32)

    func input(down: Bool) -> KiriKiriInput {
        switch self {
        case let .key(scancode): .key(scancode: scancode, down: down)
        case let .mouse(button): .click(button, down: down)
        }
    }
}

nonisolated enum SDLScancode {
    static let `return`: Int32 = 40
    static let escape: Int32 = 41
    static let backspace: Int32 = 42
    static let tab: Int32 = 43
    static let space: Int32 = 44
    static let graveAccent: Int32 = 53
    static let home: Int32 = 74
    static let pageUp: Int32 = 75
    static let end: Int32 = 77
    static let pageDown: Int32 = 78
    static let right: Int32 = 79
    static let left: Int32 = 80
    static let down: Int32 = 81
    static let up: Int32 = 82
    static let leftControl: Int32 = 224
    static let leftShift: Int32 = 225
    static let leftAlt: Int32 = 226
}

nonisolated enum SDLMouseButton {
    static let left: Int32 = 1
    static let right: Int32 = 3

    static func number(_ button: PointerEvent.Button) -> Int32 { button == .primary ? left : right }
}

nonisolated extension VirtualButton {
    /// KAG3's own bindings, from its `MainWindow.tjs`. Return reads on and presses the focused button, and the arrows
    /// move that focus. A right click runs the game's right-click handler, its way back out of menus. Escape clicks
    /// the right-click menu item, which most games use to hide the text or open their menu. Holding Control skips in
    /// KAGEX and in games that make it their extra reading key. Turning the wheel away opens the message history and
    /// pages back through it, and turning it toward the player pages forward and reads on.
    var kiriKiriControl: KiriKiriControl? {
        switch self {
        case .up: .hold(.key(SDLScancode.up))
        case .down: .hold(.key(SDLScancode.down))
        case .left: .hold(.key(SDLScancode.left))
        case .right: .hold(.key(SDLScancode.right))
        case .confirm: .hold(.key(SDLScancode.return))
        case .cancel: .hold(.mouse(SDLMouseButton.right))
        case .menu: .hold(.key(SDLScancode.escape))
        case .dash: .hold(.key(SDLScancode.leftControl))
        case .pageUp: .wheel(1)
        case .pageDown: .wheel(-1)
        case .hub: nil
        }
    }
}

nonisolated extension KeyboardKey {
    /// The SDL scancode, which is the USB HID usage, for the key's `code`. Nil for a key the engine has no use for.
    var sdlScancode: Int32? {
        if code.hasPrefix("Key"), code.count == 4, let letter = code.unicodeScalars.last, ("A"..."Z").contains(letter) {
            return 4 + Int32(letter.value - UnicodeScalar("A").value)
        }
        if code.hasPrefix("Digit"), code.count == 6, let digit = Int32(code.suffix(1)) {
            return digit == 0 ? 39 : 29 + digit
        }
        if code.hasPrefix("F"), let number = Int32(code.dropFirst()), (1...12).contains(number) {
            return 57 + number
        }
        return Self.namedScancodes[code]
    }

    private static let namedScancodes: [String: Int32] = [
        "Enter": SDLScancode.return, "Escape": SDLScancode.escape, "Backspace": SDLScancode.backspace,
        "Tab": SDLScancode.tab, "Space": SDLScancode.space, "Home": SDLScancode.home, "PageUp": SDLScancode.pageUp,
        "End": SDLScancode.end, "PageDown": SDLScancode.pageDown, "ArrowRight": SDLScancode.right,
        "ArrowLeft": SDLScancode.left, "ArrowDown": SDLScancode.down, "ArrowUp": SDLScancode.up,
        "ControlLeft": SDLScancode.leftControl, "ShiftLeft": SDLScancode.leftShift, "AltLeft": SDLScancode.leftAlt,
    ]
}

/// Turns VNDegen's controller events and the keyboard and mouse the session view reads itself into engine input.
/// Several buttons can hold the same key or mouse button, such as Confirm and a button remapped to Return, so it goes
/// down with the first press and up with the last release.
nonisolated struct KiriKiriInputRouter {
    /// Scroll distance in points that makes one wheel notch.
    static let pointsPerNotch = 40.0

    private var held: [KiriKiriHold: Int] = [:]
    private var scrolled = 0.0

    mutating func translate(_ event: ControllerEvent) -> [KiriKiriInput] {
        switch event {
        case let .button(button, pressed):
            switch button.kiriKiriControl {
            case let .hold(hold): return press(hold, pressed)
            case let .wheel(notches): return pressed ? [.wheel(notches: notches)] : []
            case nil: return []
            }
        case let .key(key, pressed):
            return key.sdlScancode.map { press(.key($0), pressed) } ?? []
        case let .click(button, pressed):
            return press(.mouse(SDLMouseButton.number(button)), pressed)
        case let .pointer(pointer):
            let button = SDLMouseButton.number(pointer.button)
            switch pointer.phase {
            case .began: return [.mouseMove(to: pointer.location), .mouseButton(button, down: true, at: pointer.location)]
            case .moved: return [.mouseMove(to: pointer.location)]
            case .ended, .cancelled: return [.mouseButton(button, down: false, at: pointer.location)]
            }
        case let .scroll(dy):
            return scroll(dy)
        }
    }

    /// A key the session view read from the keyboard. A repeat goes straight through, as a held key's repeats do in
    /// SDL, and leaves the press count alone.
    mutating func hostKey(_ scancode: Int32, pressed: Bool, isRepeat: Bool) -> [KiriKiriInput] {
        guard scancode != SDLScancode.graveAccent else { return [] }
        if isRepeat { return pressed ? [.key(scancode: scancode, down: true)] : [] }
        return press(.key(scancode), pressed)
    }

    /// Scrolling by `dy` points, positive when the content should scroll down. That is the wheel turned toward the
    /// player.
    mutating func scroll(_ dy: Double) -> [KiriKiriInput] {
        scrolled += dy
        let notches = Int(scrolled / Self.pointsPerNotch)
        guard notches != 0 else { return [] }
        scrolled -= Double(notches) * Self.pointsPerNotch
        return [.wheel(notches: -notches)]
    }

    /// Lets go of everything still held, for when the game stops getting input.
    mutating func releaseAll() -> [KiriKiriInput] {
        let releases = held.keys.map { $0.input(down: false) }
        held = [:]
        scrolled = 0
        return releases
    }

    private mutating func press(_ hold: KiriKiriHold, _ pressed: Bool) -> [KiriKiriInput] {
        let count = held[hold, default: 0]
        guard pressed || count > 0 else { return [] }
        held[hold] = pressed ? count + 1 : (count == 1 ? nil : count - 1)
        return (pressed ? count == 0 : count == 1) ? [hold.input(down: pressed)] : []
    }
}
