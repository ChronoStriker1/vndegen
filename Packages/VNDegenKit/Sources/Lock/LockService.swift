import CommonCrypto
import ControllerUI
import Foundation
import LocalAuthentication
import Observation
import Security

public enum Biometry: Sendable {
    case none, touchID, faceID, opticID

    public var name: String {
        switch self {
        case .none: "Face ID or Touch ID"
        case .touchID: "Touch ID"
        case .faceID: "Face ID"
        case .opticID: "Optic ID"
        }
    }

    public var symbol: String {
        switch self {
        case .none: "lock.fill"
        case .touchID: "touchid"
        case .faceID: "faceid"
        case .opticID: "opticid"
        }
    }
}

/// Gates locked games behind Face ID, Touch ID, or an app passcode.
@MainActor
@Observable
public final class LockService {
    public private(set) var isUnlocked = false
    public private(set) var hasPasscode = false
    /// The biometry this device can use right now. `.none` when it has none or none enrolled.
    public private(set) var biometry: Biometry = .none
    /// The user's choice to offer biometrics. Remembered across launches. Change it with `setBiometricsEnabled(_:)`.
    public private(set) var biometricsEnabled: Bool {
        didSet { defaults.set(biometricsEnabled, forKey: Self.biometricsKey) }
    }
    /// True when nothing could unlock the library yet, so the user must set a passcode before locking games.
    public var needsPasscodeSetup: Bool { !hasPasscode && !canUseBiometrics && !isGuarded }
    public var canUseBiometrics: Bool { biometricsEnabled && biometry != .none }
    public var canUseDevicePasscode: Bool { !hasPasscode && !canUseBiometrics && isGuarded }
    public private(set) var retryAfter: Date?

    /// The keypad state UnlockView shows. It lives here because the hub calls `UnlockView.handle(_:)` on a view value it may
    /// have recreated, and every copy must reach the same keypad.
    let unlockEntry = PasscodeEntry()

    private static let biometricsKey = "lock.biometricsEnabled"
    private static let guardedKey = "lock.guarded"
    private static let failuresKey = "lock.failures"
    private static let retryAfterKey = "lock.retryAfter"
    private let defaults: UserDefaults
    private let keychain: KeychainItem
    private var record: PasscodeRecord?
    private var passcodeUnreadable = false
    private var authenticating = false
    private var isGuarded: Bool { passcodeUnreadable || defaults.bool(forKey: Self.guardedKey) }

    public init(keychainService: String = KeychainService.named("com.chronostriker1.vndegen.lock"), defaults: UserDefaults = .standard) {
        self.defaults = defaults
        self.keychain = KeychainItem(service: keychainService, account: "passcode")
        biometricsEnabled = defaults.object(forKey: Self.biometricsKey) as? Bool ?? true
        retryAfter = defaults.object(forKey: Self.retryAfterKey) as? Date
        readPasscode()
        refreshBiometry()
        if hasPasscode || canUseBiometrics { defaults.set(true, forKey: Self.guardedKey) }
    }

    private func readPasscode() {
        do {
            record = try keychain.read().flatMap { try? JSONDecoder().decode(PasscodeRecord.self, from: $0) }
            passcodeUnreadable = false
        } catch {
            record = nil
            passcodeUnreadable = true
        }
        hasPasscode = record != nil
    }

    private func setGuarded(_ guarded: Bool) {
        defaults.set(guarded, forKey: Self.guardedKey)
    }

    /// Digits in the current passcode, so the keypad can submit as soon as the last one is typed.
    public var passcodeLength: Int? { record?.length }

    /// Re-reads what biometry the device offers. The views call it when they appear, since enrollment can change in Settings.
    public func refreshBiometry() {
        if passcodeUnreadable { readPasscode() }
        let context = LAContext()
        var error: NSError?
        let available = context.canEvaluatePolicy(.deviceOwnerAuthenticationWithBiometrics, error: &error)
        // A locked-out sensor still guards the library, and the device passcode stands in for it. Counting it as absent
        // would let anyone who fails Face ID a few times set a passcode of their own.
        guard available || error?.code == LAError.biometryLockout.rawValue else {
            biometry = .none
            return
        }
        biometry = switch context.biometryType {
        case .faceID: .faceID
        case .touchID: .touchID
        case .opticID: .opticID
        default: .none
        }
    }

