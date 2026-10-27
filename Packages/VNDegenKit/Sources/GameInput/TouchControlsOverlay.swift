import SwiftUI
import VNCore

/// What touches do in a running game.
nonisolated public enum TouchControlsMode: Sendable {
    /// The touch gamepad. Touches between its buttons reach the game.
    case gamepad
    /// Dragging moves a virtual cursor.
    case cursor
    /// No overlay. Touches reach the game where they land.
    case direct
    /// No overlay, and the game ignores touches.
    case off

    /// Whether `TouchControlsOverlay` draws anything in this mode.
    public var hasOverlay: Bool { self == .gamepad || self == .cursor }
}

/// Reports a touch gamepad button going down or up. A nil input is the Home button.
typealias TouchPress = (ControlInput?, _ pressed: Bool) -> Void

/// On-screen controls drawn over a running game. Gamepad mode presses buttons through `input`, which does what its
/// button mapping says, the same as for a controller. `mapping` is that same mapping, for the buttons' badges. Cursor
/// mode moves the virtual mouse's pointer, which a controller's sticks can move too, and sends pointer events. Both have a Home button that opens the VNDegen menu. The modes
/// without an overlay draw nothing.
/// Place it in a `ZStack` over the session view. The gamepad controls keep to the safe area, while the cursor surface covers
/// the full screen like the session view, so its normalized pointer locations match the game's.
public struct TouchControlsOverlay: View {
    let mode: TouchControlsMode
    let mapping: ButtonMapping
    let input: InputHub
    @State private var taps = 0

    public init(mode: TouchControlsMode, mapping: ButtonMapping, input: InputHub) {
        self.mode = mode
        self.mapping = mapping
        self.input = input
    }

    public var body: some View {
        GeometryReader { geometry in
            let unit = min(max(min(geometry.size.width, geometry.size.height) / 6.2, 54), 76)
            switch mode {
            case .gamepad: GamepadControls(unit: unit, mapping: mapping, press: press)
            case .cursor:
                ZStack(alignment: .topTrailing) {
                    MouseControls(mouse: input.mouse, send: sendPointer)
                    HomeButton(size: unit * 0.62, press: press)
                        .padding(.top, 8)
                        .padding(.trailing, 16)
                }
            case .direct, .off: EmptyView()
            }
        }
        .environment(\.colorScheme, .dark)
        .sensoryFeedback(.impact(weight: .light, intensity: 0.7), trigger: taps)
    }

    private func press(_ control: ControlInput?, _ pressed: Bool) {
        if pressed, control.map(mapping.action) != .nothing { taps += 1 }
        input.touch(control, pressed: pressed)
    }

    private func sendPointer(_ event: ControllerEvent) {
        if case let .pointer(pointer) = event, pointer.phase == .began { taps += 1 }
        input.touchPointer(event)
    }
}

/// Opens the VNDegen menu. Never remapped, so the player can always get back to the menu.
private struct HomeButton: View {
    let size: CGFloat
    let press: TouchPress

    var body: some View {
        TouchButton(nil, title: "VNDegen menu", press: press) { pressed in
            RoundKey(symbol: "house.fill", size: size, pressed: pressed)
        }
    }
}

// MARK: - Gamepad mode

private struct GamepadControls: View {
    let unit: CGFloat
    let mapping: ButtonMapping
    let press: TouchPress

    var body: some View {
        VStack(spacing: unit * 0.28) {
            Spacer(minLength: 0)
            // The triggers sit above the bumpers, as on an Xbox controller.
            HStack {
                shoulder(.l2)
                Spacer(minLength: 0)
                shoulder(.r2)
            }
            HStack(spacing: unit * 0.2) {
                shoulder(.l)
                HomeButton(size: unit * 0.62, press: press)
                Spacer(minLength: 0)
                MappedButton(input: .view, mapping: mapping, press: press) { pressed in
                    RoundKey(symbol: "rectangle.on.rectangle", size: unit * 0.62, pressed: pressed)
                }
                MappedButton(input: .menu, mapping: mapping, press: press) { pressed in
                    RoundKey(symbol: "line.3.horizontal", size: unit * 0.62, pressed: pressed)
                }
                shoulder(.r)
            }
            HStack(alignment: .bottom) {
                DPad(size: unit * 2.5, mapping: mapping, press: press)
                Spacer(minLength: 0)
                FaceButtons(unit: unit, mapping: mapping, press: press)
            }
        }
        .padding(.horizontal, unit * 0.3)
        .padding(.bottom, unit * 0.25)
    }

