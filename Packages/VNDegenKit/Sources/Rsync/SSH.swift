import CryptoKit
import Foundation
import NIOConcurrencyHelpers
import NIOCore
import NIOPosix
import NIOSSH

/// Where to log in: the server, port, and account.
public struct SSHServer: Codable, Equatable, Sendable {
    public var host: String
    public var port: Int
    public var user: String

    public init(host: String, port: Int = 22, user: String) {
        self.host = host
        self.port = port
        self.user = user
    }
}

public enum SSHError: LocalizedError, Equatable {
    /// The server's host key is not pinned yet. Carries the key it presented, as an OpenSSH line.
    case unknownHostKey(String)
    /// The server presented a different key from the pinned one.
    case hostKeyChanged(presented: String)
    case authenticationFailed(user: String)
    case unreachable(String)
    case commandRefused

    public var errorDescription: String? {
        switch self {
        case let .unknownHostKey(key): "The server's identity is new. Check that its fingerprint is \(SSHKeys.fingerprint(of: key) ?? key), then trust it."
        case let .hostKeyChanged(presented):
            "The server's identity changed. It is now \(SSHKeys.fingerprint(of: presented) ?? presented). If you did not reinstall the server, someone may be in the way. VNDegen did not connect."
        case let .authenticationFailed(user): "The server did not accept VNDegen's key for \(user). Add the public key to that account's authorized_keys."
        case let .unreachable(detail): "Can't reach the server: \(detail)"
        case .commandRefused: "The server refused to run a command."
        }
    }
}

/// An SSH connection that runs rsync on the server. Logs in with VNDegen's own key and connects only to a server whose
/// host key matches the pinned one.
public final class SSHConnection: RsyncConnection {
    private let channel: Channel

    private init(channel: Channel) {
        self.channel = channel
    }

    /// Connects and logs in. Throws `SSHError.unknownHostKey` with the presented key when `hostKey` is nil, so the caller
    /// can ask the user to trust it and connect again with it pinned.
    public static func connect(to server: SSHServer, key: Curve25519.Signing.PrivateKey, hostKey: String?,
                               timeout: Duration = .seconds(15)) async throws -> SSHConnection {
        let outcome = ConnectionOutcome()
        let hostCheck = HostKeyCheck(pinned: hostKey, outcome: outcome)
        let login = KeyLogin(user: server.user, key: NIOSSHPrivateKey(ed25519Key: key), outcome: outcome)
        let bootstrap = ClientBootstrap(group: MultiThreadedEventLoopGroup.singleton)
            .connectTimeout(.seconds(10))
            .channelOption(ChannelOptions.socket(SocketOptionLevel(IPPROTO_TCP), TCP_NODELAY), value: 1)
            .channelInitializer { channel in
                channel.eventLoop.makeCompletedFuture {
                    let ssh = NIOSSHHandler(role: .client(.init(userAuthDelegate: login, serverAuthDelegate: hostCheck)),
                                            allocator: channel.allocator, inboundChildChannelInitializer: nil)
                    try channel.pipeline.syncOperations.addHandlers([ssh, LoginWatcher(outcome: outcome)])
                }
            }
        let channel: Channel
        do {
            channel = try await bootstrap.connect(host: server.host, port: server.port).get()
        } catch {
            throw SSHError.unreachable(error.localizedDescription)
        }
        outcome.channel = channel
        let timer = channel.eventLoop.scheduleTask(in: .seconds(timeout.components.seconds)) {
            outcome.fail(.unreachable("no answer within \(timeout.components.seconds) seconds"))
        }
        defer { timer.cancel() }
        do {
            try await outcome.loggedIn.futureResult.get()
            return SSHConnection(channel: channel)
        } catch {
            try? await channel.close()
            throw outcome.error ?? SSHError.unreachable(error.localizedDescription)
        }
    }

    public func close() async {
        try? await channel.close()
    }

