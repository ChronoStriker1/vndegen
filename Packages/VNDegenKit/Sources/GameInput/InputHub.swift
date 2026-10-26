import Foundation
import GameController
import Observation
import VNCore

nonisolated public enum InputKind: Sendable { case touch, keyboardMouse, gamepad }

/// Merges game controllers, and the hardware keyboard while in the hub, into `ControllerEvent`s.
@MainActor
@Observable
public final class InputHub {
    /// The current consumer: hub navigation or the running game. Set by whoever owns the screen.
    /// Replacing it sends the outgoing consumer a release for every button it still holds,
    /// and the new consumer sees nothing from those buttons until they are pressed again. Events with no consumer are dropped.
    /// Replacing it also restores the standard button mapping.
    @ObservationIgnored public var onEvent: ((ControllerEvent) -> Void)? {
        didSet {
            repeatTask?.cancel()
            for event in router.detachConsumer() { oldValue?(event) }
            buttonMapping = ButtonMapping()
        }
    }
    /// What controller buttons do. A running game sets its own mapping while it has input. The hub's screens and the
    /// in-game menu keep the standard one, so a remapped button can never block their navigation.
    @ObservationIgnored public var buttonMapping = ButtonMapping()
    /// When false, keyboard keys are left to the focused view (a running web game reads them itself).
    /// Only `InputMapping.hubKey` is still translated.
    public var translatesKeyboard = true
    /// When true, a held direction repeats its press event, for menu navigation. Games want raw presses, so they set it false.
    public var repeatsDirections = true
    public private(set) var lastInputKind: InputKind = Platform.current == .macOS ? .keyboardMouse : .touch
    public private(set) var controllerNames: [String] = []

    @ObservationIgnored private var router = PressRouter()
    @ObservationIgnored private var sticks: [ObjectIdentifier: StickDirections] = [:]
    @ObservationIgnored private var repeatTask: Task<Void, Never>?
    @ObservationIgnored private var observers: [NSObjectProtocol] = []

    public init() {}

    /// Starts watching controllers and the keyboard. Calling it again does nothing.
    public func start() {
        guard observers.isEmpty else { return }
        for name in [Notification.Name.GCControllerDidConnect, .GCControllerDidDisconnect, .GCKeyboardDidConnect] {
            observers.append(NotificationCenter.default.addObserver(forName: name, object: nil, queue: .main) { [weak self] _ in
                MainActor.assumeIsolated { self?.syncDevices() }
            })
        }
        syncDevices()
    }

    /// The hub reports input that does not pass through here, such as touches and pointer clicks on its own views.
    public func noteInput(_ kind: InputKind) {
        if lastInputKind != kind { lastInputKind = kind }
    }

    /// A touch gamepad button went down or up, or the touch Home button when `input` is nil. Touch buttons share the
    /// controllers' press tracking, so a touch button and a controller button doing the same thing make one press.
    public func touch(_ input: ControlInput?, pressed: Bool) {
        receive(.touch(input), pressed ? input.map(buttonMapping.action) ?? .button(.hub) : nil, kind: .touch)
    }

    /// A pointer or scroll event from the touch cursor. These hold no button the router tracks, so they go straight to
    /// the consumer.
    public func touchPointer(_ event: ControllerEvent) {
        noteInput(.touch)
        onEvent?(event)
    }

    /// Takes the keyboard and the controllers' Home buttons back from a game that read them itself, such as Ren'Py on
    /// iOS, whose SDL replaces the keyboard handler and hands the Home button back to the system when it quits.
    public func reclaimDevices() {
        guard !observers.isEmpty else { return }
        syncDevices()
    }

