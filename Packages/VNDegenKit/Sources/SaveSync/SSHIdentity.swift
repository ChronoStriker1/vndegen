import CryptoKit
import Foundation
import Rsync
import Security
#if canImport(UIKit)
import UIKit
#endif

/// VNDegen's SSH key: an Ed25519 key made on first use and kept in this device's Keychain. It never leaves the device.
enum SSHIdentity {
    private static var query: [CFString: Any] {
        [
            kSecClass: kSecClassGenericPassword,
            kSecAttrService: "com.chronostriker1.vndegen.savesync",
            kSecAttrAccount: "ssh-ed25519",
        ]
    }

    static func key() throws -> Curve25519.Signing.PrivateKey {
        var lookup = query
        lookup[kSecReturnData] = true
        lookup[kSecMatchLimit] = kSecMatchLimitOne
        var result: CFTypeRef?
        let status = SecItemCopyMatching(lookup as CFDictionary, &result)
        if status == errSecSuccess, let data = result as? Data { return try Curve25519.Signing.PrivateKey(rawRepresentation: data) }
        guard status == errSecItemNotFound else { throw SaveSyncError.keychain(status) }
        let key = Curve25519.Signing.PrivateKey()
        var item = query
        item[kSecValueData] = key.rawRepresentation
        // Readable after the first unlock, so a backup can run while the screen is locked, and never copied to another device.
        item[kSecAttrAccessible] = kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly
        let added = SecItemAdd(item as CFDictionary, nil)
        guard added == errSecSuccess else { throw SaveSyncError.keychain(added) }
        return key
    }

    /// The line to add to the server's `authorized_keys`, with a comment naming this device.
    @MainActor static func authorizedKeysLine() throws -> String {
        SSHKeys.authorizedKeysLine(for: try key(), comment: "VNDegen on \(deviceName)")
    }

    @MainActor private static var deviceName: String {
        #if canImport(UIKit)
        UIDevice.current.name.replacingOccurrences(of: " ", with: "-")
        #else
        (Host.current().localizedName ?? "Mac").replacingOccurrences(of: " ", with: "-")
        #endif
    }
}
