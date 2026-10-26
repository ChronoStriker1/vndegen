import CryptoKit
import Foundation
import NIOConcurrencyHelpers
import NIOCore
import NIOPosix
import NIOSSH
import Testing
@testable import Rsync
import RsyncTestSupport

private var fm: FileManager { .default }


private func temporaryDirectory() throws -> URL {
    let url = fm.temporaryDirectory.appendingPathComponent("rsync-\(UUID().uuidString)", isDirectory: true)
    try fm.createDirectory(at: url, withIntermediateDirectories: true)
    return url.resolvingSymlinksInPath()
}

private func write(_ files: [String: String], under root: URL) throws {
    for (path, content) in files {
        let url = root.appendingPathComponent(path)
        try fm.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data(content.utf8).write(to: url)
    }
}

private func files(under root: URL) throws -> [String: String] {
    var result: [String: String] = [:]
    let base = root.pathComponents.count
    for case let url as URL in fm.enumerator(at: root, includingPropertiesForKeys: [.isRegularFileKey])! {
        guard try url.resourceValues(forKeys: [.isRegularFileKey]).isRegularFile == true else { continue }
        result[url.resolvingSymlinksInPath().pathComponents.dropFirst(base).joined(separator: "/")] = String(decoding: try Data(contentsOf: url), as: UTF8.self)
    }
    return result
}

@Test func md4MatchesTheRFCVectors() {
    let hex = { (text: String) in MD4.hash(Array(text.utf8)).map { String(format: "%02x", $0) }.joined() }
    #expect(hex("") == "31d6cfe0d16ae931b73c59d7e0c089c0")
    #expect(hex("abc") == "a448017aaf21d8525fc10ae87aa6729d")
    #expect(hex("message digest") == "d9130a8164549fe818874806e1c7014b")
    #expect(hex(String(repeating: "1234567890", count: 8)) == "e33b4ddc9c38f2199c3e7b164fcc0536")
}

@Test(.enabled(if: rsyncPath != nil)) func pullsAFilteredTreeAndSkipsUnchangedFiles() async throws {
    let remote = try temporaryDirectory(), local = try temporaryDirectory()
    try write(["Game A/mapping.yaml": "a", "Game A/backup-1/drive-0/save.dat": String(repeating: "x", count: 100_000),
               "Game B/mapping.yaml": "bb", "Game C [1]/mapping.yaml": "c"], under: remote)
    let client = RsyncClient(connection: ProcessConnection())
    let filters: [RsyncFilter] = [.include("/*/"), .include("/*/mapping.yaml"), .exclude("*")]

    let first = try await client.pull(remote.path + "/", into: local, filters: filters)
    #expect(try files(under: local) == ["Game A/mapping.yaml": "a", "Game B/mapping.yaml": "bb", "Game C [1]/mapping.yaml": "c"])
    #expect(first.transferred.sorted() == ["Game A/mapping.yaml", "Game B/mapping.yaml", "Game C [1]/mapping.yaml"])
    #expect(first.entries.map(\.path) == [".", "Game A", "Game A/mapping.yaml", "Game B", "Game B/mapping.yaml", "Game C [1]", "Game C [1]/mapping.yaml"])

    try write(["Game B/mapping.yaml": "changed"], under: remote)
    let second = try await client.pull(remote.path + "/", into: local, filters: filters)
    #expect(second.transferred == ["Game B/mapping.yaml"])
    #expect(try files(under: local)["Game B/mapping.yaml"] == "changed")

    let whole = try await client.pull(remote.path + "/Game A/", into: local.appendingPathComponent("Game A"))
    #expect(whole.transferred.sorted() == ["backup-1/drive-0/save.dat"])
    #expect(try files(under: local)["Game A/backup-1/drive-0/save.dat"]?.count == 100_000)
}

@Test(.enabled(if: rsyncPath != nil)) func listsWithOwnersAndReportsAMissingFolder() async throws {
    let remote = try temporaryDirectory()
    try write(["Game/mapping.yaml": "a"], under: remote)
    let client = RsyncClient(connection: ProcessConnection())
    let entries = try await client.list(remote.path + "/", filters: [.include("/*/"), .exclude("/*/*")], owners: true)
    #expect(entries.map(\.path) == [".", "Game"])
    let owner = try #require(entries.first?.owner)
    #expect(owner.uid == Int32(getuid()))
    #expect(owner.user == NSUserName())

    await #expect(throws: RsyncError.self) {
        try await client.list(remote.path + "/missing/")
    }
}

