import VNCore

/// One physical control that can hold an action down: a gamepad element, a D-pad or stick direction, or a keyboard key.
nonisolated enum InputSource: Hashable, Sendable {
    case gamepad(ObjectIdentifier, GamepadElement)
    case dpad(ObjectIdentifier, ControlInput)
    case stick(ObjectIdentifier, ControlInput)
    case key(Int)

    var controller: ObjectIdentifier? {
        switch self {
        case let .gamepad(id, _), let .dpad(id, _), let .stick(id, _): id
        case .key: nil
        }
    }
}

/// Turns physical presses into press and release events for one consumer at a time.
///
/// An action goes down when its first physical source goes down and up when its last one goes up, so the D-pad and the
/// stick pushed together, or A and Return, still make one press. Each source releases the action it pressed, even when
/// the mapping changed while it was held. The current consumer only gets releases for presses it saw, and losing the
/// consumer releases everything it holds.
nonisolated struct PressRouter {
    static let repeatDelay: Duration = .milliseconds(400)
    static let repeatInterval: Duration = .milliseconds(100)

    private var held: [InputSource: ControlAction] = [:]
    /// Delivered actions in press order.
    private var delivered: [ControlAction] = []
    private(set) var repeating: (action: ControlAction, at: ContinuousClock.Instant)?

    var heldSources: [InputSource] { Array(held.keys) }

    mutating func press(_ source: InputSource, _ action: ControlAction,
                        at now: ContinuousClock.Instant, hasConsumer: Bool, repeats: Bool) -> ControllerEvent? {
        guard held[source] == nil else { return nil }
        let alreadyDown = held.values.contains(action)
        held[source] = action
        guard !alreadyDown, hasConsumer, let event = action.event(pressed: true) else { return nil }
        delivered.append(action)
        if repeats, action.isDirection { repeating = (action, now + Self.repeatDelay) }
        return event
    }

    mutating func release(_ source: InputSource) -> ControllerEvent? {
        guard let action = held.removeValue(forKey: source), !held.values.contains(action) else { return nil }
        if repeating?.action == action { repeating = nil }
        guard let index = delivered.firstIndex(of: action) else { return nil }
        delivered.remove(at: index)
        return action.event(pressed: false)
    }

    /// The repeated press that is due at `now`, if any.
    mutating func tick(at now: ContinuousClock.Instant, hasConsumer: Bool, repeats: Bool) -> ControllerEvent? {
        guard let current = repeating, now >= current.at else { return nil }
        guard repeats, hasConsumer, delivered.contains(current.action) else {
            repeating = nil
            return nil
        }
        repeating = (current.action, now + Self.repeatInterval)
        return current.action.event(pressed: true)
    }

    /// Releases for every press the outgoing consumer saw, latest first. Sources still physically held stay silent until
    /// pressed again.
    mutating func detachConsumer() -> [ControllerEvent] {
        let releases = delivered.reversed().compactMap { $0.event(pressed: false) }
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

    private(set) var active: Set<ControlInput> = []

    /// Releases come first, so flicking from left to right releases left before pressing right.
    mutating func update(x: Float, y: Float) -> [(ControlInput, pressed: Bool)] {
        let axes: [(ControlInput, Float)] = [(.right, x), (.left, -x), (.up, y), (.down, -y)]
        var releases: [(ControlInput, pressed: Bool)] = []
        var presses: [(ControlInput, pressed: Bool)] = []
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
