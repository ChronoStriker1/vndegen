import Foundation

/// A byte pipe to a running `rsync --server`: an SSH exec channel, or a local process in tests.
public protocol RsyncTransport: Sendable {
    /// The server's standard output, in the order it arrived. It must be drained as fast as it comes, so a server that
    /// writes while VNDegen is still writing never waits on VNDegen.
    var output: AsyncThrowingStream<[UInt8], any Error> { get }
    func write(_ bytes: [UInt8]) async throws
    /// Waits for the server to exit. Returns its exit status and what it wrote to standard error.
    func finish() async throws -> (status: Int32, errorOutput: String)
}

/// Reads rsync's wire format: little-endian integers, and after the handshake the server's multiplexed stream, where
/// each packet carries a tag. Data packets form the protocol stream. The others are log and error messages.
final class WireReader {
    private var iterator: AsyncThrowingStream<[UInt8], any Error>.AsyncIterator
    private var raw: [UInt8] = []
    private var rawStart = 0
    private var data: [UInt8] = []
    private var dataStart = 0
    var multiplexed = false
    /// Error and warning text the server sent in the stream, for the error a failed transfer reports.
    private(set) var messages: [String] = []

    init(_ stream: AsyncThrowingStream<[UInt8], any Error>) {
        iterator = stream.makeAsyncIterator()
    }

    func int32() async throws -> Int32 {
        let bytes = try await read(4)
        return Int32(bitPattern: UInt32(bytes[0]) | UInt32(bytes[1]) << 8 | UInt32(bytes[2]) << 16 | UInt32(bytes[3]) << 24)
    }

    /// rsync's `read_longint`: an int32, or -1 followed by an int64.
    func int64() async throws -> Int64 {
        let small = try await int32()
        guard small == -1 else { return Int64(small) }
        let bytes = try await read(8)
        return Int64(bitPattern: bytes.enumerated().reduce(0) { $0 | UInt64($1.element) << (8 * UInt64($1.offset)) })
    }

    func byte() async throws -> UInt8 { try await read(1)[0] }

    func read(_ count: Int) async throws -> [UInt8] {
        guard count > 0 else { return [] }
        if multiplexed {
            while data.count - dataStart < count { try await readPacket() }
            defer { dataStart += count; compact(&data, &dataStart) }
            return Array(data[dataStart ..< dataStart + count])
        }
        try await fillRaw(count)
        defer { rawStart += count; compact(&raw, &rawStart) }
        return Array(raw[rawStart ..< rawStart + count])
    }

    private func readPacket() async throws {
        try await fillRaw(4)
        let header = UInt32(raw[rawStart]) | UInt32(raw[rawStart + 1]) << 8 | UInt32(raw[rawStart + 2]) << 16 | UInt32(raw[rawStart + 3]) << 24
        let length = Int(header & 0x00FF_FFFF)
        let tag = Int(header >> 24) - 7
        try await fillRaw(4 + length)
        let payload = raw[rawStart + 4 ..< rawStart + 4 + length]
        rawStart += 4 + length
        compact(&raw, &rawStart)
        switch tag {
        case 0: data.append(contentsOf: payload)
        // MSG_ERROR_XFER, MSG_ERROR, MSG_WARNING, MSG_ERROR_SOCKET, MSG_ERROR_UTF8
        case 1, 3, 4, 5, 8: messages.append(String(decoding: payload, as: UTF8.self).trimmingCharacters(in: .whitespacesAndNewlines))
        // MSG_ERROR_EXIT: the server is about to exit with an error. Its stderr and exit status carry the detail.
        case 86: throw RsyncError.serverFailed(messages.last ?? "the server stopped with an error")
        // Info, log, I/O error flags, keepalives, and per-file success notes.
        default: break
        }
    }

    private func fillRaw(_ count: Int) async throws {
        while raw.count - rawStart < count {
            guard let chunk = try await iterator.next() else { throw RsyncError.connectionClosed(messages.last) }
            raw.append(contentsOf: chunk)
        }
    }

    private func compact(_ buffer: inout [UInt8], _ start: inout Int) {
        if start == buffer.count {
            buffer.removeAll(keepingCapacity: true)
            start = 0
        } else if start > 1 << 16 {
            buffer.removeFirst(start)
            start = 0
        }
    }
}

/// Builds the bytes of one or more protocol messages, written with a single transport write.
struct WireBuffer {
    var bytes: [UInt8] = []

    mutating func byte(_ value: UInt8) { bytes.append(value) }

    mutating func int32(_ value: Int32) {
        withUnsafeBytes(of: value.littleEndian) { bytes.append(contentsOf: $0) }
    }

    /// rsync's `write_longint`.
    mutating func int64(_ value: Int64) {
        if value >= 0, value <= Int64(Int32.max) { return int32(Int32(value)) }
        int32(-1)
        withUnsafeBytes(of: value.littleEndian) { bytes.append(contentsOf: $0) }
    }

    mutating func append(_ data: some Sequence<UInt8>) { bytes.append(contentsOf: data) }
}

public enum RsyncError: LocalizedError, Equatable {
    case connectionClosed(String?)
    case serverFailed(String)
    case protocolViolation(String)
    case checksumMismatch(String)
    case localFile(String)

    public var errorDescription: String? {
        switch self {
        case let .connectionClosed(message): "The server closed the rsync connection\(message.map { ": \($0)" } ?? ".")"
        case let .serverFailed(message): "rsync on the server failed: \(message)"
        case let .protocolViolation(detail): "The server's rsync sent something unexpected (\(detail))."
        case let .checksumMismatch(path): "\(path) arrived damaged. Nothing was changed."
        case let .localFile(detail): detail
        }
    }
}
