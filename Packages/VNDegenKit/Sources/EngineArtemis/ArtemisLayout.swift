import Foundation
import VNCore

/// Where an Artemis game sits inside the folder the user copied in, and how its files are packed.
nonisolated public struct ArtemisLayout: Equatable, Sendable {
    /// The folder holding the game's archives or unpacked files, relative to the copied folder. Empty when it is the
    /// copied folder itself. Games often sit a folder or two down.
    public var root: String
    public var data: GameData

    public enum GameData: Equatable, Sendable {
        /// The base archive, such as `root.pfs`, with its patch volumes (`root.pfs.000`, ...) beside it. The engine
        /// mounts all of them.
        case archive(String, version: ArchiveVersion)
        /// Unpacked files, with `system.ini` in the root.
        case folder
    }

    /// The PFS format of the base archive. Games with `pf8` archives run a newer engine, whose saves are version 1002.
    public enum ArchiveVersion: String, Sendable {
        case pf2, pf6, pf8
    }

    public init(root: String, data: GameData) {
        self.root = root
        self.data = data
    }

    public func path(_ relative: String) -> String { root.isEmpty ? relative : "\(root)/\(relative)" }

    /// The file that identified the game.
    public var evidence: String {
        switch data {
        case let .archive(path, _): path
        case .folder: path("system.ini")
        }
    }

    /// The PC engine's save file version for this game: 1002 for `pf8` archives, else 1000.
    public var saveVersion: Int32 {
        if case .archive(_, .pf8) = data { 1002 } else { 1000 }
    }

    public init?(scanning folder: URL) {
        self.init(FolderProbe(scanning: folder))
    }

    /// The shallowest folder with a base archive wins, then the shallowest with an unpacked `system.ini`.
    public init?(_ probe: FolderProbe) {
        // Copies made on a Mac can hold AppleDouble files such as `._root.pfs` beside the real ones.
        let files = probe.entries.filter { !Self.name($0).hasPrefix("._") }
        let byDepth = { (a: String, b: String) in (a.split(separator: "/").count, a) < (b.split(separator: "/").count, b) }
        for archive in files.filter({ $0.lowercased().hasSuffix(".pfs") }).sorted(by: byDepth) {
            guard let magic = probe.read(archive, limit: 3), let text = String(data: magic, encoding: .ascii),
                  let version = ArchiveVersion(rawValue: text) else { continue }
            self.init(root: Self.parent(archive), data: .archive(archive, version: version))
            return
        }
        for ini in files.filter({ Self.name($0).lowercased() == "system.ini" }).sorted(by: byDepth) {
            guard let contents = probe.read(ini, limit: 64 << 10), Self.namesBoot(contents) else { continue }
            self.init(root: Self.parent(ini), data: .folder)
            return
        }
        return nil
    }

    /// Whether a `system.ini` is an Artemis one, which names the script the engine boots.
    static func namesBoot(_ contents: Data) -> Bool {
        let text = String(decoding: contents, as: UTF8.self)
        return text.split(whereSeparator: \.isNewline).contains { line in
            let setting = line.split(separator: "=", maxSplits: 1)
            return setting.count == 2 && setting[0].trimmingCharacters(in: .whitespaces).uppercased() == "BOOT"
        }
    }

    static func name(_ path: String) -> String {
        path.split(separator: "/").last.map(String.init) ?? path
    }

    static func parent(_ path: String) -> String {
        guard let slash = path.lastIndex(of: "/") else { return "" }
        return String(path[..<slash])
    }
}