    private func shoulder(_ input: ControlInput) -> some View {
        MappedButton(input: input, mapping: mapping, press: press) { pressed in
            Text(input.title)
                .font(.system(size: unit * 0.3, weight: .bold, design: .rounded))
                .foregroundStyle(.white.opacity(0.9))
                .frame(width: unit * 1.2, height: unit * 0.6)
                .background(KeyFill(shape: Capsule(), pressed: pressed))
        }
    }
}

/// A touch gamepad button that does what the mapping says. A remapped button shows its binding in a small badge, and a
/// button bound to nothing dims.
private struct MappedButton<Label: View>: View {
    let input: ControlInput
    let mapping: ButtonMapping
    let press: TouchPress
    @ViewBuilder let label: (Bool) -> Label

    var body: some View {
        let action = mapping.action(for: input)
        TouchButton(input, title: input.title, press: press, label: label)
            .overlay(alignment: .topTrailing) {
                if mapping.isRemapped(input), let badge = action.badge { Badge(text: badge).offset(x: 6, y: -6) }
            }
            .opacity(action == .nothing ? 0.35 : 1)
            .accessibilityValue(mapping.isRemapped(input) ? action.title : "")
    }
}

private struct Badge: View {
    let text: String

    var body: some View {
        Text(text)
            .font(.system(size: 11, weight: .heavy, design: .rounded))
            .foregroundStyle(.black)
            .lineLimit(1)
            .fixedSize()
            .padding(.horizontal, 5)
            .frame(minWidth: 18, minHeight: 18)
            .background(.white.opacity(0.92), in: .capsule)
            .allowsHitTesting(false)
    }
}

/// Xbox layout: A at the bottom, B right, X left, Y top.
private struct FaceButtons: View {
    let unit: CGFloat
    let mapping: ButtonMapping
    let press: TouchPress

    var body: some View {
        let spread = unit * 0.82
        ZStack {
            face(.y, Color(red: 1, green: 0.83, blue: 0.36)).offset(y: -spread)
            face(.x, Color(red: 0.36, green: 0.66, blue: 1)).offset(x: -spread)
            face(.b, Color(red: 1, green: 0.42, blue: 0.42)).offset(x: spread)
            face(.a, Color(red: 0.42, green: 0.83, blue: 0.42)).offset(y: spread)
        }
        .frame(width: unit + spread * 2, height: unit + spread * 2)
    }

    private func face(_ input: ControlInput, _ tint: Color) -> some View {
        MappedButton(input: input, mapping: mapping, press: press) { pressed in
            Text(input.title)
                .font(.system(size: unit * 0.4, weight: .heavy, design: .rounded))
                .foregroundStyle(tint)
                .frame(width: unit, height: unit)
                .background(KeyFill(shape: Circle(), pressed: pressed))
        }
    }
}

/// One finger drives the whole pad, so a thumb can roll between directions. Diagonals hold two directions.
private struct DPad: View {
    let size: CGFloat
    let mapping: ButtonMapping
    let press: TouchPress
    @State private var held: Set<ControlInput> = []

    var body: some View {
        let arm = size * 0.34
        ZStack {
            DPadCross(arm: arm, length: size * 0.94).fill(.ultraThinMaterial).opacity(0.7)
            // The pressed arms light up, from the center out.
            ForEach(Self.directions, id: \.self) { direction in
                Rectangle()
                    .fill(.white.opacity(held.contains(direction) ? 0.4 : 0.06))
                    .frame(width: arm, height: size * 0.47)
                    .offset(y: -size * 0.235)
                    .rotationEffect(direction.armAngle)
            }
            .clipShape(DPadCross(arm: arm, length: size * 0.94))
            DPadCross(arm: arm, length: size * 0.94).stroke(.white.opacity(0.22), lineWidth: 1)
            Circle().fill(.black.opacity(0.18)).frame(width: arm * 0.5)
            ForEach(Self.directions, id: \.self) { direction in
                Image(systemName: "arrowtriangle.up.fill")
                    .font(.system(size: arm * 0.24))
                    .foregroundStyle(.white.opacity(held.contains(direction) ? 1 : 0.75))
                    .offset(y: -size * 0.33)
                    .rotationEffect(direction.armAngle)
                    .opacity(mapping.action(for: direction) == .nothing ? 0.35 : 1)
            }
            // Badges sit beside each arm's outer end, unrotated so they read upright.
            ForEach(Self.directions, id: \.self) { direction in
                if mapping.isRemapped(direction), let badge = mapping.action(for: direction).badge {
                    let angle = direction.armAngle.radians
                    Badge(text: badge)
                        .offset(x: sin(angle) * size * 0.44 + cos(angle) * size * 0.17,
                                y: -cos(angle) * size * 0.44 + sin(angle) * size * 0.17)
                }
            }
        }
        .shadow(color: .black.opacity(0.25), radius: 4, y: 2)
        .animation(.easeOut(duration: 0.08), value: held)
        .frame(width: size, height: size)
        .overlay {
            TouchSurface(circular: true) { location in
                let directions = location.map {
                    dpadDirections(dx: $0.x - size / 2, dy: $0.y - size / 2, deadZone: size * 0.12)
                } ?? []
                let old = held
                held = directions
                for direction in Self.directions where old.contains(direction) && !directions.contains(direction) { press(direction, false) }
                for direction in Self.directions where directions.contains(direction) && !old.contains(direction) { press(direction, true) }
            }
        }
        .onDisappear {
            for direction in held { press(direction, false) }
            held = []
        }
        .accessibilityElement()
        .accessibilityLabel("Direction pad")
    }

