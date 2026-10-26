import Foundation

/// Starts `rsync` on the server, such as over an SSH exec channel.
public protocol RsyncConnection: Sendable {
    /// Runs rsync with these arguments. `arguments[0]` is the program.
    func start(_ arguments: [String]) async throws -> any RsyncTransport
    func close() async
}

/// An rsync client that speaks protocol 27 to `rsync --server`, the protocol openrsync and gokrazy/rsync speak. Every
/// rsync since 2.6 accepts it. It sends and receives whole files and leaves out block matching. Each call runs one
/// rsync command, like one run of `rsync -e ssh`.
public struct RsyncClient: Sendable {
    public let connection: any RsyncConnection
    /// The rsync command on the server.
    public var program: String

    static let protocolVersion: Int32 = 27
    static let chunkSize = 32 * 1024

    public init(connection: any RsyncConnection, program: String = "rsync") {
        self.connection = connection
        self.program = program
    }

    // MARK: Receiving

    /// The entries under `remote`, a folder path ending in `/`, that `filters` let through. Like `rsync --list-only`.
    public func list(_ remote: String, filters: [RsyncFilter] = [], owners: Bool = false) async throws -> [RsyncEntry] {
        try await receive(remote, filters: filters, owners: owners, into: nil).entries
    }

    /// Copies the files under `remote` that `filters` let through into `local`, as `rsync -rt` does. Files whose size and
    /// modification time already match are skipped. Every file is checked against the sender's checksum and replaces
    /// the old one in one rename. Nothing is deleted. Returns every entry the server listed, with owners when `owners`
    /// is set. Local files keep the local owner.
    ///
    /// `arrived` runs once, with every entry the server listed, as soon as every file the pull asked for is in place with
    /// its size and checksum checked. The server's closing statistics and its exit come after it, so a caller can act on
    /// the files without waiting for them. The pull still fails afterwards if the server does not end cleanly.
    @discardableResult
    public func pull(_ remote: String, into local: URL, filters: [RsyncFilter] = [], owners: Bool = false,
                     arrived: (@Sendable ([RsyncEntry]) async throws -> Void)? = nil) async throws -> PullReport {
        let (entries, transferred) = try await receive(remote, filters: filters, owners: owners, into: local, arrived: arrived)
        return PullReport(entries: entries, transferred: transferred)
    }

    public struct PullReport: Sendable {
        public var entries: [RsyncEntry]
        /// Paths of the files that were copied, because they were missing or differed.
        public var transferred: [String]
    }

    private func receive(_ remote: String, filters: [RsyncFilter], owners: Bool, into destination: URL?,
                         arrived: (@Sendable ([RsyncEntry]) async throws -> Void)? = nil) async throws -> (entries: [RsyncEntry], transferred: [String]) {
        let transport = try await connection.start([program, "--server", "--sender", owners ? "-rog" : "-r", ".", remote])
        let session = try await Session(transport)
        var buffer = WireBuffer()
        buffer.filters(filters)
        try await transport.write(buffer.bytes)

        let (entries, _) = try await FileListCodec.read(session.reader, owners: owners)
        var wanted: [Int: URL] = [:]
        if let destination {
            for (index, entry) in entries.enumerated() {
                guard let target = try Self.localURL(entry.path, in: destination) else { continue }
                switch entry.kind {
                case .directory:
                    try Self.makeDirectory(target)
                case .file:
                    if !Self.matches(target, entry) { wanted[index] = target }
                }
            }
        }

        // The generator's requests. A zero checksum header asks for the whole file. The two end markers close the
        // transfer phase and the redo phase, which never has work because no file is retried.
        buffer = WireBuffer()
        for index in wanted.keys.sorted() {
            buffer.int32(Int32(index))
            for _ in 0 ..< 4 { buffer.int32(0) }
        }
        buffer.int32(-1)
        buffer.int32(-1)
        try await transport.write(buffer.bytes)

        var transferred: [String] = []
        if wanted.isEmpty { try await arrived?(entries) }
        var phase = 0
        while true {
            let index = try await session.reader.int32()
            if index == -1 {
                phase += 1
                if phase == 2 { break }
                continue
            }
            guard let target = wanted[Int(index)] else { throw RsyncError.protocolViolation("file \(index), which was not requested") }
            let entry = entries[Int(index)]
            try await receiveFile(entry, to: target, session: session)
            transferred.append(entry.path)
            if transferred.count == wanted.count { try await arrived?(entries) }
        }
        // Bytes read, bytes written, and total size, which only rsync's own client prints.
        for _ in 0 ..< 3 { _ = try await session.reader.int64() }
        buffer = WireBuffer()
        buffer.int32(-1)
        try await transport.write(buffer.bytes)
        try await session.finish()
        return (entries, transferred)
    }

