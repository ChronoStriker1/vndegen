import SwiftUI

/// Passcode and biometrics options, shown inside the app's Settings screen.
public struct LockSettingsView: View {
    let lock: LockService
    @State private var flow: PasscodeFlow.Purpose?

    public init(lock: LockService) { self.lock = lock }

    public var body: some View {
        Form {
            Section {
                if lock.hasPasscode {
                    Button("Change Passcode") { flow = .change }
                    Button("Remove Passcode", role: .destructive) { flow = .remove }
                } else {
                    Button("Set Passcode") {
                        Task { if await lock.authorizeCredentialChange() { flow = .create } }
                    }
                }
            } header: {
                Text("Hidden games")
            } footer: {
                Text(lock.needsPasscodeSetup
                     ? "Set a passcode before you lock games."
                     : "Locked games stay hidden until you unlock them with \(unlockMethods).")
            }
            Section {
                Toggle(isOn: Binding(get: { lock.biometricsEnabled }, set: { enabled in
                    Task { await lock.setBiometricsEnabled(enabled) }
                })) {
                    Label("Use \(lock.biometry.name)", systemImage: lock.biometry.symbol)
                }
                .disabled(lock.biometry == .none)
            } footer: {
                if lock.biometry == .none {
                    Text("This device has no Face ID or Touch ID set up.")
                }
            }
        }
        .onAppear { lock.refreshBiometry() }
        .sheet(item: $flow) { purpose in
            PasscodeFlowSheet(lock: lock, purpose: purpose) { flow = nil }
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
    let lock: LockService
    let onClose: () -> Void
    @State private var entry: PasscodeEntry

    init(lock: LockService, purpose: PasscodeFlow.Purpose, onClose: @escaping () -> Void) {
        self.lock = lock
        self.onClose = onClose
        _entry = State(initialValue: PasscodeEntry(flow: PasscodeFlow(purpose)))
    }

    var body: some View {
        NavigationStack {
            ZStack {
                LockBackdrop()
                if let flow = entry.flow {
                    PasscodeKeypad(entry: entry, title: flow.title, expectedLength: flow.expectedLength(current: lock.passcodeLength),
                                   keySize: 72, onSubmit: submit)
                }
            }
            .navigationTitle(entry.flow?.purpose.title ?? "")
            #if os(iOS)
            .navigationBarTitleDisplayMode(.inline)
            #endif
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("Cancel", action: onClose) }
            }
        }
        .preferredColorScheme(.dark)
    }

    private func submit(_ code: String) {
        guard var flow = entry.flow else { return }
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
                onClose()
            } else {
                entry.flow = PasscodeFlow(flow.purpose)
                entry.reject("The passcode could not be saved. Try again.")
            }
        }
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
