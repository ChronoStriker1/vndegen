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
                    Button("Change Passcode") { model.select(.changePasscode) }
                        .focusRow(model.isFocused(.changePasscode), id: LockSettingsModel.Target.changePasscode)
                    Button("Remove Passcode", role: .destructive) { model.select(.removePasscode) }
                        .focusRow(model.isFocused(.removePasscode), id: LockSettingsModel.Target.removePasscode)
                } else {
                    Button("Set Passcode") { model.select(.setPasscode) }
                        .focusRow(model.isFocused(.setPasscode), id: LockSettingsModel.Target.setPasscode)
                }
            } header: {
                Text("Hidden games")
            } footer: {
                Text(lock.needsPasscodeSetup
                     ? "Set a passcode before you lock games."
                     : "Locked games stay hidden until you unlock them with \(unlockMethods).")
            }
            Section {
                Toggle(isOn: Binding(get: { lock.biometricsEnabled }, set: { _ in model.select(.biometrics) })) {
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

    var body: some View {
        NavigationStack {
            ZStack {
                LockBackdrop()
                if let flow = entry.flow {
                    PasscodeKeypad(entry: entry, title: flow.title, expectedLength: flow.expectedLength(current: model.lock.passcodeLength),
                                   keySize: 72, onSubmit: model.submit)
                }
            }
            .navigationTitle(entry.flow?.purpose.title ?? "")
            #if os(iOS)
            .navigationBarTitleDisplayMode(.inline)
            #endif
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("Cancel", action: model.closeFlow) }
            }
        }
        .preferredColorScheme(.dark)
    }
}

private extension PasscodeFlow.Purpose {
    var title: String {
        switch self {
        case .create: "Set Passcode"
        case .change: "Change Passcode"
        case .remove: "Remove Passcode"
        }
    }
}
