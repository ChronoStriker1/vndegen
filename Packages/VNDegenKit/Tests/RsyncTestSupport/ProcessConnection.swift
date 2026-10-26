import Foundation
import Rsync

/// The real rsync, run as `rsync --server` with pipes in place of SSH. `VNDEGEN_RSYNC` picks one, such as macOS's
/// openrsync at /usr/bin/rsync. Else Homebrew's 3.x when present, as on the build Mac and the servers VNDegen talks to.
public let rsyncPath = ([ProcessInfo.processInfo.environment["VNDEGEN_RSYNC"]].compactMap { $0 }
    + ["/opt/homebrew/bin/rsync", "/usr/local/bin/rsync", "/usr/bin/rsync"]).first { FileManager.default.isExecutableFile(atPath: $0) }

public struct ProcessConnection: RsyncConnection {
    public init() {}

    public func start(_ arguments: [String]) async throws -> any RsyncTransport {
        try ProcessTransport(program: rsyncPath!, arguments: Array(arguments.dropFirst()))
    }

    public func close() async {}
}


/// Runs rsync as a child process. Its output is drained by readability handlers, as the SSH channel drains its data.
public final class ProcessTransport: RsyncTransport, @unchecked Sendable {
    public let output: AsyncThrowingStream<[UInt8], any Error>
    private let process = Process()
    private let input = Pipe()
    private let errors = Pipe()
    private let lock = NSLock()
    private var errorText = Data()
    private var exitStatus: Int32?
    private var waiting: CheckedContinuation<Int32, Never>?

    init(program: String, arguments: [String]) throws {
        let stdout = Pipe()
        let (stream, continuation) = AsyncThrowingStream<[UInt8], any Error>.makeStream()
        output = stream
        stdout.fileHandleForReading.readabilityHandler = { handle in
            let data = handle.availableData
            if data.isEmpty {
                handle.readabilityHandler = nil
                continuation.finish()
            } else {
                continuation.yield(Array(data))
            }
        }
        errors.fileHandleForReading.readabilityHandler = { [lock] handle in
            let data = handle.availableData
            if data.isEmpty { handle.readabilityHandler = nil }
            lock.withLock { self.errorText.append(data) }
        }
        process.terminationHandler = { [lock] process in
            lock.withLock {
                self.exitStatus = process.terminationStatus
                self.waiting?.resume(returning: process.terminationStatus)
                self.waiting = nil
            }
        }
        process.executableURL = URL(fileURLWithPath: program)
        process.arguments = arguments
        process.standardInput = input
        process.standardOutput = stdout
        process.standardError = errors
        try process.run()
    }

    public func write(_ bytes: [UInt8]) async throws {
        try input.fileHandleForWriting.write(contentsOf: bytes)
    }

    public func finish() async throws -> (status: Int32, errorOutput: String) {
        // waitUntilExit can wait forever off the thread that launched the process, so this waits for the handler.
        let status: Int32 = await withCheckedContinuation { continuation in
            lock.withLock {
                if let exitStatus { continuation.resume(returning: exitStatus) } else { waiting = continuation }
            }
        }
        try? input.fileHandleForWriting.close()
        let text = lock.withLock { String(decoding: errorText, as: UTF8.self) }
        return (status, text)
    }
}
