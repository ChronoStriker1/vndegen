import Foundation
import Rclone
import Testing
@testable import SaveSync

/// The SMB share the SMB tests use, from `VNDEGEN_SMB_TEST` as `host:port`. Nil leaves those tests off.
let smbTest: (host: String, port: Int)? = {
    guard let value = ProcessInfo.processInfo.environment["VNDEGEN_SMB_TEST"], let colon = value.lastIndex(of: ":"),
          let port = Int(value[value.index(after: colon)...]) else { return nil }
    return (String(value[..<colon]), port)
}()

/// The password of the user `player` on the test share.
let smbPassword = ProcessInfo.processInfo.environment["VNDEGEN_SMB_TEST_PASSWORD"]

/// Where a test's store lives: a local folder through rclone's local backend, or a folder on the test share.
enum StoreKind: String, CustomTestStringConvertible, Sendable {
    case local, smb

    /// Every kind the machine running the tests can reach.
    static var all: [StoreKind] { smbTest == nil ? [.local] : [.local, .smb] }

    var testDescription: String { rawValue }
}

/// A test's store folder, reached the way another device reaches it: through rclone, with a remote of its own.
struct TestServer: Sendable {
    let kind: StoreKind
    /// rclone's name for the store folder.
    let root: String
    /// The store folder itself, for a local store.
    let folder: URL?
    /// The path VNDegen's settings take: for SMB the share, then the folder inside it.
    let path: String

    /// A store folder of the test's own: a new local folder, or `games/vndegen-tests/<UUID>` on the test share.
    static func make(_ kind: StoreKind) async throws -> TestServer {
        switch kind {
        case .local:
            let folder = try temporaryDirectory().appendingPathComponent("server/Ludusavi", isDirectory: true)
            try fm.createDirectory(at: folder, withIntermediateDirectories: true)
            return TestServer(kind: kind, root: folder.path, folder: folder, path: "games/Ludusavi")
        case .smb:
            let share = try #require(smbTest)
            let id = UUID().uuidString
            let name = "deck-" + id.prefix(8).lowercased()
            try await rc("config/create", ["name": name, "type": "smb",
                                           "parameters": ["host": share.host, "port": String(share.port), "user": SMBRemote.guest],
                                           "opt": ["obscure": true, "nonInteractive": true]])
            let path = "games/vndegen-tests/\(id)"
            let server = TestServer(kind: kind, root: "\(name):\(path)", folder: nil, path: path)
            try await rc("operations/mkdir", ["fs": server.root, "remote": ""])
            return server
        }
    }

    /// Runs `body` with a new store folder, then deletes the folder from the test share, whether `body` failed or not.
    @MainActor
    static func with(_ kind: StoreKind, _ body: (TestServer) async throws -> Void) async throws {
        let server = try await make(kind)
        do {
            try await body(server)
        } catch {
            try? await server.remove()
            throw error
        }
        try await server.remove()
    }

    /// Deletes the test's own folder on the share. A local folder stays with the other temporary files.
    func remove() async throws {
        guard kind == .smb else { return }
        try await rc("operations/purge", ["fs": root, "remote": ""])
    }

    /// The file at `path` in the store, or nil when there is none.
    func read(_ path: String) async throws -> Data? {
        let scratch = try temporaryDirectory()
        do {
            try await rc("operations/copyfile", ["srcFs": root, "srcRemote": path, "dstFs": scratch.path, "dstRemote": "file"])
        } catch let error as RcloneError where error.status == 404 {
            return nil
        }
        return try Data(contentsOf: scratch.appendingPathComponent("file"))
    }

    func write(_ data: Data, to path: String) async throws {
        let scratch = try temporaryDirectory()
        try data.write(to: scratch.appendingPathComponent("file"))
        try await rc("operations/copyfile", ["srcFs": scratch.path, "srcRemote": "file", "dstFs": root, "dstRemote": path, "_config": ["IgnoreTimes": true]])
    }

    /// The names in `folder` of the store, hidden ones included.
    func names(in folder: String) async throws -> Set<String> {
        let reply = try await rc("operations/list", ["fs": root, "remote": folder])
        let items = (try JSONSerialization.jsonObject(with: reply) as? [String: Any])?["list"] as? [[String: Any]] ?? []
        return Set(items.compactMap { $0["Name"] as? String })
    }

    /// A copy of the store with only the game folder `game`, as Ludusavi's download makes one. Returns its root.
    func download(_ game: String) async throws -> URL {
        let copy = try temporaryDirectory()
        try fm.createDirectory(at: copy.appendingPathComponent(game), withIntermediateDirectories: true)
        if try await names(in: "").contains(game) {
            try await rc("sync/sync", ["srcFs": "\(root)/\(game)", "dstFs": copy.appendingPathComponent(game).path])
        }
        return copy
    }

