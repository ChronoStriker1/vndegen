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
    /// The server sent nothing and took nothing for longer than the connection allows.
    case timedOut

    public var errorDescription: String? {
        switch self {
        case let .unknownHostKey(key): "The server's identity is new. Check that its fingerprint is \(SSHKeys.fingerprint(of: key) ?? key), then trust it."
        case let .hostKeyChanged(presented):
            "The server's identity changed. It is now \(SSHKeys.fingerprint(of: presented) ?? presented). If you did not reinstall the server, someone may be in the way. VNDegen did not connect."
        case let .authenticationFailed(user): "The server did not accept VNDegen's key for \(user). Add the public key to that account's authorized_keys."
        case let .unreachable(detail): "Can't reach the server: \(detail)"
        case .commandRefused: "The server refused to run a command."
        case .timedOut: "The server stopped answering, so VNDegen gave up."
        }
    }
}

/// An SSH connection that runs rsync on the server. Logs in with VNDegen's own key and connects only to a server whose
/// host key matches the pinned one.
public final class SSHConnection: RsyncConnection {
    private let channel: Channel
    private let idleTimeout: TimeAmount

    private init(channel: Channel, idleTimeout: TimeAmount) {
        self.channel = channel
        self.idleTimeout = idleTimeout
    }

    /// Connects and logs in. Throws `SSHError.unknownHostKey` with the presented key when `hostKey` is nil, so the caller
    /// can ask the user to trust it and connect again with it pinned. A command that sends and receives nothing for
    /// `idleTimeout`, including while the server opens its channel and accepts it, closes the whole connection.
    /// Cancelling the caller closes a connection that is still being made and throws `CancellationError`.
    public static func connect(to server: SSHServer, key: Curve25519.Signing.PrivateKey, hostKey: String?,
                               timeout: Duration = .seconds(15), idleTimeout: Duration = .seconds(30)) async throws -> SSHConnection {
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
        let connecting = bootstrap.connect(host: server.host, port: server.port)
        let connected = connecting.eventLoop.makePromise(of: Channel.self)
        connecting.cascade(to: connected)
        let channel: Channel
        do {
            channel = try await withTaskCancellationHandler {
                try await connected.futureResult.get()
            } onCancel: {
                connected.fail(CancellationError())
                // A socket that connects after the caller gave up is closed as soon as it does.
                connecting.whenSuccess { $0.close(promise: nil) }
            }
        } catch {
            if Task.isCancelled { throw CancellationError() }
            throw SSHError.unreachable(error.localizedDescription)
        }
        outcome.channel = channel
        let timer = channel.eventLoop.scheduleTask(in: TimeAmount(timeout)) {
            outcome.fail(.unreachable("no answer within \(timeout.components.seconds) seconds"))
        }
        defer { timer.cancel() }
        do {
            try await withTaskCancellationHandler {
                try await outcome.loggedIn.futureResult.get()
            } onCancel: {
                outcome.loggedIn.fail(CancellationError())
                channel.close(promise: nil)
            }
            try Task.checkCancellation()
            return SSHConnection(channel: channel, idleTimeout: TimeAmount(idleTimeout))
        } catch {
            try? await channel.close()
            if Task.isCancelled { throw CancellationError() }
            throw outcome.error ?? SSHError.unreachable(error.localizedDescription)
        }
    }

    public func close() async {
        try? await channel.close()
    }