    private static let directions: [ControlInput] = [.up, .right, .down, .left]
}

/// Directions for a touch offset from the pad's center, with y growing downward. Each direction covers 135 degrees,
/// so the 45 degrees between two neighbors press both.
nonisolated func dpadDirections(dx: CGFloat, dy: CGFloat, deadZone: CGFloat) -> Set<ControlInput> {
    let distance = (dx * dx + dy * dy).squareRoot()
    guard distance > deadZone else { return [] }
    let cos = dx / distance, sin = -dy / distance
    let edge = 0.38 // cos(67.5 degrees)
    var result: Set<ControlInput> = []
    if cos > edge { result.insert(.right) }
    if cos < -edge { result.insert(.left) }
    if sin > edge { result.insert(.up) }
    if sin < -edge { result.insert(.down) }
    return result
}

private extension ControlInput {
    var armAngle: Angle {
        switch self {
        case .right: .degrees(90)
        case .down: .degrees(180)
        case .left: .degrees(270)
        default: .zero
        }
    }
}

/// A plus sign with rounded corners, centered in its rect: one piece, like a controller's D-pad.
nonisolated private struct DPadCross: Shape {
    let arm: CGFloat
    let length: CGFloat

    func path(in rect: CGRect) -> Path {
        let a = arm / 2, b = length / 2
        let c = CGPoint(x: rect.midX, y: rect.midY)
        // Clockwise from the top arm's top left corner. Every third corner, where two arms meet, turns inward.
        let corners: [(CGFloat, CGFloat)] = [
            (-a, -b), (a, -b), (a, -a), (b, -a), (b, a), (a, a), (a, b), (-a, b), (-a, a), (-b, a), (-b, -a), (-a, -a),
        ]
        let points = corners.map { CGPoint(x: c.x + $0.0, y: c.y + $0.1) }
        var path = Path()
        path.move(to: CGPoint(x: c.x, y: c.y - b))
        for index in points.indices {
            let inner = index % 3 == 2
            path.addArc(tangent1End: points[(index + 1) % points.count], tangent2End: points[(index + 2) % points.count],
                        radius: arm * (inner ? 0.1 : 0.2))
        }
        path.closeSubpath()
        return path
    }
}

/// Presses when a finger lands and releases when it lifts or the system cancels the touch.
/// Each button tracks its own finger, so several can be held at once.
private struct TouchButton<Label: View>: View {
    let control: ControlInput?
    let title: String
    let press: TouchPress
    let label: (Bool) -> Label
    @State private var pressed = false

    init(_ control: ControlInput?, title: String, press: @escaping TouchPress, @ViewBuilder label: @escaping (Bool) -> Label) {
        self.control = control
        self.title = title
        self.press = press
        self.label = label
    }

    var body: some View {
        label(pressed)
            .scaleEffect(pressed ? 0.92 : 1)
            .animation(.easeOut(duration: 0.08), value: pressed)
            .overlay {
                TouchSurface { location in
                    let now = location != nil
                    guard now != pressed else { return }
                    pressed = now
                    press(control, now)
                }
            }
            .onDisappear {
                if pressed { press(control, false) }
                pressed = false
            }
            .accessibilityElement()
            .accessibilityLabel(title)
            .accessibilityAddTraits(.isButton)
    }
}