    /// Runs `arguments` through the login shell, each one quoted.
    public func start(_ arguments: [String]) async throws -> any RsyncTransport {
        let command = arguments.map(Self.shellQuoted).joined(separator: " ")
        let exec = ExecHandler(eventLoop: channel.eventLoop)
        let created = channel.eventLoop.makePromise(of: Channel.self)
        let channel = channel
        try await channel.eventLoop.submit {
            try channel.pipeline.syncOperations.handler(type: NIOSSHHandler.self).createChannel(created, channelType: .session) { child, _ in
                child.eventLoop.makeCompletedFuture {
                    try child.pipeline.syncOperations.addHandler(exec)
                }
            }
        }.get()
        let child: Channel
        do { child = try await created.futureResult.get() } catch { throw SSHError.commandRefused }
        try await child.setOption(ChannelOptions.allowRemoteHalfClosure, value: true).get()
        try await child.triggerUserOutboundEvent(SSHChannelRequestEvent.ExecRequest(command: command, wantReply: true)).get()
        try await exec.accepted.futureResult.get()
        return ExecTransport(channel: child, exec: exec)
    }

    /// POSIX shell quoting: single quotes, with each `'` written as `'\''`.
    static func shellQuoted(_ argument: String) -> String {
        if !argument.isEmpty, argument.allSatisfy({ $0.isASCII && ($0.isLetter || $0.isNumber || "-_./=:,".contains($0)) }) { return argument }
        return "'" + argument.replacingOccurrences(of: "'", with: "'\\''") + "'"
    }
}

/// What a connection attempt learned, shared by the delegates that see each part of it.
private final class ConnectionOutcome: @unchecked Sendable {
    private let lock = NIOLock()
    private var firstError: SSHError?
    private var _channel: Channel?
    let loggedIn: EventLoopPromise<Void>

    init() {
        loggedIn = MultiThreadedEventLoopGroup.singleton.any().makePromise()
    }

    var error: SSHError? { lock.withLock { firstError } }

    var channel: Channel? {
        get { lock.withLock { _channel } }
        set { lock.withLock { _channel = newValue } }
    }

    /// Keeps the first failure, which explains the ones after it, and ends the attempt.
    func fail(_ error: SSHError) {
        lock.withLock { if firstError == nil { firstError = error } }
        loggedIn.fail(error)
        channel?.close(promise: nil)
    }
}

private final class HostKeyCheck: NIOSSHClientServerAuthenticationDelegate, @unchecked Sendable {
    let pinned: String?
    let outcome: ConnectionOutcome

    init(pinned: String?, outcome: ConnectionOutcome) {
        self.pinned = pinned
        self.outcome = outcome
    }

    func validateHostKey(hostKey: NIOSSHPublicKey, validationCompletePromise: EventLoopPromise<Void>) {
        let presented = String(openSSHPublicKey: hostKey)
        let error: SSHError? = switch pinned.map(SSHKeys.keyPart) {
        case nil: .unknownHostKey(presented)
        case SSHKeys.keyPart(presented)?: nil
        default: .hostKeyChanged(presented: presented)
        }
        if let error {
            outcome.fail(error)
            validationCompletePromise.fail(error)
        } else {
            validationCompletePromise.succeed()
        }
    }
}

/// Offers VNDegen's key once. When the server asks again, the key was refused.
private final class KeyLogin: NIOSSHClientUserAuthenticationDelegate, @unchecked Sendable {
    let user: String
    let key: NIOSSHPrivateKey
    let outcome: ConnectionOutcome
    private var offered = false

    init(user: String, key: NIOSSHPrivateKey, outcome: ConnectionOutcome) {
        self.user = user
        self.key = key
        self.outcome = outcome
    }

    func nextAuthenticationType(availableMethods: NIOSSHAvailableUserAuthenticationMethods,
                                nextChallengePromise: EventLoopPromise<NIOSSHUserAuthenticationOffer?>) {
        guard !offered, availableMethods.contains(.publicKey) else {
            nextChallengePromise.succeed(nil)
            outcome.fail(.authenticationFailed(user: user))
            return
        }
        offered = true
        nextChallengePromise.succeed(.init(username: user, serviceName: "", offer: .privateKey(.init(privateKey: key))))
    }
}

private final class LoginWatcher: ChannelInboundHandler, @unchecked Sendable {
    typealias InboundIn = Any
    let outcome: ConnectionOutcome

    init(outcome: ConnectionOutcome) { self.outcome = outcome }

