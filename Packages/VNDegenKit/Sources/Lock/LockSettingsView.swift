import SwiftUI
import VNCore
import ControllerUI

/// The state behind `LockSettingsView`, including gamepad focus and the passcode flow it opens. A host that drives the
/// view with a controller keeps the model and forwards button presses to `handle(_:)`.
@Observable
public final class LockSettingsModel {
    enum Target: Hashable { case setPasscode, changePasscode, removePasscode, biometrics }

    let lock: LockService
    /// The passcode being set, changed, or removed, in a sheet over the settings.
    var entry: PasscodeEntry?
    var focus = FocusRing<Target>()

    public init(lock: LockService) { self.lock = lock }

    var rows: [[Target]] {
        (lock.hasPasscode ? [[.changePasscode], [.removePasscode]] : [[.setPasscode]])
            + (lock.biometry == .none ? [] : [[.biometrics]])
    }

    func isFocused(_ target: Target) -> Bool { focus.isFocused(target, in: rows) }

    /// Directions move focus and confirm activates it. While a passcode sheet is up, the keypad takes every button, and
    /// cancel deletes a digit or, with none left, closes the sheet. Returns false for buttons the host should handle, such
    /// as cancel with no sheet up.
    public func handle(_ button: VirtualButton) -> Bool {
        if let entry, let flow = entry.flow {
            switch entry.handle(.button(button, pressed: true), expectedLength: flow.expectedLength(current: lock.passcodeLength)) {
            case .none: break
            case let .submit(code): submit(code)
            case .cancel: closeFlow()
            }
            return true
        }
        switch button {
        case .confirm:
            focus.showsFocus = true
            if let target = focus.current(in: rows) { activate(target) }
            return true
        default:
            return focus.move(button, in: rows)
        }
    }

    /// Touch and pointer: focus the control without showing the ring, then activate it.
    func select(_ target: Target) {
        focus.select(target)
        activate(target)
    }

    private func activate(_ target: Target) {
        switch target {
        case .setPasscode:
            Task { if await lock.authorizeCredentialChange() { open(.create) } }
        case .changePasscode: open(.change)
        case .removePasscode: open(.remove)
        case .biometrics:
            guard lock.biometry != .none else { return }
            let enabled = !lock.biometricsEnabled
            Task { await lock.setBiometricsEnabled(enabled) }
        }
    }

    private func open(_ purpose: PasscodeFlow.Purpose) {
        entry = PasscodeEntry(flow: PasscodeFlow(purpose))
    }

    func closeFlow() {
        entry = nil
    }

    func submit(_ code: String) {
        guard let entry, var flow = entry.flow else { return }
        // Checking the current passcode also unlocks the library, which is fine: the user just proved they know it.
        switch flow.submit(code, isCurrent: lock.unlock(passcode:)) {
        case .next:
            entry.flow = flow
            entry.message = nil
        case let .retry(message):
            entry.flow = flow
            entry.reject(message)
        case let .finished(passcode):
            if lock.setPasscode(passcode) {
                closeFlow()
            } else {
                entry.flow = PasscodeFlow(flow.purpose)
                entry.reject("The passcode could not be saved. Try again.")
            }
        }
    }
}

/// Passcode and biometrics options, shown inside the app's Settings screen.
public struct LockSettingsView: View {
    @State private var model: LockSettingsModel

    public init(lock: LockService) { _model = State(initialValue: LockSettingsModel(lock: lock)) }
    public init(model: LockSettingsModel) { _model = State(initialValue: model) }

    private var lock: LockService { model.lock }

    public var body: some View {
        Form {
            Section {
                if lock.hasPasscode {
                    Button("Change passcode") { model.select(.changePasscode) }
                        .focusRow(model.isFocused(.changePasscode), id: LockSettingsModel.Target.changePasscode)
                    Button("Remove passcode", role: .destructive) { model.select(.removePasscode) }
                        .focusRow(model.isFocused(.removePasscode), id: LockSettingsModel.Target.removePasscode)
                } else {
                    Button("Set passcode") { model.select(.setPasscode) }
                        .focusRow(model.isFocused(.setPasscode), id: LockSettingsModel.Target.setPasscode)
                }
            } header: {
                // The page around it is titled Hidden games already.
                Text("Passcode")
            } footer: {
                Text(lock.needsPasscodeSetup
                     ? "Set a passcode before you lock games."
                     : "Locked games stay hidden until you unlock them with \(unlockMethods).")
            }
            Section {
                // Off while the device has no Face ID or Touch ID, whatever the stored choice, so it never reads as on.
                Toggle(isOn: Binding(get: { lock.canUseBiometrics }, set: { _ in model.select(.biometrics) })) {
                    Label("Use \(lock.biometry.name)", systemImage: lock.biometry.symbol)
                }
                .disabled(lock.biometry == .none)
                .focusRow(model.isFocused(.biometrics), id: LockSettingsModel.Target.biometrics)
            } footer: {
                if lock.biometry == .none {
                    Text("This device has no Face ID or Touch ID set up.")
                }
            }
        }
        .onAppear { lock.refreshBiometry() }
        .sheet(isPresented: Binding(get: { model.entry != nil }, set: { if !$0 { model.closeFlow() } })) {
            if let entry = model.entry { PasscodeFlowSheet(model: model, entry: entry) }
        }
    }

    private var unlockMethods: String {
        switch (lock.canUseBiometrics, lock.hasPasscode) {
        case (true, true): "\(lock.biometry.name) or your passcode"
        case (true, false): lock.biometry.name
        default: "your passcode"
        }
    }
}

private struct PasscodeFlowSheet: View {
    let model: LockSettingsModel
    let entry: PasscodeEntry
    @Environment(\.cameraCornerTrailing) private var cameraCorner

    /// Laid out as the unlock screen is: Cancel in the top corner, then the flow's name over the keypad's prompt, with the
    /// keypad centered in the space under Cancel.
    var body: some View {
        ZStack {
            LockBackdrop()
            VStack(spacing: 0) {
                LockCancelButton(escapes: true, action: model.closeFlow)
                    .padding(20)
                    .frame(maxWidth: .infinity, alignment: .leading)
                GeometryReader { geometry in
                    if let flow = entry.flow {
                        // The keys shrink to fit a short window, such as a phone held sideways.
                        let keySize = min(72, max(40, (geometry.size.height - 80) / 5.1))
                        VStack(spacing: 6) {
                            Text(flow.purpose.title)
                                .font(.subheadline.weight(.semibold))
                                .foregroundStyle(.white.opacity(0.6))
                            PasscodeKeypad(entry: entry, title: flow.title, expectedLength: flow.expectedLength(current: model.lock.passcodeLength),
                                           keySize: keySize, onSubmit: model.submit)
                        }
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                    }
                }
            }
        }
        // Beside a camera in the top corner the system insets that whole side, which would push the keypad off center
        // below the camera too. Cancel is in the other corner.
        .ignoresSafeArea(.container, edges: cameraCorner > 0 ? .horizontal : [])
        .preferredColorScheme(.dark)
        #if os(macOS)
        // The keypad sizes itself from the room it has, so the sheet can't take its size from the keypad.
        .frame(minWidth: 380, minHeight: 600)
        #endif
    }
}

private extension PasscodeFlow.Purpose {
    var title: String {
        switch self {
        case .create: "Set passcode"
        case .change: "Change passcode"
        case .remove: "Remove passcode"
        }
    }
}