#if canImport(UIKit)
/// Reports one finger's position from the moment it lands, as it moves, and nil when it lifts or the system cancels it.
/// UIKit hands a gesture recognizer each touch as it lands. A SwiftUI DragGesture here saw a touch-down only once the
/// finger lifted, so every button pressed and released together.
private struct TouchSurface: UIViewRepresentable {
    var circular = false
    let touch: (CGPoint?) -> Void

    func makeUIView(context: Context) -> SurfaceView {
        let view = SurfaceView()
        view.backgroundColor = .clear
        view.addGestureRecognizer(TouchRecognizer())
        return view
    }

    func updateUIView(_ view: SurfaceView, context: Context) {
        view.circular = circular
        (view.gestureRecognizers?.first as? TouchRecognizer)?.report = touch
    }

    final class SurfaceView: UIView {
        var circular = false

        override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
            guard circular else { return super.point(inside: point, with: event) }
            let dx = point.x - bounds.midX, dy = point.y - bounds.midY, radius = min(bounds.width, bounds.height) / 2
            return dx * dx + dy * dy <= radius * radius
        }
    }

    /// Never recognizes, so it blocks no other gesture, and nothing else can stop it from seeing its touch.
    final class TouchRecognizer: UIGestureRecognizer {
        var report: (CGPoint?) -> Void = { _ in }
        private var finger: UITouch?

        override init(target: Any?, action: Selector?) {
            super.init(target: target, action: action)
            cancelsTouchesInView = false
            delaysTouchesBegan = false
            delaysTouchesEnded = false
        }

        override func canPrevent(_ preventedGestureRecognizer: UIGestureRecognizer) -> Bool { false }
        override func canBePrevented(by preventingGestureRecognizer: UIGestureRecognizer) -> Bool { false }

        override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent) {
            guard finger == nil, let touch = touches.first else { return }
            finger = touch
            report(touch.location(in: view))
        }

        override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent) {
            if let finger, touches.contains(finger) { report(finger.location(in: view)) }
        }

        override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent) {
            lift(touches)
        }

        override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent) { lift(touches) }

        override func reset() {
            super.reset()
            if finger != nil {
                finger = nil
                report(nil)
            }
        }

        private func lift(_ touches: Set<UITouch>) {
            guard let finger, touches.contains(finger) else { return }
            self.finger = nil
            report(nil)
            state = .failed
        }
    }
}
#else
/// The touch gamepad shows only on touch devices.
private struct TouchSurface: View {
    var circular = false
    let touch: (CGPoint?) -> Void

    var body: some View { Color.clear }
}
#endif

private struct KeyFill<S: InsettableShape>: View {
    let shape: S
    let pressed: Bool

    var body: some View {
        ZStack {
            shape.fill(.ultraThinMaterial).opacity(0.7)
            shape.fill(.white.opacity(pressed ? 0.4 : 0.06))
            shape.strokeBorder(.white.opacity(pressed ? 0.6 : 0.22), lineWidth: 1)
        }
        .shadow(color: .black.opacity(0.25), radius: 4, y: 2)
    }
}

private struct RoundKey: View {
    let symbol: String
    let size: CGFloat
    let pressed: Bool

    var body: some View {
        Image(systemName: symbol)
            .font(.system(size: size * 0.4, weight: .semibold))
            .foregroundStyle(.white.opacity(0.9))
            .frame(width: size, height: size)
            .background(KeyFill(shape: Circle(), pressed: pressed))
    }
}

// MARK: - Cursor mode

private struct MouseControls: View {
    let mouse: VirtualMouse
    let send: (ControllerEvent) -> Void

    private var cursor: CGPoint { mouse.location }

    var body: some View {
        GeometryReader { geometry in
            ZStack(alignment: .topLeading) {
                #if canImport(UIKit)
                TrackpadSurface(handle: handle)
                #endif
                PointerArrow(location: cursor, size: geometry.size)
            }
        }
        .ignoresSafeArea()
    }

    private func handle(_ gesture: TrackpadGesture) {
        switch gesture {
        case let .move(delta):
            moveCursor(by: delta)
            send(.pointer(PointerEvent(phase: .moved, location: cursor)))
        case let .press(phase):
            send(.pointer(PointerEvent(phase: phase, location: cursor)))
        case let .drag(delta):
            moveCursor(by: delta)
            send(.pointer(PointerEvent(phase: .moved, location: cursor)))
        case let .click(button):
            send(.pointer(PointerEvent(phase: .began, location: cursor, button: button)))
            send(.pointer(PointerEvent(phase: .ended, location: cursor, button: button)))
        case let .scroll(dy):
            send(.scroll(dy: dy))
        }
    }