    /// Makes the server's game folder `game` match the copy at `copy`, as Ludusavi's upload is a sync. Every file goes,
    /// whatever its size and time.
    func upload(_ copy: URL, game: String) async throws {
        try await rc("sync/sync", ["srcFs": copy.appendingPathComponent(game).path, "dstFs": "\(root)/\(game)", "_config": ["IgnoreTimes": true]])
    }
}

/// One rc call with JSON in and out, with the app's limits under any `_config` of the input's own.
@discardableResult
func rc(_ method: String, _ input: [String: Any]) async throws -> Data {
    var input = input
    input["_config"] = RcloneLimits().config.merging(input["_config"] as? [String: Any] ?? [:]) { _, own in own }
    return try await Rclone.call(method, json: JSONSerialization.data(withJSONObject: input))
}

/// Lets a test act between the rc calls of an operation, the way another device would.
final class Hooks: @unchecked Sendable {
    enum Moment { case before, after }

    private struct Pending {
        var moment: Moment
        var method: String
        var remaining: Int
        var action: @Sendable () async throws -> Void
    }

    private let lock = NSLock()
    private var pending: [Pending] = []
    private var connectHook: (@Sendable (Int) async throws -> Void)?
    private var next: RemoteAccess?
    private var connected = 0

    /// Runs `action` once, right before the `nth` call of `method` from now on. A throw fails that call unsent.
    func before(_ method: String, _ nth: Int = 1, _ action: @escaping @Sendable () async throws -> Void) {
        lock.withLock { pending.append(Pending(moment: .before, method: method, remaining: nth, action: action)) }
    }

    /// Runs `action` once, right after the `nth` call of `method` from now on returned.
    func after(_ method: String, _ nth: Int = 1, _ action: @escaping @Sendable () async throws -> Void) {
        lock.withLock { pending.append(Pending(moment: .after, method: method, remaining: nth, action: action)) }
    }

    /// Runs before each connection opens, with how many opened before it.
    var beforeConnect: (@Sendable (Int) async throws -> Void)? {
        get { lock.withLock { connectHook } }
        set { lock.withLock { connectHook = newValue } }
    }

    /// Used in place of the test's store for the next connection only.
    var nextAccess: RemoteAccess? {
        get { lock.withLock { next } }
        set { lock.withLock { next = newValue } }
    }

    /// How many connections opened.
    var connections: Int { lock.withLock { connected } }

    func connect(_ make: @Sendable () async throws -> RemoteAccess) async throws -> RemoteAccess {
        let (hook, index, replacement) = lock.withLock {
            defer { connected += 1; next = nil }
            return (connectHook, connected, next)
        }
        try await hook?(index)
        var access: RemoteAccess
        if let replacement { access = replacement } else { access = try await make() }
        let call = access.call
        access.call = { method, input in
            try await self.fire(.before, method)
            let output = try await call(method, input)
            try await self.fire(.after, method)
            return output
        }
        return access
    }

    private func fire(_ moment: Moment, _ method: String) async throws {
        let due: [Pending] = lock.withLock {
            var due: [Pending] = []
            for index in pending.indices.reversed() where pending[index].moment == moment && pending[index].method == method {
                pending[index].remaining -= 1
                if pending[index].remaining == 0 { due.insert(pending.remove(at: index), at: 0) }
            }
            return due
        }
        for hook in due { try await hook.action() }
    }
}

final class Locked<Value>: @unchecked Sendable {
    private let lock = NSLock()
    private var value: Value

    init(_ value: Value) { self.value = value }

    func callAsFunction<R>(_ body: (inout Value) -> R) -> R { lock.withLock { body(&value) } }
}

/// A TCP server that takes connections and never answers, as a share that stopped responding does. The system accepts
/// connections into the backlog on its own, so nothing here reads or writes.
final class SilentServer: @unchecked Sendable {
    let port: Int
    private let listener: Int32

    init() throws {
        struct Failed: Error {}
        let listener = socket(AF_INET, SOCK_STREAM, 0)
        var address = sockaddr_in()
        address.sin_family = sa_family_t(AF_INET)
        address.sin_addr.s_addr = inet_addr("127.0.0.1")
        #if canImport(Darwin)
        address.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
        #endif
        var length = socklen_t(MemoryLayout<sockaddr_in>.size)
        let bound = withUnsafeMutablePointer(to: &address) { pointer in
            pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) { address in
                bind(listener, address, length) == 0 && listen(listener, 16) == 0 && getsockname(listener, address, &length) == 0
            }
        }
        guard listener >= 0, bound else { throw Failed() }
        self.listener = listener
        port = Int(UInt16(bigEndian: address.sin_port))
    }

    deinit { close(listener) }
}
