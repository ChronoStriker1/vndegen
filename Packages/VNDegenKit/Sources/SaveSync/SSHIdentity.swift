import CryptoKit
import Foundation
import Rsync
import Security
#if canImport(UIKit)
import UIKit
#endif

/// VNDegen's SSH key: an Ed25519 key made on first use. It never leaves the device. iOS keeps it in the Keychain. macOS
/// keeps it in a file only this user can read, as `~/.ssh` does: the login Keychain ties an item to the app's exact code
/// signature, so every update of the ad-hoc signed app would ask for the user's password before a background backup.
/// Moving macOS back to the Keychain needs a Developer ID team, which the data protection Keychain requires.
enum SSHIdentity {
    static func key(in directory: URL) throws -> Curve25519.Signing.PrivateKey {
        #if os(macOS)
        try fileKey(at: directory.appendingPathComponent("ssh-ed25519"))
        #else
        try keychainKey()
        #endif
    }

    /// The line to add to the server's `authorized_keys`, with a comment naming this device.
    @MainActor static func authorizedKeysLine(in directory: URL) throws -> String {
        SSHKeys.authorizedKeysLine(for: try key(in: directory), comment: "VNDegen on \(deviceName)")
    }

    #if os(macOS)
    private static func fileKey(at url: URL) throws -> Curve25519.Signing.PrivateKey {
        if let data = try? Data(contentsOf: url) { return try Curve25519.Signing.PrivateKey(rawRepresentation: data) }
        guard !FileManager.default.fileExists(atPath: url.path) else { throw SaveSyncError.unreadable(url.path) }
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        let key = Curve25519.Signing.PrivateKey()
        // Written under a temporary name created with mode 0600, so the key is never readable by others, and renamed into
        // place only once complete.
        let temporary = url.deletingLastPathComponent().appendingPathComponent(".ssh-ed25519-\(UUID().uuidString)")
        let fd = open(temporary.path, O_WRONLY | O_CREAT | O_EXCL, 0o600)
        guard fd >= 0 else { throw SaveSyncError.unreadable(temporary.path) }
        let data = key.rawRepresentation
        let written = data.withUnsafeBytes { write(fd, $0.baseAddress, $0.count) }
        let synced = fsync(fd) == 0
        close(fd)
        guard written == data.count, synced, rename(temporary.path, url.path) == 0 else {
            unlink(temporary.path)
            throw SaveSyncError.unreadable(url.path)
        }
        return key
    }
    #else
    private static var keychainQuery: [CFString: Any] {
        [
            kSecClass: kSecClassGenericPassword,
            kSecAttrService: "com.chronostriker1.vndegen.savesync",
            kSecAttrAccount: "ssh-ed25519",
        ]
    }

    private static func keychainKey() throws -> Curve25519.Signing.PrivateKey {
        var lookup = keychainQuery
        lookup[kSecReturnData] = true
        lookup[kSecMatchLimit] = kSecMatchLimitOne
        var result: CFTypeRef?
        let status = SecItemCopyMatching(lookup as CFDictionary, &result)
        if status == errSecSuccess, let data = result as? Data { return try Curve25519.Signing.PrivateKey(rawRepresentation: data) }
        guard status == errSecItemNotFound else { throw SaveSyncError.keychain(status) }
        let key = Curve25519.Signing.PrivateKey()
        var item = keychainQuery
        item[kSecValueData] = key.rawRepresentation
        // Readable after the first unlock, so a backup can run while the screen is locked, and never copied to another device.
        item[kSecAttrAccessible] = kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly
        let added = SecItemAdd(item as CFDictionary, nil)
        guard added == errSecSuccess else { throw SaveSyncError.keychain(added) }
        return key
    }
    #endif

    @MainActor private static var deviceName: String {
        #if canImport(UIKit)
        UIDevice.current.name.replacingOccurrences(of: " ", with: "-")
        #else
        (Host.current().localizedName ?? "Mac").replacingOccurrences(of: " ", with: "-")
        #endif
    }
}
