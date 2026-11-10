import ControllerUI
import CryptoKit
import Foundation
import Rclone
import Security

/// Where the user's Ludusavi store lives, as Ludusavi's cloud settings name it: a remote and the path on it.
public struct SaveServer: Codable, Equatable, Sendable {
    public var remote: CloudRemote
    /// Ludusavi's cloud path. For SMB the share, then the folder inside it, such as `games/Ludusavi`.
    public var path: String

    public init(remote: CloudRemote, path: String) {
        self.remote = remote
        self.path = path
    }
}

public enum CloudRemote: Codable, Equatable, Sendable {
    case smb(SMBRemote)
    case ftp(FTPRemote)
    case webdav(WebDAVRemote)

    public enum Kind: String, CaseIterable, Sendable {
        case smb, ftp, webdav

        public var title: String {
            switch self {
            case .smb: "SMB"
            case .ftp: "FTP"
            case .webdav: "WebDAV"
            }
        }
    }

    public var kind: Kind {
        switch self {
        case .smb: .smb
        case .ftp: .ftp
        case .webdav: .webdav
        }
    }

    /// The machine the store is on, for labels.
    public var host: String {
        switch self {
        case let .smb(smb): smb.host
        case let .ftp(ftp): ftp.host
        case let .webdav(webdav): URL(string: webdav.url)?.host() ?? webdav.url
        }
    }
}

/// Ludusavi's SMB remote: what its cloud settings ask for, without the password, which VNDegen keeps apart.
public struct SMBRemote: Codable, Equatable, Sendable {
    public var host: String
    public var port: Int
    /// `guest` for a guest login, as Ludusavi's remote stores it.
    public var username: String

    public static let guest = "guest"
    public static let defaultPort = 445

    public init(host: String, port: Int = SMBRemote.defaultPort, username: String = SMBRemote.guest) {
        self.host = host
        self.port = port
        self.username = username
    }

    public var isGuest: Bool { username == Self.guest }
}

public struct FTPRemote: Codable, Equatable, Sendable {
    public var host: String
    public var port: Int
    public var username: String

    public static let defaultPort = 21

    public init(host: String, port: Int = FTPRemote.defaultPort, username: String) {
        self.host = host
        self.port = port
        self.username = username
    }
}

public struct WebDAVRemote: Codable, Equatable, Sendable {
    public var url: String
    public var username: String
    public var vendor: Vendor

    public enum Vendor: String, Codable, CaseIterable, Sendable {
        case other, nextcloud, owncloud, sharepoint
        case sharepointNTLM = "sharepoint-ntlm"

        public var title: String {
            switch self {
            case .other: "Other"
            case .nextcloud: "Nextcloud"
            case .owncloud: "Owncloud"
            case .sharepoint: "Sharepoint"
            case .sharepointNTLM: "Sharepoint (NTLM)"
            }
        }
    }

    public init(url: String, username: String, vendor: Vendor = .other) {
        self.url = url
        self.username = username
        self.vendor = vendor
    }
}

/// The store as rclone names it, and the way to call rclone.
public struct RemoteAccess: Sendable {
    /// rclone's name for the store folder: `<remote name>:<path>`, or in tests a local folder path (rclone's local backend).
    public var root: String
    public var call: @Sendable (_ method: String, _ input: Data) async throws -> Data

    public init(root: String, call: @escaping @Sendable (_ method: String, _ input: Data) async throws -> Data = { try await Rclone.call($0, json: $1) }) {
        self.root = root
        self.call = call
    }

    public static func connect(_ server: SaveServer, password: String?, name: String? = nil) async throws -> RemoteAccess {
        switch server.remote {
        case .smb:
            return try await smb(server, password: password, name: name)
        case let .ftp(ftp):
            let parameters = ["host": ftp.host, "port": String(ftp.port), "user": ftp.username, "force_list_hidden": "true"]
            return try await create(server, type: "ftp", identity: ["ftp", ftp.host, String(ftp.port), ftp.username], password: password,
                                    name: name, parameters: parameters)
        case let .webdav(webdav):
            var parameters = ["url": webdav.url, "vendor": webdav.vendor.rawValue]
            if !webdav.username.isEmpty { parameters["user"] = webdav.username }
            return try await create(server, type: "webdav", identity: ["webdav", webdav.url, webdav.vendor.rawValue, webdav.username],
                                    password: password, name: name, parameters: parameters)
        }
    }

    /// Adds `server` to rclone's in-memory config as an smb remote, as Ludusavi's `rclone config create` does, and
    /// returns its store folder. The remote's name comes from the host, port, username, and password, so a change to any
    /// of them never reuses a remote rclone already has. `name` replaces it, so a test can act as a second device.
    public static func smb(_ server: SaveServer, password: String?, name: String? = nil) async throws -> RemoteAccess {
        guard case let .smb(smb) = server.remote else { throw SaveSyncError.notSetUp }
        let password = smb.isGuest ? nil : password
        let identity = [smb.host, String(smb.port), smb.username, password ?? ""].joined(separator: "\n")
        let name = name ?? "vndegen-" + SHA256.hash(data: Data(identity.utf8)).prefix(4).map { String(format: "%02x", $0) }.joined()
        var parameters = ["host": smb.host, "port": String(smb.port), "user": smb.username]
        if let password, !password.isEmpty { parameters["pass"] = password }
        try await Rclone.call("config/create", RemoteConfig(name: name, type: "smb", parameters: parameters))
        let path = server.path.trimmingCharacters(in: CharacterSet(charactersIn: "/").union(.whitespaces))
        return RemoteAccess(root: "\(name):\(path)")
    }

