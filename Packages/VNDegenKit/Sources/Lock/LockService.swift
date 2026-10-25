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
