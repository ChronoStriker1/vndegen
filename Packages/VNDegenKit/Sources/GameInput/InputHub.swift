import Foundation
import GameController
import Observation
import VNCore

public enum InputKind: Sendable { case touch, keyboardMouse, gamepad }

/// Merges game controllers, and the hardware keyboard while in the hub, into `ControllerEvent`s.
@MainActor
@Observable
public final class InputHub {
    /// The current consumer: hub navigation or the running game. Set by whoever owns the screen.
    /// Replacing it sends the outgoing consumer a release for every button it still holds,
    /// and the new consumer sees nothing from those buttons until they are pressed again. Events with no consumer are dropped.
    @ObservationIgnored public var onEvent: ((ControllerEvent) -> Void)? {
        didSet {
            repeatTask?.cancel()
            for event in router.detachConsumer() { oldValue?(event) }
        }
    }
    /// When false, keyboard keys are left to the focused view (a running web game reads them itself).
    /// Only `InputMapping.hubKey` is still translated.
    public var translatesKeyboard = true
    /// When true, a held direction repeats its press event, for menu navigation. Games want raw presses, so they set it false.
    public var repeatsDirections = true
    public private(set) var lastInputKind: InputKind = .touch
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

    /// Reconciles with the system's device list, so a repeated or missed notification cannot attach a controller twice.
    private func syncDevices() {
        let pads = GCController.controllers().filter { $0.extendedGamepad != nil }
        let ids = Set(pads.map(ObjectIdentifier.init))
        for id in Set(sticks.keys).subtracting(ids) { detach(id) }
        for pad in pads where sticks[ObjectIdentifier(pad)] == nil { attach(pad) }
        controllerNames = pads.map { $0.vendorName ?? "Controller" }
        GCKeyboard.coalesced?.keyboardInput?.keyChangedHandler = { [weak self] _, _, code, pressed in
            self?.receiveKey(code, pressed: pressed)
        }
    }

    private func attach(_ controller: GCController) {
        guard let pad = controller.extendedGamepad else { return }
        let id = ObjectIdentifier(controller)
        for (element, button) in InputMapping.gamepad {
            element.input(on: pad)?.pressedChangedHandler = { [weak self] _, _, pressed in
                self?.receive(.gamepad(id, element), button, pressed: pressed, kind: .gamepad)
            }
        }
        // Asks the system to hand the Home button to the app. iOS only honors it when the user's settings allow it.
        pad.buttonHome?.preferredSystemGestureState = .disabled
        let dpad: [(GCControllerButtonInput, VirtualButton)] = [
            (pad.dpad.up, .up), (pad.dpad.down, .down), (pad.dpad.left, .left), (pad.dpad.right, .right),
        ]
        for (input, direction) in dpad {
            input.pressedChangedHandler = { [weak self] _, _, pressed in
                self?.receive(.dpad(id), direction, pressed: pressed, kind: .gamepad)
            }
        }
        sticks[id] = StickDirections()
        pad.leftThumbstick.valueChangedHandler = { [weak self] _, x, y in
            self?.moveStick(id, x: x, y: y)
        }
    }

    private func detach(_ id: ObjectIdentifier) {
        for (source, button) in router.heldSources where source.controller == id {
            receive(source, button, pressed: false, kind: .gamepad)
        }
        sticks[id] = nil
    }

    private func moveStick(_ id: ObjectIdentifier, x: Float, y: Float) {
        guard var stick = sticks[id] else { return }
        let changes = stick.update(x: x, y: y)
        sticks[id] = stick
        for (direction, pressed) in changes {
            receive(.stick(id), direction, pressed: pressed, kind: .gamepad)
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
        receive(.key(code.rawValue), button, pressed: pressed, kind: .keyboardMouse)
    }

    private func receive(_ source: InputSource, _ button: VirtualButton, pressed: Bool, kind: InputKind) {
        if pressed { noteInput(kind) }
        let event = router.update(source, button, pressed: pressed, at: .now,
                                  hasConsumer: onEvent != nil, repeats: repeatsDirections)
        guard let event else { return }
        if pressed, button.isDirection, router.repeating != nil { scheduleRepeat() }
        onEvent?(event)
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
