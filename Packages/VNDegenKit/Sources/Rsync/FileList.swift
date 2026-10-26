import Foundation

/// One entry of an rsync file list, relative to the transfer's root folder.
public struct RsyncEntry: Sendable, Equatable {
    public enum Kind: Sendable, Equatable { case file, directory }

    /// Forward slashes. `.` is the root folder itself.
    public var path: String
    public var kind: Kind
    public var size: Int64
    /// Seconds since 1970, as rsync sends them.
    public var modified: Int64
    /// Permission bits only.
    public var permissions: UInt32
    public var owner: RsyncOwner?

    public init(path: String, kind: Kind, size: Int64, modified: Int64, permissions: UInt32, owner: RsyncOwner? = nil) {
        self.path = path
        self.kind = kind
        self.size = size
        self.modified = modified
        self.permissions = permissions
        self.owner = owner
    }
}

/// A file's owner and group on the server, with their names when the server sent them.
public struct RsyncOwner: Sendable, Equatable, Codable {
    public var uid: Int32
    public var gid: Int32
    public var user: String?
    public var group: String?

    public init(uid: Int32, gid: Int32, user: String? = nil, group: String? = nil) {
        self.uid = uid
        self.gid = gid
        self.user = user
        self.group = group
    }
}

/// Filter rules in rsync's syntax, sent to the server. The first rule that matches a path decides.
public enum RsyncFilter: Sendable, Equatable {
    case include(String)
    case exclude(String)

    var rule: String {
        switch self {
        case let .include(pattern): "+ \(pattern)"
        case let .exclude(pattern): "- \(pattern)"
        }
    }
}

/// File list flags of protocol 27 (rsync.h).
enum FileListFlag {
    static let topDir: UInt8 = 0x01
    static let sameMode: UInt8 = 0x02
    static let sameUID: UInt8 = 0x08
    static let sameGID: UInt8 = 0x10
    static let sameName: UInt8 = 0x20
    static let longName: UInt8 = 0x40
    static let sameTime: UInt8 = 0x80
}

enum FileMode {
    static let typeMask: UInt32 = 0o170000
    static let directory: UInt32 = 0o040000
    static let regular: UInt32 = 0o100000
}

extension RsyncEntry {
    /// rsync's order for protocol 27: by the bytes of the path. Both sides sort the list, and file indexes point into it.
    static func sorted(_ entries: [RsyncEntry]) -> [RsyncEntry] {
        entries.sorted { $0.path.utf8.lexicographicallyPrecedes($1.path.utf8) }
    }

    /// Nil when the path could leave the folder it is relative to, such as `../x`, `/x`, or `a//b`.
    static func safeComponents(_ path: String) -> [String]? {
        guard path != "." else { return [] }
        let parts = path.split(separator: "/", omittingEmptySubsequences: false).map(String.init)
        guard !parts.isEmpty, !parts.contains(where: { $0.isEmpty || $0 == "." || $0 == ".." || $0.contains("\0") }) else { return nil }
        return parts
    }
}

