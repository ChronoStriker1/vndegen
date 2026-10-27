import CoreGraphics
import Observation
import VNCore

/// The mouse a game's buttons and sticks drive: buttons mapped to mouse movement, the wheel, or the on-screen keyboard.
/// Its pointer is also the touch cursor's, so the cursor and a stick move the same pointer. `InputHub` hands it every
/// press of those actions and every stick position, and it sends the game pointer and scroll events through the hub's
/// consumer. `PointerView` shows where the pointer is.
@MainActor
@Observable
public final class VirtualMouse {
    /// Normalized over the game's view, so (0, 0) is top left and (1, 1) bottom right.
    public private(set) var location = CGPoint(x: 0.5, y: 0.5)
    /// True once a button or stick moved the pointer in this game, so `PointerView` shows it.
    public private(set) var isShown = false
    /// True while a button asked for the on-screen keyboard.
    public private(set) var showsKeyboard = false
    /// The game view's size in points, which sets how fast the pointer crosses it. `PointerView` keeps it current.
    @ObservationIgnored public var viewSize = CGSize(width: 1024, height: 768)
    /// Where the system's own pointer is over the game, on a Mac, so the controller carries on from the real mouse.
    @ObservationIgnored public var systemPointer: (() -> CGPoint?)?

    /// The hub's consumer.
    @ObservationIgnored var send: ((ControllerEvent) -> Void)?
    @ObservationIgnored private var motion = PointerMotion()
    @ObservationIgnored private var held: [InputSource: ControlAction] = [:]
    @ObservationIgnored private var mover: Task<Void, Never>?
    @ObservationIgnored private var wheelRepeat: (source: InputSource, task: Task<Void, Never>)?

    nonisolated static let frame: Duration = .milliseconds(16)
    /// One notch, in the points `ControllerEvent.scroll` counts. The engines that read the wheel take 40 points as a notch.
    nonisolated static let notch = 40.0

    init() {}

    /// A game starts: the pointer goes back to the middle and hides until a button moves it.
    public func start() {
        releaseAll()
        location = CGPoint(x: 0.5, y: 0.5)
        isShown = false
    }

    /// The touch cursor moved the pointer itself.
    public func place(at point: CGPoint) {
        location = point.clamped
    }

    public func hideKeyboard() {
        showsKeyboard = false
    }

    /// A press of `source` doing `action`. Returns false for actions this does not do, which the hub delivers itself.
    func press(_ source: InputSource, _ action: ControlAction, hasConsumer: Bool) -> Bool {
        guard action.isVirtualMouse else { return false }
        guard held[source] == nil else { return true }
        held[source] = action
        guard hasConsumer else { return true }
        switch action {
        case let .mouseMove(direction):
            // A stick moves the pointer through `moveStick`, by how far it is pushed.
            if case .stick = source { return true }
            motion.held[direction, default: 0] += 1
            startMoving()
        case let .wheel(direction):
            turn(direction)
            wheelRepeat?.task.cancel()
            wheelRepeat = (source, Task { [weak self] in
                try? await Task.sleep(for: PressRouter.repeatDelay)
                while !Task.isCancelled {
                    self?.turn(direction)
                    try? await Task.sleep(for: PressRouter.repeatInterval)
                }
            })
        case .showKeyboard:
            showsKeyboard.toggle()
        default:
            break
        }
        return true
    }

    /// The release of `source`. Returns false when this did not take its press.
    func release(_ source: InputSource) -> Bool {
        guard let action = held.removeValue(forKey: source) else { return false }
        switch action {
        case let .mouseMove(direction):
            if case .stick = source { break }
            if let count = motion.held[direction] { motion.held[direction] = count > 1 ? count - 1 : nil }
        case .wheel:
            if wheelRepeat?.source == source {
                wheelRepeat?.task.cancel()
                wheelRepeat = nil
            }
        default:
            break
        }
        return true
    }

    /// A stick moved. Each of its directions mapped to mouse movement pushes the pointer by as much as the stick
    /// points that way.
    func moveStick(_ controller: ObjectIdentifier, _ stick: Stick, x: Float, y: Float, mapping: ButtonMapping?,
                   hasConsumer: Bool) {
        let push = hasConsumer ? PointerMotion.push(x: Double(x), y: Double(y), stick: stick) { mapping?.action(for: $0) } : .zero
        let key = PointerMotion.StickKey(controller: controller, stick: stick)
        motion.sticks[key] = push == .zero ? nil : push
        startMoving()
    }

    /// A controller went away.
    func forget(controller: ObjectIdentifier) {
        for source in held.keys where source.controller == controller { _ = release(source) }
        motion.sticks = motion.sticks.filter { $0.key.controller != controller }
    }

