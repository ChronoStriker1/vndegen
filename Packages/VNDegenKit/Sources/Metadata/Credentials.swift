import Foundation
import Security

/// The user's API keys. Sources without keys are skipped.
enum Credential: String, CaseIterable {
    case steamGridDBKey
    case igdbClientID
    case igdbClientSecret
}

protocol CredentialStore {
    func read(_ credential: Credential) -> String?
    /// Stores the value, or deletes it when `value` is nil.
    func write(_ credential: Credential, _ value: String?) throws
}

struct KeychainError: LocalizedError {
    var status: OSStatus
    var errorDescription: String? {
        "The Keychain refused the change: \(SecCopyErrorMessageString(status, nil) as String? ?? "status \(status)")."
    }
}

/// Generic passwords under one service name, readable after first unlock so background refreshes can use them.
struct KeychainCredentials: CredentialStore {
    private let service = "com.chronostriker1.vndegen.metadata"

    func read(_ credential: Credential) -> String? {
        var query = baseQuery(credential)
        query[kSecReturnData] = true
        query[kSecMatchLimit] = kSecMatchLimitOne
        var result: CFTypeRef?
        guard SecItemCopyMatching(query as CFDictionary, &result) == errSecSuccess, let data = result as? Data else { return nil }
        return String(data: data, encoding: .utf8)
    }

    func write(_ credential: Credential, _ value: String?) throws {
        let deleted = SecItemDelete(baseQuery(credential) as CFDictionary)
        guard deleted == errSecSuccess || deleted == errSecItemNotFound else { throw KeychainError(status: deleted) }
        guard let value else { return }
        var item = baseQuery(credential)
        item[kSecValueData] = Data(value.utf8)
        item[kSecAttrAccessible] = kSecAttrAccessibleAfterFirstUnlock
        let added = SecItemAdd(item as CFDictionary, nil)
        guard added == errSecSuccess else { throw KeychainError(status: added) }
    }

    private func baseQuery(_ credential: Credential) -> [CFString: Any] {
        [kSecClass: kSecClassGenericPassword, kSecAttrService: service, kSecAttrAccount: credential.rawValue]
    }
}