/// Reading and writing the file list with `WireReader` and `WireBuffer`.
enum FileListCodec {
    /// Reads entries until the end marker, then the owner and group name lists when `owners` is set, then the I/O error
    /// flag. Entries come back in rsync's sorted order. Device files and symlinks never arrive, because VNDegen does not
    /// ask for them. Any other kind is rejected.
    static func read(_ reader: WireReader, owners: Bool) async throws -> (entries: [RsyncEntry], ioError: Bool) {
        var entries: [RsyncEntry] = []
        var lastName: [UInt8] = []
        var last = RsyncEntry(path: "", kind: .file, size: 0, modified: 0, permissions: 0)
        var lastMode: UInt32 = 0
        var lastUID: Int32 = 0, lastGID: Int32 = 0
        while true {
            let flags = try await reader.byte()
            if flags == 0 { break }
            let shared = flags & FileListFlag.sameName != 0 ? Int(try await reader.byte()) : 0
            let length = flags & FileListFlag.longName != 0 ? Int(try await reader.int32()) : Int(try await reader.byte())
            guard shared <= lastName.count, length >= 0, shared + length < 4096 else {
                throw RsyncError.protocolViolation("a file name of \(shared + length) bytes")
            }
            let name = Array(lastName.prefix(shared)) + (try await reader.read(length))
            lastName = name
            let size = try await reader.int64()
            let modified = flags & FileListFlag.sameTime != 0 ? last.modified : Int64(try await reader.int32())
            let mode = flags & FileListFlag.sameMode != 0 ? lastMode : UInt32(bitPattern: try await reader.int32())
            lastMode = mode
            var owner: RsyncOwner?
            if owners {
                lastUID = flags & FileListFlag.sameUID != 0 ? lastUID : try await reader.int32()
                lastGID = flags & FileListFlag.sameGID != 0 ? lastGID : try await reader.int32()
                owner = RsyncOwner(uid: lastUID, gid: lastGID)
            }
            let kind: RsyncEntry.Kind
            switch mode & FileMode.typeMask {
            case FileMode.directory: kind = .directory
            case FileMode.regular: kind = .file
            default: throw RsyncError.protocolViolation("an entry of type \(String(mode & FileMode.typeMask, radix: 8))")
            }
            guard let path = String(validating: name, as: UTF8.self) else { throw RsyncError.protocolViolation("a file name that is not UTF-8") }
            last = RsyncEntry(path: path, kind: kind, size: size, modified: modified, permissions: mode & 0o7777, owner: owner)
            entries.append(last)
        }
        if owners {
            let users = try await readNames(reader)
            let groups = try await readNames(reader)
            for index in entries.indices {
                guard let owner = entries[index].owner else { continue }
                entries[index].owner = RsyncOwner(uid: owner.uid, gid: owner.gid, user: users[owner.uid], group: groups[owner.gid])
            }
        }
        let ioError = try await reader.int32() != 0
        return (RsyncEntry.sorted(entries), ioError)
    }

    private static func readNames(_ reader: WireReader) async throws -> [Int32: String] {
        var names: [Int32: String] = [:]
        while true {
            let id = try await reader.int32()
            if id == 0 { return names }
            let length = Int(try await reader.byte())
            names[id] = String(decoding: try await reader.read(length), as: UTF8.self)
        }
    }

    /// Writes `entries`, already sorted, then the name lists when `owners` is set, then a clear I/O error flag.
    static func write(_ entries: [RsyncEntry], owners: Bool, into buffer: inout WireBuffer) {
        var users: [Int32: String] = [:], groups: [Int32: String] = [:]
        for entry in entries {
            var flags = FileListFlag.longName
            if entry.path == "." { flags |= FileListFlag.topDir }
            buffer.byte(flags)
            let name = Array(entry.path.utf8)
            buffer.int32(Int32(name.count))
            buffer.append(name)
            buffer.int64(entry.size)
            buffer.int32(Int32(truncatingIfNeeded: entry.modified))
            let type = entry.kind == .directory ? FileMode.directory : FileMode.regular
            buffer.int32(Int32(bitPattern: type | entry.permissions & 0o7777))
            if owners {
                let owner = entry.owner ?? RsyncOwner(uid: 0, gid: 0)
                buffer.int32(owner.uid)
                buffer.int32(owner.gid)
                // rsync never maps id 0 by name.
                if owner.uid != 0, let user = owner.user { users[owner.uid] = user }
                if owner.gid != 0, let group = owner.group { groups[owner.gid] = group }
            }
        }
        buffer.byte(0)
        if owners {
            for names in [users, groups] {
                for (id, name) in names.sorted(by: { $0.key < $1.key }) {
                    let bytes = Array(name.utf8.prefix(255))
                    buffer.int32(id)
                    buffer.byte(UInt8(bytes.count))
                    buffer.append(bytes)
                }
                buffer.int32(0)
            }
        }
        buffer.int32(0)
    }
}