    /// Biometrics first, falling back as `authenticate(reason:)` describes. Returns whether the library is now unlocked.
    public func unlockWithBiometrics() async -> Bool {
        guard canUseBiometrics, !authenticating else { return isUnlocked }
        let ok = await authenticate(reason: "Show your hidden games.")
        if ok {
            isUnlocked = true
            setGuarded(true)
        }
        return ok
    }

    /// The device passcode, or the Mac's password, for hidden games guarded by biometrics that can't be used right now.
    public func unlockWithDevicePasscode() async -> Bool {
        guard canUseDevicePasscode, !authenticating else { return isUnlocked }
        let ok = await authenticate(reason: "Show your hidden games.")
        if ok { isUnlocked = true }
        return ok
    }

    /// True when the user may add a passcode or turn biometrics off. When biometrics alone guard hidden games, either
    /// change would let whoever holds the device in, so it asks for biometrics first, the same check that unlocks.
    /// With a passcode set, the passcode flows ask for the current one instead.
    public func authorizeCredentialChange() async -> Bool {
        guard !hasPasscode, canUseBiometrics || isGuarded else { return true }
        guard !authenticating else { return false }
        return await authenticate(reason: "Change how your hidden games unlock.")
    }

    /// Turns biometrics on, or off once `authorizeCredentialChange()` allows it. Returns the setting now in effect.
    @discardableResult
    public func setBiometricsEnabled(_ enabled: Bool) async -> Bool {
        if enabled {
            biometricsEnabled = true
            if canUseBiometrics { setGuarded(true) }
        } else if await authorizeCredentialChange() {
            biometricsEnabled = false
            setGuarded(hasPasscode)
        }
        return biometricsEnabled
    }

    /// With an app passcode set, the prompt's fallback button returns to the caller so the user types it on the keypad.
    /// Without one, the system falls back to the device passcode.
    private func authenticate(reason: String) async -> Bool {
        authenticating = true
        defer { authenticating = false }
        let context = LAContext()
        context.localizedFallbackTitle = hasPasscode ? "Enter Passcode" : nil
        let policy: LAPolicy = hasPasscode ? .deviceOwnerAuthenticationWithBiometrics : .deviceOwnerAuthentication
        return (try? await context.evaluatePolicy(policy, localizedReason: reason)) ?? false
    }

    public func unlock(passcode: String) -> Bool {
        guard let record, !isWaiting else { return false }
        guard record.matches(passcode) else {
            let failures = defaults.integer(forKey: Self.failuresKey) + 1
            defaults.set(failures, forKey: Self.failuresKey)
            if let delay = Self.delay(afterFailures: failures) {
                retryAfter = .now.addingTimeInterval(delay)
                defaults.set(retryAfter, forKey: Self.retryAfterKey)
            }
            return false
        }
        defaults.removeObject(forKey: Self.failuresKey)
        defaults.removeObject(forKey: Self.retryAfterKey)
        retryAfter = nil
        isUnlocked = true
        return true
    }

    public var isWaiting: Bool { retryAfter.map { $0 > .now } ?? false }

    public func rejection(_ message: String) -> String {
        guard let wait = retryAfter?.timeIntervalSinceNow, wait > 0 else { return message }
        let time = Duration.seconds(wait.rounded(.up)).formatted(.units(allowed: [.minutes, .seconds], width: .wide))
        return "Too many tries. Try again in \(time)."
    }

    static func delay(afterFailures failures: Int) -> TimeInterval? {
        guard failures >= 5 else { return nil }
        return TimeInterval(min(30 << min(failures - 5, 5), 15 * 60))
    }