    private func receiveFile(_ entry: RsyncEntry, to target: URL, session: Session) async throws {
        let reader = session.reader
        let blocks = try await reader.int32()
        for _ in 0 ..< 3 { _ = try await reader.int32() }
        guard blocks == 0 else { throw RsyncError.protocolViolation("block checksums for a whole-file request") }

        let temporary = target.deletingLastPathComponent().appendingPathComponent(".\(target.lastPathComponent).vndegen-\(UUID().uuidString)")
        guard FileManager.default.createFile(atPath: temporary.path, contents: nil),
              let handle = try? FileHandle(forWritingTo: temporary) else {
            throw RsyncError.localFile("Can't write \(target.path).")
        }
        var md4 = MD4()
        md4.update(session.seed)
        do {
            defer { try? handle.close() }
            while true {
                let token = try await reader.int32()
                if token == 0 { break }
                guard token > 0 else { throw RsyncError.protocolViolation("a block reference in a whole-file transfer") }
                let data = try await reader.read(Int(token))
                md4.update(data)
                try handle.write(contentsOf: data)
            }
            let expected = try await reader.read(16)
            guard md4.finalize() == expected, try handle.offset() == UInt64(entry.size) else { throw RsyncError.checksumMismatch(entry.path) }
            try handle.synchronize()
        } catch {
            try? FileManager.default.removeItem(at: temporary)
            throw error
        }
        do {
            try FileManager.default.setAttributes([.modificationDate: Date(timeIntervalSince1970: TimeInterval(entry.modified))],
                                                  ofItemAtPath: temporary.path)
            guard rename(temporary.path, target.path) == 0 else { throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO) }
        } catch {
            try? FileManager.default.removeItem(at: temporary)
            throw RsyncError.localFile("Can't write \(target.path): \(error.localizedDescription)")
        }
    }

    /// rsync's quick check: same size and same modification second.
    static func matches(_ url: URL, _ entry: RsyncEntry) -> Bool {
        guard let attributes = try? FileManager.default.attributesOfItem(atPath: url.path),
              attributes[.type] as? FileAttributeType == .typeRegular,
              let size = attributes[.size] as? NSNumber, let modified = attributes[.modificationDate] as? Date else { return false }
        return size.int64Value == entry.size && Int64(modified.timeIntervalSince1970.rounded(.down)) == entry.modified
    }

    /// Where `path` goes under `root`. Nil for the root itself. Throws for a path that could leave `root`.
    static func localURL(_ path: String, in root: URL) throws -> URL? {
        guard let parts = RsyncEntry.safeComponents(path) else { throw RsyncError.protocolViolation("the unsafe path \(path)") }
        guard !parts.isEmpty else { return nil }
        return parts.reduce(root) { $0.appendingPathComponent($1) }
    }

    private static func makeDirectory(_ url: URL) throws {
        var isDirectory: ObjCBool = false
        if FileManager.default.fileExists(atPath: url.path, isDirectory: &isDirectory), isDirectory.boolValue { return }
        do {
            try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        } catch {
            throw RsyncError.localFile("Can't create \(url.path): \(error.localizedDescription)")
        }
    }

    // MARK: Sending

    /// A file or folder to create on the server, read from `source` for a file.
    public struct Upload: Sendable {
        public var entry: RsyncEntry
        public var source: URL?

        public init(entry: RsyncEntry, source: URL?) {
            self.entry = entry
            self.source = source
        }
    }

    /// Sends `uploads` into the folder `remote`, as `rsync -rtpI` does, plus owner and group when `owners` is set. Every
    /// upload is transferred, even one whose size and time match. Each file lands under a temporary name and replaces
    /// the old one in one rename. Folders listed before their contents are created first.
    ///
    /// With `confirm`, each file waits until the server has asked for its content, then goes only when `confirm`
    /// returns true for its path. A file `confirm` declines is never sent, and the server never opens a temporary file
    /// for it, so its copy on the server stays as it was. Returns the paths that were sent.
    @discardableResult
    public func push(_ uploads: [Upload], to remote: String, owners: Bool,
                     confirm: (@Sendable (String) async throws -> Bool)? = nil) async throws -> [String] {
        let sorted = uploads.sorted { $0.entry.path.utf8.lexicographicallyPrecedes($1.entry.path.utf8) }
        for upload in sorted where RsyncEntry.safeComponents(upload.entry.path) == nil {
            throw RsyncError.localFile("\(upload.entry.path) is not a path inside the folder.")
        }
        // -O leaves folder times alone, so a backup does not change the time on the game folder.
        let transport = try await connection.start([program, "--server", owners ? "-rtpogOI" : "-rtpOI", ".", remote])
        let session = try await Session(transport)
        var buffer = WireBuffer()
        FileListCodec.write(sorted.map(\.entry), owners: owners, into: &buffer)
        try await transport.write(buffer.bytes)
        return try await sendRequested(sorted, session: session, confirm: confirm)
    }

    /// Deletes the items directly in the folder `remote` whose names are in `names`, folders with all their contents, as
    /// `rsync -r --delete` with filter rules does. Everything else is excluded, which protects it.
    public func delete(_ names: [String], in remote: String) async throws {
        guard !names.isEmpty else { return }
        for name in names where name.contains("/") || RsyncEntry.safeComponents(name) == nil {
            throw RsyncError.localFile("\(name) is not a name in the folder.")
        }
        let transport = try await connection.start([program, "--server", "-r", "--delete", ".", remote])
        let session = try await Session(transport)
        var buffer = WireBuffer()
        // `name/***` is the folder and everything in it. Contents left excluded would keep the folder from being deleted.
        let included = names.map(Self.escapedPattern).flatMap { [RsyncFilter.include("/\($0)"), .include("/\($0)/***")] }
        buffer.filters(included + [.exclude("*")])
        let root = RsyncEntry(path: ".", kind: .directory, size: 0, modified: Int64(Date.now.timeIntervalSince1970), permissions: 0o755)
        FileListCodec.write([root], owners: false, into: &buffer)
        try await transport.write(buffer.bytes)
        try await sendRequested([Upload(entry: root, source: nil)], session: session, confirm: nil)
    }

    /// rsync's wildcards are `*`, `?`, and `[`. A backslash makes the next character plain.
    public static func escapedPattern(_ name: String) -> String {
        name.reduce(into: "") { result, character in
            if "*?[\\".contains(character) { result.append("\\") }
            result.append(character)
        }
    }

    /// The sender's side after the file list: answers each file the server asks for with its whole content, or with
    /// nothing when `confirm` declines it, as rsync's own sender does with a file it can't open.
    private func sendRequested(_ uploads: [Upload], session: Session, confirm: (@Sendable (String) async throws -> Bool)?) async throws -> [String] {
        let reader = session.reader
        var sent: [String] = []
        var phase = 0
        while true {
            let index = try await reader.int32()
            if index == -1 {
                phase += 1
                var buffer = WireBuffer()
                buffer.int32(-1)
                try await session.transport.write(buffer.bytes)
                if phase == 2 { break }
                continue
            }
            guard uploads.indices.contains(Int(index)), let source = uploads[Int(index)].source else {
                throw RsyncError.protocolViolation("a request for file \(index)")
            }
            // The server sends checksums of its copy for block matching, which this client skips. It echoes the header.
            let header = [try await reader.int32(), try await reader.int32(), try await reader.int32(), try await reader.int32()]
            guard header[0] >= 0, header[2] >= 0, header[2] <= 16 else { throw RsyncError.protocolViolation("a checksum header \(header)") }
            _ = try await reader.read(Int(header[0]) * (4 + Int(header[2])))
            if let confirm, try await !confirm(uploads[Int(index)].entry.path) { continue }

            let data: Data
            do { data = try Data(contentsOf: source) } catch { throw RsyncError.localFile("Can't read \(source.path).") }
            guard Int64(data.count) == uploads[Int(index)].entry.size else {
                throw RsyncError.localFile("\(source.path) changed while it was being sent.")
            }
            var buffer = WireBuffer()
            buffer.int32(index)
            header.forEach { buffer.int32($0) }
            var md4 = MD4()
            md4.update(session.seed)
            md4.update(data)
            var offset = 0
            while offset < data.count {
                let end = min(offset + Self.chunkSize, data.count)
                buffer.int32(Int32(end - offset))
                buffer.append(data[offset ..< end])
                offset = end
                if buffer.bytes.count >= 4 * Self.chunkSize {
                    try await session.transport.write(buffer.bytes)
                    buffer = WireBuffer()
                }
            }
            buffer.int32(0)
            buffer.append(md4.finalize())
            try await session.transport.write(buffer.bytes)
            sent.append(uploads[Int(index)].entry.path)
        }
        // The receiver's goodbye.
        guard try await reader.int32() == -1 else { throw RsyncError.protocolViolation("no final goodbye") }
        try await session.finish()
        return sent
    }
}

