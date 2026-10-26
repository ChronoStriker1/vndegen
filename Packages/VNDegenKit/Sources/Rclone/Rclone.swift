import CRclone
import Foundation
import Synchronization

/// rclone's remote control API, built into VNDegen (`Rclone/rclone.go`). Each method is one of rclone's rc calls, such
/// as `operations/list` or `sync/copy`, with JSON in and out. Ludusavi drives rclone's command line; iOS can't launch a
/// program, so VNDegen calls the same code in process.
public enum Rclone {
    /// Runs the rc call `method` with `input` and decodes its reply. Cancelling the caller cancels the call, which then
    /// ends at rclone's next step, or for a request already sent, when the server answers or the idle timeout passes.
    /// This returns only once rclone has, so a cancelled call never acts after its caller moved on.
    public static func call<Output: Decodable>(_ method: String, _ input: some Encodable, as _: Output.Type = Output.self) async throws -> Output {
        let data = try await call(method, json: JSONEncoder().encode(input))
        return try JSONDecoder().decode(Output.self, from: data)
    }

    /// `call` for methods whose reply carries nothing VNDegen reads.
    public static func call(_ method: String, _ input: some Encodable) async throws {
        _ = try await call(method, json: JSONEncoder().encode(input))
    }

    /// `call` with raw JSON in and out.
    public static func call(_ method: String, json input: Data) async throws -> Data {
        _ = initialized
        let id = nextID.next()
        let reply: (Data, Int32) = try await withTaskCancellationHandler {
            try Task.checkCancellation()
            return await withCheckedContinuation { continuation in
                // rclone blocks the calling thread until the call ends, so it gets a thread of its own rather than one
                // of the concurrency pool's.
                Thread.detachNewThread {
                    var status: Int32 = 0
                    let output = String(decoding: input, as: UTF8.self).withCString { input in
                        method.withCString { VNDRcloneCall(id, $0, input, &status) }
                    }
                    defer { VNDRcloneFree(output) }
                    continuation.resume(returning: (Data(bytes: output!, count: strlen(output!)), status))
                }
            }
        } onCancel: {
            VNDRcloneCancel(id)
        }
        guard reply.1 == 200 else {
            let failure = try? JSONDecoder().decode(Failure.self, from: reply.0)
            if Task.isCancelled { throw CancellationError() }
            throw RcloneError(method: method, status: Int(reply.1), message: failure?.error ?? String(decoding: reply.0, as: UTF8.self))
        }
        return reply.0
    }

    private struct Failure: Decodable {
        var error: String
    }

    private static let initialized: Void = VNDRcloneInitialize()
    private static let nextID = Counter()

    private final class Counter: Sendable {
        private let value = Mutex(Int64(0))
        func next() -> Int64 { value.withLock { $0 += 1; return $0 } }
    }
}

/// An rc call that failed, with rclone's own message.
public struct RcloneError: LocalizedError, Equatable, Sendable {
    public var method: String
    /// The HTTP status rclone gave: 404 for no such file or method, 400 for bad input, 500 otherwise.
    public var status: Int
    public var message: String

    public var errorDescription: String? { message }
}