@Test(.enabled(if: rsyncPath != nil)) func pushesNewFoldersAndFilesWithTimesAndModes() async throws {
    let remote = try temporaryDirectory(), local = try temporaryDirectory()
    try write(["Game/mapping.yaml": "old"], under: remote)
    let big = String(repeating: "0123456789", count: 20_000)
    try write(["backup-2/drive-0/home/deck/save.dat": big, "mapping.yaml": "new mapping"], under: local)
    let time: Int64 = 1_760_000_000
    let uploads = ["backup-2", "backup-2/drive-0", "backup-2/drive-0/home", "backup-2/drive-0/home/deck"].map {
        RsyncClient.Upload(entry: RsyncEntry(path: $0, kind: .directory, size: 0, modified: time, permissions: 0o777), source: nil)
    } + ["backup-2/drive-0/home/deck/save.dat", "mapping.yaml"].map {
        let url = local.appendingPathComponent($0)
        let size = Int64(try! Data(contentsOf: url).count)
        return RsyncClient.Upload(entry: RsyncEntry(path: $0, kind: .file, size: size, modified: time, permissions: 0o666), source: url)
    }
    let client = RsyncClient(connection: ProcessConnection())
    try await client.push(uploads, to: remote.path + "/Game/", owners: false)

    let game = remote.appendingPathComponent("Game")
    #expect(try files(under: game) == ["backup-2/drive-0/home/deck/save.dat": big, "mapping.yaml": "new mapping"])
    let attributes = try fm.attributesOfItem(atPath: game.appendingPathComponent("mapping.yaml").path)
    #expect((attributes[.modificationDate] as? Date)?.timeIntervalSince1970 == TimeInterval(time))
    #expect((attributes[.posixPermissions] as? NSNumber)?.intValue == 0o666)
    #expect((try fm.attributesOfItem(atPath: game.appendingPathComponent("backup-2").path)[.posixPermissions] as? NSNumber)?.intValue == 0o777)

    // A file that already exists with the same size and time is sent again, as -I asks.
    try write(["mapping.yaml": "NEW mapping"], under: local)
    try await client.push(Array(uploads.suffix(1)), to: game.path + "/", owners: false)
    #expect(try files(under: game)["mapping.yaml"] == "NEW mapping")
}

@Test(.enabled(if: rsyncPath != nil)) func pushSetsOwnerAndGroupByName() async throws {
    let remote = try temporaryDirectory(), local = try temporaryDirectory()
    try write(["save.dat": "x"], under: local)
    let client = RsyncClient(connection: ProcessConnection())
    let owner = try #require(try await client.list(remote.path + "/", owners: true).first?.owner)
    let upload = RsyncClient.Upload(entry: RsyncEntry(path: "save.dat", kind: .file, size: 1, modified: 1_700_000_000, permissions: 0o644,
                                                      owner: owner), source: local.appendingPathComponent("save.dat"))
    try await client.push([upload], to: remote.path + "/", owners: true)
    let attributes = try fm.attributesOfItem(atPath: remote.appendingPathComponent("save.dat").path)
    #expect((attributes[.ownerAccountID] as? NSNumber)?.int32Value == owner.uid)
    #expect((attributes[.groupOwnerAccountID] as? NSNumber)?.int32Value == owner.gid)
}

@Test(.enabled(if: rsyncPath != nil)) func deletesOnlyTheNamedItems() async throws {
    let remote = try temporaryDirectory()
    try write(["Game/backup-1/drive-0/a.dat": "1", "Game/backup-1-diff/drive-0/a.dat": "2", "Game/backup-[2]/x": "3",
               "Game/backup-3/drive-0/a.dat": "4", "Game/mapping.yaml": "m"], under: remote)
    let client = RsyncClient(connection: ProcessConnection())
    try await client.delete(["backup-1", "backup-[2]"], in: remote.path + "/Game/")
    #expect(try files(under: remote.appendingPathComponent("Game")) == ["backup-1-diff/drive-0/a.dat": "2", "backup-3/drive-0/a.dat": "4",
                                                                       "mapping.yaml": "m"])
}