    /// Lets go of everything, for when the game stops getting input, such as while the VNDegen menu is open.
    /// Buttons and sticks still held then do nothing until they are pressed or moved again.
    func releaseAll() {
        held = [:]
        motion = PointerMotion()
        wheelRepeat?.task.cancel()
        wheelRepeat = nil
        mover?.cancel()
        mover = nil
        showsKeyboard = false
    }

    private func turn(_ direction: WheelDirection) {
        // The wheel turned down scrolls the content down, which is a positive scroll.
        send?(.scroll(dy: direction == .down ? Self.notch : -Self.notch))
    }

    private func startMoving() {
        guard mover == nil, motion.isMoving else { return }
        if let point = systemPointer?() { location = point.clamped }
        mover = Task { [weak self] in
            var last = ContinuousClock.now
            while !Task.isCancelled {
                try? await Task.sleep(for: Self.frame)
                let now = ContinuousClock.now
                guard let self, motion.isMoving else { break }
                step(seconds: (now - last).seconds)
                last = now
            }
            // A cancelled mover may already have a successor.
            if !Task.isCancelled { self?.mover = nil }
        }
    }

    private func step(seconds: Double) {
        let moved = motion.step(location, seconds: seconds, size: viewSize)
        guard moved != location else { return }
        location = moved
        isShown = true
        send?(.pointer(PointerEvent(phase: .moved, location: moved)))
    }
}

/// How fast the pointer moves, from the buttons held and how far each stick is pushed. Speeds are in fractions of the
/// game view's longer side per second, so the pointer crosses any screen in about the same time, at the same speed
/// across and down.
nonisolated struct PointerMotion: Equatable {
    struct StickKey: Hashable {
        let controller: ObjectIdentifier
        let stick: Stick
    }

    /// A stick pushed all the way crosses the screen in a little over a second.
    static let stickSpeed = 0.85
    /// A button moves the pointer slower than a full stick, for aiming at small buttons.
    static let buttonSpeed = 0.4
    /// Sticks rest a little off center.
    static let deadZone = 0.15

    /// Buttons and D-pad directions held, by how many hold each.
    var held: [PointerDirection: Int] = [:]
    /// Each stick's push, in screen directions with y down, from 0 to 1 a side.
    var sticks: [StickKey: CGVector] = [:]

    var isMoving: Bool { !held.isEmpty || !sticks.isEmpty }

    /// Fractions of the longer side per second.
    var velocity: CGVector {
        var velocity = CGVector.zero
        for direction in held.keys {
            velocity.dx += direction.vector.dx * Self.buttonSpeed
            velocity.dy += direction.vector.dy * Self.buttonSpeed
        }
        for push in sticks.values {
            velocity.dx += push.dx * Self.stickSpeed
            velocity.dy += push.dy * Self.stickSpeed
        }
        return velocity
    }

    func step(_ location: CGPoint, seconds: Double, size: CGSize) -> CGPoint {
        guard size.width > 0, size.height > 0 else { return location }
        let side = max(size.width, size.height), velocity = velocity
        return CGPoint(x: location.x + velocity.dx * seconds * side / size.width,
                       y: location.y + velocity.dy * seconds * side / size.height).clamped
    }

    /// A stick at `x` and `y`, with y up, through `action` for each of its directions. The dead zone comes off the
    /// stick's distance from center, and what is left is squared, so a small push moves the pointer slowly for aiming
    /// and a full push moves it fast. Each stick direction mapped to mouse movement then adds as much of that push as
    /// lies its way, so a stick mapped the usual way moves the pointer where it points.
    static func push(x: Double, y: Double, stick: Stick, action: (ControlInput) -> ControlAction?) -> CGVector {
        let distance = (x * x + y * y).squareRoot()
        guard distance > deadZone else { return .zero }
        let past = (min(distance, 1) - deadZone) / (1 - deadZone)
        let strength = past * past
        let along = (x: x / distance * strength, y: y / distance * strength)
        var push = CGVector.zero
        for (side, amount) in [(stick.up, along.y), (stick.down, -along.y), (stick.left, -along.x), (stick.right, along.x)]
        where amount > 0 {
            guard case let .mouseMove(direction)? = action(side) else { continue }
            push.dx += direction.vector.dx * amount
            push.dy += direction.vector.dy * amount
        }
        return push
    }
}

nonisolated extension PointerDirection {
    /// On screen, with y down.
    var vector: CGVector {
        switch self {
        case .up: CGVector(dx: 0, dy: -1)
        case .down: CGVector(dx: 0, dy: 1)
        case .left: CGVector(dx: -1, dy: 0)
        case .right: CGVector(dx: 1, dy: 0)
        }
    }
}

nonisolated extension CGPoint {
    fileprivate var clamped: CGPoint {
        CGPoint(x: min(max(x, 0), 1), y: min(max(y, 0), 1))
    }
}

nonisolated extension Duration {
    fileprivate var seconds: Double {
        let (seconds, attoseconds) = components
        return Double(seconds) + Double(attoseconds) / 1e18
    }
}
