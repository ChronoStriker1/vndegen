import VNCore

/// One physical control that can hold a virtual button down: a gamepad element or a keyboard key.
nonisolated enum InputSource: Hashable, Sendable {
    case gamepad(ObjectIdentifier, GamepadElement)
    case dpad(ObjectIdentifier)
    case stick(ObjectIdentifier)
    case key(Int)

    var controller: ObjectIdentifier? {
        switch self {
        case let .gamepad(id, _), let .dpad(id), let .stick(id): id
        case .key: nil
        }
    }
}

/// Turns physical presses into virtual press and release events for one consumer at a time.
///
/// A virtual button goes down when its first physical source goes down and up when its last one goes up,
/// so the D-pad and the stick pushed together, or A and Return, still make one press.
/// The current consumer only gets releases for presses it saw, and losing the consumer releases everything it holds.
nonisolated struct PressRouter {
    static let repeatDelay: Duration = .milliseconds(400)
    static let repeatInterval: Duration = .milliseconds(100)

    private var held: [VirtualButton: Set<InputSource>] = [:]
    private var delivered: Set<VirtualButton> = []
    private(set) var repeating: (button: VirtualButton, at: ContinuousClock.Instant)?

    var heldSources: [(InputSource, VirtualButton)] {
        held.flatMap { button, sources in sources.map { ($0, button) } }
    }

    mutating func update(_ source: InputSource, _ button: VirtualButton, pressed: Bool,
                         at now: ContinuousClock.Instant, hasConsumer: Bool, repeats: Bool) -> ControllerEvent? {
        if pressed {
            let alreadyDown = !(held[button]?.isEmpty ?? true)
            held[button, default: []].insert(source)
            guard !alreadyDown, hasConsumer else { return nil }
            delivered.insert(button)
            if repeats, button.isDirection { repeating = (button, now + Self.repeatDelay) }
            return .button(button, pressed: true)
        }
        guard held[button]?.remove(source) != nil, held[button]?.isEmpty == true else { return nil }
        held[button] = nil
        if repeating?.button == button { repeating = nil }
        guard delivered.remove(button) != nil else { return nil }
        return .button(button, pressed: false)
    }

    /// The repeated press that is due at `now`, if any.
    mutating func tick(at now: ContinuousClock.Instant, hasConsumer: Bool, repeats: Bool) -> ControllerEvent? {
        guard let current = repeating, now >= current.at else { return nil }
        guard repeats, hasConsumer, delivered.contains(current.button) else {
            repeating = nil
            return nil
        }
        repeating = (current.button, now + Self.repeatInterval)
        return .button(current.button, pressed: true)
    }

    /// Releases for every press the outgoing consumer saw. Buttons still physically held stay silent until pressed again.
    mutating func detachConsumer() -> [ControllerEvent] {
        let releases = VirtualButton.allCases.filter(delivered.contains).map { ControllerEvent.button($0, pressed: false) }
        delivered = []
        repeating = nil
        return releases
    }
}

nonisolated extension VirtualButton {
    var isDirection: Bool {
        switch self {
        case .up, .down, .left, .right: true
        default: false
        }
    }
}

/// Turns an analog stick position into direction presses, with a dead zone and hysteresis per axis,
/// so a push past the press threshold makes exactly one press and small wobbles around it make none.
nonisolated struct StickDirections {
    static let pressThreshold: Float = 0.5
    static let releaseThreshold: Float = 0.3

    private(set) var active: Set<VirtualButton> = []

    /// Releases come first, so flicking from left to right releases left before pressing right.
    mutating func update(x: Float, y: Float) -> [(VirtualButton, pressed: Bool)] {
        let axes: [(VirtualButton, Float)] = [(.right, x), (.left, -x), (.up, y), (.down, -y)]
        var releases: [(VirtualButton, pressed: Bool)] = []
        var presses: [(VirtualButton, pressed: Bool)] = []
        for (direction, value) in axes {
            if active.contains(direction) {
                if value < Self.releaseThreshold {
                    active.remove(direction)
                    releases.append((direction, false))
                }
            } else if value > Self.pressThreshold {
                active.insert(direction)
                presses.append((direction, true))
            }
        }
        return releases + presses
    }
}