@Test func unsafePathsAreRejected() {
    #expect(RsyncEntry.safeComponents("a/b") == ["a", "b"])
    #expect(RsyncEntry.safeComponents(".") == [])
    for path in ["../a", "/a", "a//b", "a/../b", "a/./b", ""] { #expect(RsyncEntry.safeComponents(path) == nil) }
}

/// Over SSH to this Mac, when a test key is given: `VNDEGEN_SSH_TEST_KEY` holds the base64 Ed25519 seed of a key in this
/// account's authorized_keys.
private let sshTestKey = ProcessInfo.processInfo.environment["VNDEGEN_SSH_TEST_KEY"].flatMap { Data(base64Encoded: $0) }

@Test(.enabled(if: sshTestKey != nil && rsyncPath != nil)) func pinsTheHostKeyAndSyncsOverSSH() async throws {
    let key = try Curve25519.Signing.PrivateKey(rawRepresentation: sshTestKey!)
    let server = SSHServer(host: "localhost", user: NSUserName())

    var presented = ""
    do {
        _ = try await SSHConnection.connect(to: server, key: key, hostKey: nil)
        Issue.record("connected without a pinned host key")
    } catch let SSHError.unknownHostKey(hostKey) {
        presented = hostKey
    }
    #expect(SSHKeys.fingerprint(of: presented)?.hasPrefix("SHA256:") == true)

    let impostor = SSHKeys.authorizedKeysLine(for: .init(), comment: "")
    await #expect(throws: SSHError.hostKeyChanged(presented: presented)) {
        try await SSHConnection.connect(to: server, key: key, hostKey: impostor)
    }
    await #expect(throws: SSHError.authenticationFailed(user: server.user)) {
        try await SSHConnection.connect(to: server, key: .init(), hostKey: presented)
    }

    let connection = try await SSHConnection.connect(to: server, key: key, hostKey: presented)
    let remote = try temporaryDirectory(), local = try temporaryDirectory()
    try write(["Game's [1]/mapping.yaml": "m", "Game's [1]/backup-1/drive-0/a.dat": String(repeating: "a", count: 300_000)], under: remote)
    let client = RsyncClient(connection: connection, program: rsyncPath!)
    let pulled = try await client.pull(remote.path + "/", into: local)
    #expect(pulled.transferred.count == 2)
    #expect(try files(under: local) == (try files(under: remote)))

    try write(["Game's [1]/mapping.yaml": "changed"], under: local)
    let url = local.appendingPathComponent("Game's [1]/mapping.yaml")
    try await client.push([.init(entry: RsyncEntry(path: "mapping.yaml", kind: .file, size: 7, modified: 1_700_000_000, permissions: 0o644),
                                 source: url)], to: remote.path + "/Game's [1]/", owners: false)
    try await client.delete(["backup-1"], in: remote.path + "/Game's [1]/")
    #expect(try files(under: remote) == ["Game's [1]/mapping.yaml": "changed"])
    await connection.close()
}

/// An SSH server that accepts a test key, from `VNDEGEN_SSH_TEST` ("host port user"), `VNDEGEN_SSH_KEY` (the raw
/// Ed25519 private key, base64), and `VNDEGEN_SSH_HOSTKEY` (the server's OpenSSH host key line).
private let sshTest: (server: SSHServer, key: Curve25519.Signing.PrivateKey, hostKey: String)? = {
    let environment = ProcessInfo.processInfo.environment
    guard let target = environment["VNDEGEN_SSH_TEST"]?.split(separator: " "), target.count == 3, let port = Int(target[1]),
          let raw = environment["VNDEGEN_SSH_KEY"].flatMap({ Data(base64Encoded: $0) }),
          let key = try? Curve25519.Signing.PrivateKey(rawRepresentation: raw), let hostKey = environment["VNDEGEN_SSH_HOSTKEY"] else { return nil }
    return (SSHServer(host: String(target[0]), port: port, user: String(target[2])), key, hostKey)
}()

@Test(.enabled(if: sshTest != nil)) func aCommandThatStopsAnsweringClosesTheConnection() async throws {
    let test = try #require(sshTest)
    let connection = try await SSHConnection.connect(to: test.server, key: test.key, hostKey: test.hostKey, idleTimeout: .seconds(2))
    let echo = try await connection.start(["echo", "hi"])
    var output: [UInt8] = []
    for try await chunk in echo.output { output += chunk }
    #expect(String(decoding: output, as: UTF8.self) == "hi\n")
    #expect(try await echo.finish().status == 0)

    let start = ContinuousClock.now
    let sleeper = try await connection.start(["sleep", "60"])
    await #expect(throws: SSHError.timedOut) { for try await _ in sleeper.output {} }
    await #expect(throws: SSHError.timedOut) { try await sleeper.finish() }
    #expect(ContinuousClock.now - start < .seconds(10))
    await #expect(throws: (any Error).self) { _ = try await connection.start(["true"]) }
    await connection.close()
}