    func userInboundEventTriggered(context: ChannelHandlerContext, event: Any) {
        if event is UserAuthSuccessEvent { outcome.loggedIn.succeed() }
        context.fireUserInboundEventTriggered(event)
    }

    func errorCaught(context: ChannelHandlerContext, error: any Error) {
        outcome.fail(.unreachable(String(describing: error)))
        context.close(promise: nil)
    }

    func channelInactive(context: ChannelHandlerContext) {
        outcome.fail(.unreachable("the server closed the connection"))
        context.fireChannelInactive()
    }
}

/// One exec channel. Its data goes into `stream` as it arrives, and SSH's flow control window reopens as it is read, so
/// the server never waits for VNDegen to read.
private final class ExecHandler: ChannelInboundHandler, @unchecked Sendable {
    typealias InboundIn = SSHChannelData

    let stream: AsyncThrowingStream<[UInt8], any Error>
    private let continuation: AsyncThrowingStream<[UInt8], any Error>.Continuation
    let accepted: EventLoopPromise<Void>
    let exited: EventLoopPromise<(status: Int32, errorOutput: String)>
    private var status: Int32?
    private var errorOutput = ""

    init(eventLoop: EventLoop) {
        (stream, continuation) = AsyncThrowingStream.makeStream()
        accepted = eventLoop.makePromise()
        exited = eventLoop.makePromise()
    }

    func channelRead(context: ChannelHandlerContext, data: NIOAny) {
        let message = unwrapInboundIn(data)
        guard case let .byteBuffer(buffer) = message.data else { return }
        switch message.type {
        case .channel: continuation.yield(Array(buffer.readableBytesView))
        case .stdErr: errorOutput += String(buffer: buffer)
        default: break
        }
    }

    func userInboundEventTriggered(context: ChannelHandlerContext, event: Any) {
        switch event {
        case is ChannelSuccessEvent: accepted.succeed()
        case is ChannelFailureEvent: accepted.fail(SSHError.commandRefused)
        case let exit as SSHChannelRequestEvent.ExitStatus: status = Int32(exit.exitStatus)
        case let event as ChannelEvent where event == .inputClosed: continuation.finish()
        default: context.fireUserInboundEventTriggered(event)
        }
    }

    func errorCaught(context: ChannelHandlerContext, error: any Error) {
        continuation.finish(throwing: error)
        context.close(promise: nil)
    }

    func channelInactive(context: ChannelHandlerContext) {
        continuation.finish()
        accepted.fail(SSHError.commandRefused)
        // A server killed by a signal reports no exit status.
        exited.succeed((status ?? -1, errorOutput))
    }
}

private final class ExecTransport: RsyncTransport {
    private let channel: Channel
    private let exec: ExecHandler

    init(channel: Channel, exec: ExecHandler) {
        self.channel = channel
        self.exec = exec
    }

    var output: AsyncThrowingStream<[UInt8], any Error> { exec.stream }

    func write(_ bytes: [UInt8]) async throws {
        try await channel.writeAndFlush(SSHChannelData(type: .channel, data: .byteBuffer(ByteBuffer(bytes: bytes))))
    }

    func finish() async throws -> (status: Int32, errorOutput: String) {
        try await exec.exited.futureResult.get()
    }
}

/// VNDegen's SSH key and the key formats OpenSSH shows.
public enum SSHKeys {
    /// `ssh-ed25519 AAAA... comment`, the line that goes into `authorized_keys`.
    public static func authorizedKeysLine(for key: Curve25519.Signing.PrivateKey, comment: String) -> String {
        String(openSSHPublicKey: NIOSSHPrivateKey(ed25519Key: key).publicKey) + " " + comment
    }

    /// `SHA256:...`, as `ssh-keygen -lf` prints it.
    public static func fingerprint(of openSSHKey: String) -> String? {
        let parts = openSSHKey.split(separator: " ")
        guard parts.count >= 2, let blob = Data(base64Encoded: String(parts[1])) else { return nil }
        return "SHA256:" + Data(SHA256.hash(data: blob)).base64EncodedString().trimmingCharacters(in: ["="])
    }

    /// The type and key of an OpenSSH line, without its comment.
    static func keyPart(_ line: String) -> String {
        line.split(separator: " ").prefix(2).joined(separator: " ")
    }
}