    private static func create(_ server: SaveServer, type: String, identity: [String], password: String?, name: String?,
                               parameters: [String: String]) async throws -> RemoteAccess {
        let name = name ?? "vndegen-" + SHA256.hash(data: Data((identity + [password ?? ""]).joined(separator: "\n").utf8)).prefix(4)
            .map { String(format: "%02x", $0) }.joined()
        var parameters = parameters
        if let password, !password.isEmpty { parameters["pass"] = password }
        try await Rclone.call("config/create", RemoteConfig(name: name, type: type, parameters: parameters))
        var path = server.path.replacingOccurrences(of: "\\", with: "/").trimmingCharacters(in: .whitespaces)
        while path.count > 1, path.hasSuffix("/") { path.removeLast() }
        return RemoteAccess(root: "\(name):\(path)")
    }

    private struct RemoteConfig: Encodable {
        struct Options: Encodable {
            /// rclone obscures the password itself, as `rclone config create` does by default.
            var obscure = true
            var nonInteractive = true
        }

        var name: String
        var type: String
        var parameters: [String: String]
        var opt = Options()
    }
}

/// iPhone and iPad keep it in the Keychain, on this device only. The Mac keeps it in a file only this
/// user can read: the login Keychain ties an item to the app's exact code signature, so every update of the ad-hoc
/// signed app would ask for the user's password before a background backup.
enum ServerPassword {
    static func read(_ kind: CloudRemote.Kind, in directory: URL) throws -> String? {
        let fileName = fileName(kind)
        #if os(macOS)
        do {
            return String(decoding: try Data(contentsOf: directory.appendingPathComponent(fileName)), as: UTF8.self)
        } catch CocoaError.fileReadNoSuchFile {
            return nil
        } catch {
            throw SaveSyncError.unreadable(directory.appendingPathComponent(fileName).path)
        }
        #else
        return try Keychain.read(account: fileName).map { String(decoding: $0, as: UTF8.self) }
        #endif
    }

    /// Keeps `password`, or deletes the kept one when nil.
    static func save(_ password: String?, for kind: CloudRemote.Kind, in directory: URL) throws {
        let fileName = fileName(kind)
        #if os(macOS)
        let url = directory.appendingPathComponent(fileName)
        guard let password else {
            guard unlink(url.path) == 0 || errno == ENOENT else { throw SaveSyncError.unreadable(url.path) }
            return
        }
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        // Written under a temporary name created with mode 0600, so the password is never readable by others, then
        // renamed over the old file only once complete.
        let temporary = directory.appendingPathComponent(".\(fileName)-\(UUID().uuidString)")
        let fd = open(temporary.path, O_WRONLY | O_CREAT | O_EXCL, 0o600)
        guard fd >= 0 else { throw SaveSyncError.unreadable(temporary.path) }
        defer { unlink(temporary.path) }
        let data = Data(password.utf8)
        let written = data.withUnsafeBytes { write(fd, $0.baseAddress, $0.count) }
        let synced = fsync(fd) == 0
        close(fd)
        guard written == data.count, synced, rename(temporary.path, url.path) == 0 else { throw SaveSyncError.unreadable(url.path) }
        #else
        try Keychain.delete(account: fileName)
        if let password { try Keychain.add(Data(password.utf8), account: fileName) }
        #endif
    }

    private static func fileName(_ kind: CloudRemote.Kind) -> String { "\(kind.rawValue)-password" }
}

/// VNDegen 0.4.0 reached the store over SSH with a key of its own, which nothing uses any more.
enum OldSSHKey {
    static func delete(in directory: URL) {
        #if os(macOS)
        unlink(directory.appendingPathComponent("ssh-ed25519").path)
        #else
        try? Keychain.delete(account: "ssh-ed25519")
        #endif
    }
}

#if !os(macOS)
/// Save sync's generic passwords in the Keychain.
private enum Keychain {
    static func query(_ account: String) -> [CFString: Any] {
        [kSecClass: kSecClassGenericPassword, kSecAttrService: KeychainService.named("com.chronostriker1.vndegen.savesync"), kSecAttrAccount: account]
    }

    static func read(account: String) throws -> Data? {
        var lookup = query(account)
        lookup[kSecReturnData] = true
        lookup[kSecMatchLimit] = kSecMatchLimitOne
        var result: CFTypeRef?
        let status = SecItemCopyMatching(lookup as CFDictionary, &result)
        if status == errSecItemNotFound { return nil }
        guard status == errSecSuccess, let data = result as? Data else { throw SaveSyncError.keychain(status) }
        return data
    }

    static func add(_ data: Data, account: String) throws {
        var item = query(account)
        item[kSecValueData] = data
        // Readable after the first unlock, so a backup can run while the screen is locked, and never copied to another device.
        item[kSecAttrAccessible] = kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly
        let status = SecItemAdd(item as CFDictionary, nil)
        guard status == errSecSuccess else { throw SaveSyncError.keychain(status) }
    }

    static func delete(account: String) throws {
        let status = SecItemDelete(query(account) as CFDictionary)
        guard status == errSecSuccess || status == errSecItemNotFound else { throw SaveSyncError.keychain(status) }
    }
}
#endif