    /// Runs `arguments` through the login shell, each one quoted.
    public func start(_ arguments: [String]) async throws -> any RsyncTransport {
        let command = arguments.map(Self.shellQuoted).joined(separator: " ")
        let exec = ExecHandler(eventLoop: channel.eventLoop, idleTimeout: idleTimeout)
        let created = channel.eventLoop.makePromise(of: Channel.self)
        let channel = channel
        // A server that never opens the channel or never answers the exec request would leave both waits below open.
        let deadline = channel.eventLoop.scheduleTask(in: idleTimeout) {
            created.fail(SSHError.timedOut)
            exec.abandon(SSHError.timedOut)
            channel.close(promise: nil)
        }
        defer { deadline.cancel() }
        do {
            try await channel.eventLoop.submit {
                try channel.pipeline.syncOperations.handler(type: NIOSSHHandler.self).createChannel(created, channelType: .session) { child, _ in
                    child.eventLoop.makeCompletedFuture {
                        try child.pipeline.syncOperations.addHandler(exec)
                    }
                }
            }.get()
        } catch {
            // A closed connection has no SSH handler left. The promises must still end, or NIO reports them leaked.
            created.fail(SSHError.commandRefused)
            exec.abandon(SSHError.commandRefused)
            throw SSHError.commandRefused
        }
        let child: Channel
        do { child = try await created.futureResult.get() } catch SSHError.timedOut { throw SSHError.timedOut } catch { throw SSHError.commandRefused }
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
/// the server never waits for VNDegen to read. When nothing moves either way for `idleTimeout`, it ends the stream and
/// the exit wait with `SSHError.timedOut` and closes the whole connection, because a stalled server may never answer
/// a request to close just this channel.
private final class ExecHandler: ChannelDuplexHandler, @unchecked Sendable {
    typealias InboundIn = SSHChannelData
    typealias OutboundIn = SSHChannelData
    typealias OutboundOut = SSHChannelData

    let stream: AsyncThrowingStream<[UInt8], any Error>
    private let continuation: AsyncThrowingStream<[UInt8], any Error>.Continuation
    let accepted: EventLoopPromise<Void>
    let exited: EventLoopPromise<(status: Int32, errorOutput: String)>
    private var status: Int32?
    private var errorOutput = ""
    private let idleTimeout: TimeAmount
    // The fields below change only on the channel's event loop.
    private var lastActivity = NIODeadline.now()
    private var watchdog: RepeatedTask?
    private let timedOutFlag = NIOLockedValueBox(false)

    init(eventLoop: EventLoop, idleTimeout: TimeAmount) {
        (stream, continuation) = AsyncThrowingStream.makeStream()
        accepted = eventLoop.makePromise()
        exited = eventLoop.makePromise()
        self.idleTimeout = idleTimeout
    }

    var timedOut: Bool { timedOutFlag.withLockedValue { $0 } }

    /// Ends every wait on a command whose channel never opened.
    func abandon(_ error: SSHError) {
        accepted.fail(error)
        exited.fail(error)
        continuation.finish(throwing: error)
    }

    func handlerAdded(context: ChannelHandlerContext) {
        lastActivity = .now()
        let channel = context.channel
        let interval = min(idleTimeout, .seconds(1))
        watchdog = context.eventLoop.scheduleRepeatedTask(initialDelay: interval, delay: interval) { [self] task in
            guard NIODeadline.now() - lastActivity >= idleTimeout else { return }
            task.cancel()
            timedOutFlag.withLockedValue { $0 = true }
            accepted.fail(SSHError.timedOut)
            exited.fail(SSHError.timedOut)
            continuation.finish(throwing: SSHError.timedOut)
            (channel.parent ?? channel).close(promise: nil)
        }
    }

    func handlerRemoved(context: ChannelHandlerContext) {
        watchdog?.cancel()
    }

    func write(context: ChannelHandlerContext, data: NIOAny, promise: EventLoopPromise<Void>?) {
        lastActivity = .now()
        context.write(data, promise: promise)
    }

    func channelRead(context: ChannelHandlerContext, data: NIOAny) {
        lastActivity = .now()
        let message = unwrapInboundIn(data)
        guard case let .byteBuffer(buffer) = message.data else { return }
        switch message.type {
        case .channel: continuation.yield(Array(buffer.readableBytesView))
        case .stdErr: errorOutput += String(buffer: buffer)
        default: break
        }
    }

    func userInboundEventTriggered(context: ChannelHandlerContext, event: Any) {
        lastActivity = .now()
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
        watchdog?.cancel()
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
        do {
            try await channel.writeAndFlush(SSHChannelData(type: .channel, data: .byteBuffer(ByteBuffer(bytes: bytes))))
        } catch {
            throw exec.timedOut ? SSHError.timedOut : error
        }
    }

    func finish() async throws -> (status: Int32, errorOutput: String) {
        try await exec.exited.futureResult.get()
    }
}

extension TimeAmount {
    init(_ duration: Duration) {
        let (seconds, attoseconds) = duration.components
        self = .nanoseconds(seconds * 1_000_000_000 + attoseconds / 1_000_000_000)
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