    /// Sets, replaces, or with nil removes the passcode. It does not check the current one, so callers that change or remove
    /// a passcode ask for it first, as LockSettingsView does. Returns false if the Keychain refused the write.
    @discardableResult
    public func setPasscode(_ passcode: String?) -> Bool {
        guard let passcode else {
            keychain.delete()
            record = nil
            hasPasscode = false
            passcodeUnreadable = false
            setGuarded(canUseBiometrics)
            return true
        }
        let new = PasscodeRecord(passcode: passcode)
        guard let data = try? JSONEncoder().encode(new), keychain.write(data) else { return false }
        record = new
        hasPasscode = true
        passcodeUnreadable = false
        setGuarded(true)
        return true
    }

    public func lock() {
        isUnlocked = false
        unlockEntry.reset()
    }
}

/// A passcode as a salted PBKDF2-SHA256 hash. The iteration count is stored so it can grow without breaking old records.
nonisolated struct PasscodeRecord: Codable, Equatable {
    static let defaultIterations = 200_000

    var salt: Data
    var hash: Data
    var iterations: Int
    var length: Int

    init(passcode: String, iterations: Int = Self.defaultIterations) {
        var salt = Data(count: 16)
        let status = salt.withUnsafeMutableBytes { SecRandomCopyBytes(kSecRandomDefault, 16, $0.baseAddress!) }
        precondition(status == errSecSuccess, "no system randomness")
        self.salt = salt
        self.iterations = iterations
        self.length = passcode.count
        self.hash = Self.derive(passcode, salt: salt, iterations: iterations)
    }

    func matches(_ passcode: String) -> Bool {
        let candidate = Self.derive(passcode, salt: salt, iterations: iterations)
        guard candidate.count == hash.count else { return false }
        return zip(candidate, hash).reduce(0) { $0 | ($1.0 ^ $1.1) } == 0
    }

    private static func derive(_ passcode: String, salt: Data, iterations: Int) -> Data {
        let password = Array(passcode.utf8).map { CChar(bitPattern: $0) }
        var key = [UInt8](repeating: 0, count: 32)
        let status = salt.withUnsafeBytes { saltBytes in
            CCKeyDerivationPBKDF(CCPBKDFAlgorithm(kCCPBKDF2), password, password.count,
                                 saltBytes.bindMemory(to: UInt8.self).baseAddress, salt.count,
                                 CCPseudoRandomAlgorithm(kCCPRFHmacAlgSHA256), UInt32(iterations), &key, key.count)
        }
        precondition(status == kCCSuccess, "PBKDF2 failed")
        return Data(key)
    }
}

/// One generic-password Keychain item, readable only while the device is unlocked and never synced or backed up to other devices.
nonisolated struct KeychainItem {
    let service: String
    let account: String

    private var query: [String: Any] {
        [kSecClass as String: kSecClassGenericPassword, kSecAttrService as String: service, kSecAttrAccount as String: account]
    }

    struct ReadError: Error {
        let status: OSStatus
    }

    func read() throws -> Data? {
        var query = query
        query[kSecReturnData as String] = true
        query[kSecMatchLimit as String] = kSecMatchLimitOne
        var result: CFTypeRef?
        let status = SecItemCopyMatching(query as CFDictionary, &result)
        if status == errSecItemNotFound { return nil }
        guard status == errSecSuccess else { throw ReadError(status: status) }
        return result as? Data
    }

    /// Updates in place when the item exists, so a failed write never loses the old passcode.
    func write(_ data: Data) -> Bool {
        let values: [String: Any] = [
            kSecValueData as String: data,
            kSecAttrAccessible as String: kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
        ]
        let status = SecItemUpdate(query as CFDictionary, values as CFDictionary)
        guard status == errSecItemNotFound else { return status == errSecSuccess }
        return SecItemAdd(query.merging(values) { $1 } as CFDictionary, nil) == errSecSuccess
    }

    func delete() {
        SecItemDelete(query as CFDictionary)
    }
}