@Test(.enabled(if: rsyncPath != nil)) func aDeclinedFileIsNeverSent() async throws {
    let remote = try temporaryDirectory(), local = try temporaryDirectory()
    try write(["Game/mapping.yaml": "old"], under: remote)
    try write(["mapping.yaml": "new mapping"], under: local)
    let url = local.appendingPathComponent("mapping.yaml")
    let upload = RsyncClient.Upload(entry: RsyncEntry(path: "mapping.yaml", kind: .file, size: 11, modified: 1_760_000_000, permissions: 0o666),
                                    source: url)
    let client = RsyncClient(connection: ProcessConnection())
    let game = remote.appendingPathComponent("Game")

    // The server asks for the file, and the answer is no: its copy stays, and no temporary file is left beside it.
    let asked = NIOLockedValueBox<[String]>([])
    let declined = try await client.push([upload], to: game.path + "/", owners: false) { path in
        asked.withLockedValue { $0.append(path) }
        return false
    }
    #expect(declined.isEmpty && asked.withLockedValue { $0 } == ["mapping.yaml"])
    #expect(try fm.contentsOfDirectory(atPath: game.path) == ["mapping.yaml"])
    #expect(try files(under: game) == ["mapping.yaml": "old"])

    let sent = try await client.push([upload], to: game.path + "/", owners: false) { _ in true }
    #expect(sent == ["mapping.yaml"])
    #expect(try files(under: game) == ["mapping.yaml": "new mapping"])
}

/// A local server for SSH tests. With `ssh` false it accepts TCP connections and never says anything, as a server
/// stuck before its SSH banner. With `ssh` true it speaks SSH, takes any key, opens channels, and never answers a
/// command.
private final class StuckServer {
    let channel: Channel
    let hostKey: String

    init(ssh: Bool) async throws {
        let key = NIOSSHPrivateKey(ed25519Key: .init())
        hostKey = String(openSSHPublicKey: key.publicKey)
        channel = try await ServerBootstrap(group: MultiThreadedEventLoopGroup.singleton)
            .childChannelInitializer { child in
                child.eventLoop.makeCompletedFuture {
                    guard ssh else { return }
                    let handler = NIOSSHHandler(role: .server(.init(hostKeys: [key], userAuthDelegate: AnyKey())), allocator: child.allocator) { channel, _ in
                        channel.eventLoop.makeCompletedFuture { try channel.pipeline.syncOperations.addHandler(IgnoreRequests()) }
                    }
                    try child.pipeline.syncOperations.addHandler(handler)
                }
            }
            .bind(host: "127.0.0.1", port: 0).get()
    }

    var server: SSHServer { SSHServer(host: "127.0.0.1", port: channel.localAddress?.port ?? 0, user: "test") }

    private final class AnyKey: NIOSSHServerUserAuthenticationDelegate, @unchecked Sendable {
        var supportedAuthenticationMethods: NIOSSHAvailableUserAuthenticationMethods { .publicKey }

        func requestReceived(request: NIOSSHUserAuthenticationRequest, responsePromise: EventLoopPromise<NIOSSHUserAuthenticationOutcome>) {
            responsePromise.succeed(.success)
        }
    }

    /// Swallows channel requests such as exec, so none is ever answered.
    private final class IgnoreRequests: ChannelInboundHandler, @unchecked Sendable {
        typealias InboundIn = SSHChannelData
        func userInboundEventTriggered(context: ChannelHandlerContext, event: Any) {}
    }
}

@Test func cancellingALoginInProgressEndsItAtOnce() async throws {
    let stuck = try await StuckServer(ssh: false)
    defer { stuck.channel.close(promise: nil) }
    let start = ContinuousClock.now
    let server = stuck.server, hostKey = stuck.hostKey
    let login = Task { _ = try await SSHConnection.connect(to: server, key: .init(), hostKey: hostKey) }
    try await Task.sleep(for: .milliseconds(300))
    login.cancel()
    await #expect(throws: CancellationError.self) { try await login.value }
    #expect(ContinuousClock.now - start < .seconds(3))
}

@Test func aCommandTheServerNeverAcceptsTimesOut() async throws {
    let stuck = try await StuckServer(ssh: true)
    defer { stuck.channel.close(promise: nil) }
    let connection = try await SSHConnection.connect(to: stuck.server, key: .init(), hostKey: stuck.hostKey, idleTimeout: .seconds(1))
    let start = ContinuousClock.now
    await #expect(throws: SSHError.timedOut) { _ = try await connection.start(["rsync", "--server"]) }
    #expect(ContinuousClock.now - start < .seconds(5))
    await connection.close()
}

@Test func aServerThatCannotBeReachedFailsCleanly() async throws {
    // A port that was free a moment ago, so nothing listens on it.
    let probe = try await ServerBootstrap(group: MultiThreadedEventLoopGroup.singleton).bind(host: "127.0.0.1", port: 0).get()
    let port = probe.localAddress?.port ?? 1
    try await probe.close()
    for server in [SSHServer(host: "no-such-host.invalid", user: "test"), SSHServer(host: "127.0.0.1", port: port, user: "test")] {
        await #expect(throws: SSHError.self, "\(server.host)") { _ = try await SSHConnection.connect(to: server, key: .init(), hostKey: nil) }
    }
}