    private func moveCursor(by delta: CGVector) {
        mouse.place(at: CGPoint(x: cursor.x + delta.dx, y: cursor.y + delta.dy))
    }
}

enum TrackpadGesture {
    /// Relative motion as a fraction of the surface size.
    case move(CGVector)
    /// Touch and hold, then drag, holds the primary button down.
    case press(PointerEvent.Phase)
    case drag(CGVector)
    case click(PointerEvent.Button)
    /// Points, positive when the content should scroll down, as with a natural-scrolling trackpad.
    case scroll(Double)
}

#if canImport(UIKit)
import UIKit

/// SwiftUI has no two-finger tap, so the trackpad uses UIKit recognizers.
private struct TrackpadSurface: UIViewRepresentable {
    let handle: (TrackpadGesture) -> Void

    func makeCoordinator() -> Coordinator { Coordinator() }

    func makeUIView(context: Context) -> UIView {
        let view = UIView()
        view.backgroundColor = .clear
        view.isMultipleTouchEnabled = true
        let coordinator = context.coordinator

        let hold = UILongPressGestureRecognizer(target: coordinator, action: #selector(Coordinator.hold))
        hold.minimumPressDuration = 0.35
        hold.allowableMovement = 8
        let pan = UIPanGestureRecognizer(target: coordinator, action: #selector(Coordinator.pan))
        pan.maximumNumberOfTouches = 1
        let scroll = UIPanGestureRecognizer(target: coordinator, action: #selector(Coordinator.scroll))
        scroll.minimumNumberOfTouches = 2
        scroll.maximumNumberOfTouches = 2
        let tap = UITapGestureRecognizer(target: coordinator, action: #selector(Coordinator.tap))
        let twoFingerTap = UITapGestureRecognizer(target: coordinator, action: #selector(Coordinator.twoFingerTap))
        twoFingerTap.numberOfTouchesRequired = 2
        tap.require(toFail: twoFingerTap)
        for recognizer in [hold, pan, scroll, tap, twoFingerTap] { view.addGestureRecognizer(recognizer) }
        return view
    }

    func updateUIView(_ view: UIView, context: Context) {
        context.coordinator.handle = handle
    }

    final class Coordinator: NSObject {
        static let sensitivity: CGFloat = 1.6
        var handle: (TrackpadGesture) -> Void = { _ in }
        private var last: CGPoint = .zero

        private func normalized(_ x: CGFloat, _ y: CGFloat, in view: UIView?) -> CGVector {
            guard let size = view?.bounds.size, size.width > 0, size.height > 0 else { return .zero }
            return CGVector(dx: x / size.width * Self.sensitivity, dy: y / size.height * Self.sensitivity)
        }

        // Pans read the translation on every state with one, so a short flick that goes straight from began to ended still counts.
        private static let moving: Set<UIGestureRecognizer.State> = [.began, .changed, .ended]

        @objc func pan(_ recognizer: UIPanGestureRecognizer) {
            guard Self.moving.contains(recognizer.state) else { return }
            let translation = recognizer.translation(in: recognizer.view)
            recognizer.setTranslation(.zero, in: recognizer.view)
            handle(.move(normalized(translation.x, translation.y, in: recognizer.view)))
        }

        @objc func hold(_ recognizer: UILongPressGestureRecognizer) {
            let point = recognizer.location(in: recognizer.view)
            switch recognizer.state {
            case .began: handle(.press(.began))
            case .changed: handle(.drag(normalized(point.x - last.x, point.y - last.y, in: recognizer.view)))
            case .ended: handle(.press(.ended))
            case .cancelled, .failed: handle(.press(.cancelled))
            default: break
            }
            last = point
        }

        @objc func scroll(_ recognizer: UIPanGestureRecognizer) {
            guard Self.moving.contains(recognizer.state) else { return }
            let translation = recognizer.translation(in: recognizer.view)
            guard translation.y != 0 else { return }
            recognizer.setTranslation(.zero, in: recognizer.view)
            handle(.scroll(-translation.y))
        }

        @objc func tap(_ recognizer: UITapGestureRecognizer) { handle(.click(.primary)) }
        @objc func twoFingerTap(_ recognizer: UITapGestureRecognizer) { handle(.click(.secondary)) }
    }
}
#endif
