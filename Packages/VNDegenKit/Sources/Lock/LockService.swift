import Foundation
import Observation

/// Gates locked games behind Face ID, Touch ID, or an app passcode.
@MainActor
@Observable
public final class LockService {
    public private(set) var isUnlocked = false
    public private(set) var hasPasscode = false

    public init() {}

    /// Biometrics first, falling back to the device passcode. Returns whether the library is now unlocked.
    public func unlockWithBiometrics() async -> Bool { false }
    public func unlock(passcode: String) -> Bool { false }
    public func setPasscode(_ passcode: String?) {}
    public func lock() { isUnlocked = false }
}

import SwiftUI
import VNCore

/// Asks for Face ID, Touch ID, or the passcode. Calls `onDone` with whether the library unlocked.
public struct UnlockView: View {
    let lock: LockService
    let onDone: (Bool) -> Void
    public init(lock: LockService, onDone: @escaping (Bool) -> Void) {
        self.lock = lock
        self.onDone = onDone
    }
    public var body: some View { Text("Unlock") }

    /// Gamepad and keyboard input while the view is shown.
    public func handle(_ event: ControllerEvent) {}
}

/// Passcode and biometrics options, shown inside the app's Settings screen.
public struct LockSettingsView: View {
    let lock: LockService
    public init(lock: LockService) { self.lock = lock }
    public var body: some View { Text("Hidden games") }
}
