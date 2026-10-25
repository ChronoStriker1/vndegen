import SwiftUI
import VNCore

/// Asks for Face ID, Touch ID, or the passcode. Calls `onDone` with whether the library unlocked.
/// With no passcode and no usable biometrics, it first asks the user to create a passcode.
/// While it is shown, keep `InputHub.translatesKeyboard` on: digits come from the keyboard directly, and Return, Backspace,
/// and the arrows arrive through `handle(_:)` as confirm, cancel, and directions.
public struct UnlockView: View {
    let lock: LockService
    let onDone: (Bool) -> Void

    public init(lock: LockService, onDone: @escaping (Bool) -> Void) {
        self.lock = lock
        self.onDone = onDone
    }

    private var entry: PasscodeEntry { lock.unlockEntry }

    public var body: some View {
        GeometryReader { geometry in
            let wide = geometry.size.width > geometry.size.height * 1.15
            // The keypad column is about 60 points of text plus 5.3 key heights.
            let keySize = min(max((geometry.size.height - (wide ? 110 : 300)) / 5.3, 44), 80)
            ZStack {
                LockBackdrop()
                Group {
                    if wide {
                        HStack(spacing: 56) {
                            header(keySize: keySize)
                            keypadOrBiometrics(keySize: keySize)
                        }
                    } else {
                        VStack(spacing: keySize * 0.4) {
                            header(keySize: keySize)
                            keypadOrBiometrics(keySize: keySize)
                        }
                    }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                cancelButton
            }
        }
        .preferredColorScheme(.dark)
        .onAppear {
            lock.refreshBiometry()
            entry.reset()
            entry.flow = lock.needsPasscodeSetup ? PasscodeFlow(.create) : nil
        }
        .task {
            guard entry.flow == nil, lock.canUseBiometrics else { return }
            await tryBiometrics()
        }
    }

    /// Gamepad and keyboard input while the view is shown.
    public func handle(_ event: ControllerEvent) {
        switch entry.handle(event, expectedLength: expectedLength) {
        case .none: break
        case let .submit(code): submit(code)
        case .cancel: finish(false)
        }
    }

    private var expectedLength: Int? {
        entry.flow.map { $0.expectedLength(current: lock.passcodeLength) } ?? lock.passcodeLength
    }

    private func header(keySize: CGFloat) -> some View {
        VStack(spacing: 14) {
            Image(systemName: entry.flow == nil ? "lock.fill" : "key.fill")
                .font(.system(size: keySize * 0.55, weight: .semibold))
                .foregroundStyle(.white.opacity(0.9))
            Text(entry.flow == nil ? "Hidden games are locked" : "Set a passcode first")
                .font(.title2.weight(.bold))
            Text(entry.flow == nil
                 ? "Unlock to show the games you hid."
                 : "Locking needs a passcode because \(lock.biometry == .none ? "this device has no Face ID or Touch ID" : "\(lock.biometry.name) is turned off").")
                .font(.subheadline)
                .foregroundStyle(.white.opacity(0.65))
                .multilineTextAlignment(.center)
                .frame(maxWidth: 320)
        }
        .foregroundStyle(.white)
    }

    @ViewBuilder
    private func keypadOrBiometrics(keySize: CGFloat) -> some View {
        VStack(spacing: keySize * 0.3) {
            if let flow = entry.flow {
                PasscodeKeypad(entry: entry, title: flow.title, expectedLength: expectedLength, keySize: keySize, onSubmit: submit)
            } else if lock.hasPasscode {
                PasscodeKeypad(entry: entry, title: "Enter passcode", expectedLength: expectedLength, keySize: keySize, onSubmit: submit)
            }
            if entry.flow == nil, lock.canUseBiometrics {
                Button {
                    Task { await tryBiometrics() }
                } label: {
                    Label("Use \(lock.biometry.name)", systemImage: lock.biometry.symbol)
                        .font(.headline)
                        .padding(.horizontal, 22)
                        .padding(.vertical, 12)
                        .background(Capsule().fill(.white.opacity(0.14)))
                }
                .buttonStyle(.plain)
                .foregroundStyle(.white)
            }
        }
    }

    private var cancelButton: some View {
        Button("Cancel") { finish(false) }
            .font(.body.weight(.medium))
            .foregroundStyle(.white.opacity(0.8))
            .padding(20)
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
    }

    private func tryBiometrics() async {
        if await lock.unlockWithBiometrics() { finish(true) }
    }

    private func submit(_ code: String) {
        guard var flow = entry.flow else {
            if lock.unlock(passcode: code) { finish(true) } else { entry.reject("Wrong passcode") }
            return
        }
        switch flow.submit(code, isCurrent: lock.unlock(passcode:)) {
        case .next:
            entry.flow = flow
            entry.message = nil
        case let .retry(message):
            entry.flow = flow
            entry.reject(message)
        case let .finished(passcode):
            if let passcode, lock.setPasscode(passcode), lock.unlock(passcode: passcode) {
                finish(true)
            } else {
                entry.reject("The passcode could not be saved. Try again.")
                entry.flow = PasscodeFlow(.create)
            }
        }
    }

    private func finish(_ unlocked: Bool) {
        entry.reset()
        entry.flow = nil
        onDone(unlocked)
    }
}