    /// Reconciles with the system's device list, so a repeated or missed notification cannot attach a controller twice.
    private func syncDevices() {
        var pads = GCController.controllers().filter { $0.extendedGamepad != nil }
        #if DEBUG
        // The simulator exposes a virtual gamepad, which hides the touch overlay. `-ignoreControllers YES` shows it.
        if UserDefaults.standard.bool(forKey: "ignoreControllers") { pads = [] }
        #endif
        let ids = Set(pads.map(ObjectIdentifier.init))
        for id in Set(sticks.keys).subtracting(ids) { detach(id) }
        for pad in pads where sticks[ObjectIdentifier(pad)] == nil { attach(pad) }
        controllerNames = pads.map { $0.vendorName ?? "Controller" }
        for pad in pads {
            // Asks the system to hand the Home button to the app. iOS only honors it when the user's settings allow it.
            pad.extendedGamepad?.buttonHome?.preferredSystemGestureState = .disabled
        }
        GCKeyboard.coalesced?.handlerQueue = .main
        GCKeyboard.coalesced?.keyboardInput?.keyChangedHandler = { [weak self] _, _, code, pressed in
            self?.receiveKey(code, pressed: pressed)
        }
    }

    private func attach(_ controller: GCController) {
        guard let pad = controller.extendedGamepad else { return }
        let id = ObjectIdentifier(controller)
        for element in GamepadElement.allCases {
            element.input(on: pad)?.pressedChangedHandler = { [weak self] _, _, pressed in
                guard let self else { return }
                receive(.gamepad(id, element), pressed ? element.action(in: buttonMapping) : nil, kind: .gamepad)
            }
        }
        let dpad: [(GCControllerButtonInput, ControlInput)] = [
            (pad.dpad.up, .up), (pad.dpad.down, .down), (pad.dpad.left, .left), (pad.dpad.right, .right),
        ]
        for (input, direction) in dpad {
            input.pressedChangedHandler = { [weak self] _, _, pressed in
                guard let self else { return }
                receive(.dpad(id, direction), pressed ? buttonMapping.action(for: direction) : nil, kind: .gamepad)
            }
        }
        sticks[id] = StickDirections()
        pad.leftThumbstick.valueChangedHandler = { [weak self] _, x, y in
            self?.moveStick(id, x: x, y: y)
        }
    }

    private func detach(_ id: ObjectIdentifier) {
        for source in router.heldSources where source.controller == id {
            receive(source, nil, kind: .gamepad)
        }
        sticks[id] = nil
    }

    private func moveStick(_ id: ObjectIdentifier, x: Float, y: Float) {
        guard var stick = sticks[id] else { return }
        let changes = stick.update(x: x, y: y)
        sticks[id] = stick
        for (direction, pressed) in changes {
            receive(.stick(id, direction), pressed ? buttonMapping.action(for: direction) : nil, kind: .gamepad)
        }
    }

    private func receiveKey(_ code: GCKeyCode, pressed: Bool) {
        guard let button = InputMapping.keyboard[code] else {
            if pressed { noteInput(.keyboardMouse) }
            return
        }
        // Releases always go through, so a key held while translation switches off still lets go.
        if pressed, !translatesKeyboard, code != InputMapping.hubKey {
            noteInput(.keyboardMouse)
            return
        }
        receive(.key(code.rawValue), pressed ? .button(button) : nil, kind: .keyboardMouse)
    }

    /// A press of `source` doing `action`, or its release when `action` is nil.
    private func receive(_ source: InputSource, _ action: ControlAction?, kind: InputKind) {
        let event: ControllerEvent?
        if let action {
            noteInput(kind)
            event = router.press(source, action, at: .now, hasConsumer: onEvent != nil, repeats: repeatsDirections)
            if event != nil, router.repeating?.action == action { scheduleRepeat() }
        } else {
            event = router.release(source)
        }
        if let event { onEvent?(event) }
    }

    private func scheduleRepeat() {
        repeatTask?.cancel()
        repeatTask = Task { [weak self] in
            while let due = self?.router.repeating?.at {
                try? await Task.sleep(until: due, clock: .continuous)
                guard !Task.isCancelled, let self else { return }
                let event = router.tick(at: .now, hasConsumer: onEvent != nil, repeats: repeatsDirections)
                if let event { onEvent?(event) }
            }
        }
    }
}
