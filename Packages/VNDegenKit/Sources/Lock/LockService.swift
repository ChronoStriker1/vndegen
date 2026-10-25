import CommonCrypto
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
    /// The biometry this device can use right now. `.none` when it has none, none enrolled, or it is locked out.
    public private(set) var biometry: Biometry = .none
    /// The user's choice to offer biometrics. Remembered across launches.
    public var biometricsEnabled: Bool {
        didSet { defaults.set(biometricsEnabled, forKey: Self.biometricsKey) }
    }
    /// True when nothing could unlock the library yet, so the user must set a passcode before locking games.
    public var needsPasscodeSetup: Bool { !hasPasscode && !canUseBiometrics }
    public var canUseBiometrics: Bool { biometricsEnabled && biometry != .none }

    /// The keypad state UnlockView shows. It lives here because the hub calls `UnlockView.handle(_:)` on a view value it may
    /// have recreated, and every copy must reach the same keypad.
    let unlockEntry = PasscodeEntry()

    private static let biometricsKey = "lock.biometricsEnabled"
    private let defaults: UserDefaults
    private let keychain: KeychainItem
    private var record: PasscodeRecord?
    private var authenticating = false

    public init(keychainService: String = "com.chronostriker1.vndegen.lock", defaults: UserDefaults = .standard) {
        self.defaults = defaults
        self.keychain = KeychainItem(service: keychainService, account: "passcode")
        biometricsEnabled = defaults.object(forKey: Self.biometricsKey) as? Bool ?? true
        record = keychain.read().flatMap { try? JSONDecoder().decode(PasscodeRecord.self, from: $0) }
        hasPasscode = record != nil
        refreshBiometry()
    }

    /// Digits in the current passcode, so the keypad can submit as soon as the last one is typed.
    public var passcodeLength: Int? { record?.length }

    /// Re-reads what biometry the device offers. The views call it when they appear, since enrollment can change in Settings.
    public func refreshBiometry() {
        let context = LAContext()
        guard context.canEvaluatePolicy(.deviceOwnerAuthenticationWithBiometrics, error: nil) else {
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

    /// Biometrics first. With an app passcode set, the prompt's fallback button returns here so the user types it on the keypad.
    /// Without one, the system falls back to the device passcode. Returns whether the library is now unlocked.
    public func unlockWithBiometrics() async -> Bool {
        guard canUseBiometrics, !authenticating else { return isUnlocked }
        authenticating = true
        defer { authenticating = false }
        let context = LAContext()
        context.localizedFallbackTitle = hasPasscode ? "Enter Passcode" : nil
        let policy: LAPolicy = hasPasscode ? .deviceOwnerAuthenticationWithBiometrics : .deviceOwnerAuthentication
        let ok = (try? await context.evaluatePolicy(policy, localizedReason: "Show your hidden games.")) ?? false
        if ok { isUnlocked = true }
        return ok
    }

    public func unlock(passcode: String) -> Bool {
        guard let record, record.matches(passcode) else { return false }
        isUnlocked = true
        return true
    }

    /// Sets, replaces, or with nil removes the passcode. It does not check the current one, so callers that change or remove
    /// a passcode ask for it first, as LockSettingsView does. Returns false if the Keychain refused the write.
    @discardableResult
    public func setPasscode(_ passcode: String?) -> Bool {
        guard let passcode else {
            keychain.delete()
            record = nil
            hasPasscode = false
            return true
        }
        let new = PasscodeRecord(passcode: passcode)
        guard let data = try? JSONEncoder().encode(new), keychain.write(data) else { return false }
        record = new
        hasPasscode = true
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

    func read() -> Data? {
        var query = query
        query[kSecReturnData as String] = true
        query[kSecMatchLimit as String] = kSecMatchLimitOne
        var result: CFTypeRef?
        guard SecItemCopyMatching(query as CFDictionary, &result) == errSecSuccess else { return nil }
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