extension WireBuffer {
    /// The client's filter list: each rule with its length, then a zero length.
    mutating func filters(_ filters: [RsyncFilter]) {
        for filter in filters {
            let rule = Array(filter.rule.utf8)
            int32(Int32(rule.count))
            append(rule)
        }
        int32(0)
    }
}

/// One running rsync command after the protocol handshake.
final class Session {
    let transport: any RsyncTransport
    let reader: WireReader
    let seed: Int32

    init(_ transport: any RsyncTransport) async throws {
        self.transport = transport
        reader = WireReader(transport.output)
        var buffer = WireBuffer()
        buffer.int32(RsyncClient.protocolVersion)
        do {
            try await transport.write(buffer.bytes)
            let remote = try await reader.int32()
            guard remote >= RsyncClient.protocolVersion else {
                throw RsyncError.serverFailed("the server speaks rsync protocol \(remote), older than \(RsyncClient.protocolVersion)")
            }
            seed = try await reader.int32()
        } catch let error as RsyncError {
            // A server that failed to start, such as a missing rsync or folder, says why on stderr.
            if case .connectionClosed = error, let exit = try? await transport.finish(), exit.status != 0 {
                throw RsyncError.serverFailed(Self.describe(exit.status, exit.errorOutput))
            }
            throw error
        }
        reader.multiplexed = true
    }

    /// Waits for the server to exit, and fails unless it exited cleanly.
    func finish() async throws {
        let (status, errorOutput) = try await transport.finish()
        guard status == 0 else {
            throw RsyncError.serverFailed(Self.describe(status, reader.messages.joined(separator: "\n") + errorOutput))
        }
    }

    static func describe(_ status: Int32, _ output: String) -> String {
        let text = output.trimmingCharacters(in: .whitespacesAndNewlines)
        return text.isEmpty ? "exit status \(status)" : text
    }
}
