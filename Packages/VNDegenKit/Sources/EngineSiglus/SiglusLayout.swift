import Foundation
import VNCore

/// Where a SiglusEngine or RealLive game sits inside the folder the player copied in, and which engine it is.
nonisolated public struct SiglusLayout: Equatable, Sendable {
    public enum Engine: Sendable {
        /// `Scene.pck` and `Gameexe.dat`.
        case siglus
        /// `Gameexe.ini` and a `SEEN.TXT` scenario archive.
        case realLive
    }

    public var engine: Engine
    /// The game's folder relative to the copied folder, empty when it is the copied folder itself.
    public var root: String
    /// The file that identified the game.
    public var evidence: String

    public init(engine: Engine, root: String, evidence: String) {
        self.engine = engine
        self.root = root
        self.evidence = evidence
    }

    public func url(in folder: URL) -> URL {
        root.isEmpty ? folder : folder.appending(path: root, directoryHint: .isDirectory)
    }

    public init?(scanning folder: URL) {
        self.init(FolderProbe(scanning: folder))
    }

    /// The game's folder is the copied folder or one folder down. A SiglusEngine game has `Scene.pck` or
    /// `Gameexe.dat` there. A RealLive game has `Gameexe.ini` and its `SEEN.TXT`, there or in a folder below it such as
    /// `DAT`; AVG32, RealLive's predecessor, uses the same names, and its `SEEN.TXT` starts with `PACL`. The shallowest
    /// folder wins, and SiglusEngine wins in a folder with both, since some SiglusEngine games ship a `Gameexe.ini` too.
    public init?(_ probe: FolderProbe) {
        // Copies made on a Mac can hold AppleDouble files such as `._Scene.pck` beside the real ones.
        let files = probe.entries.filter { !($0.split(separator: "/").last ?? "").hasPrefix("._") }
            .sorted { ($0.depth, $0) < ($1.depth, $1) }
        let siglus = files.first { $0.depth <= 1 && ["scene.pck", "gameexe.dat"].contains($0.name.lowercased()) }
        let realLive = files.lazy.filter { $0.depth <= 1 && $0.name.lowercased() == "gameexe.ini" }.compactMap { ini -> (String, String)? in
            let folder = ini.folder
            let siglusThere = files.contains { $0.folder == folder && ["scene.pck", "gameexe.dat"].contains($0.name.lowercased()) }
            guard !siglusThere, let seen = Self.seen(in: folder, files: files, probe: probe) else { return nil }
            return (folder, seen)
        }.first
        switch (siglus, realLive) {
        case let (siglus?, realLive?) where realLive.0.depth < siglus.folder.depth:
            self.init(engine: .realLive, root: realLive.0, evidence: realLive.1)
        case let (siglus?, _):
            self.init(engine: .siglus, root: siglus.folder, evidence: siglus)
        case let (nil, realLive?):
            self.init(engine: .realLive, root: realLive.0, evidence: realLive.1)
        case (nil, nil):
            return nil
        }
    }

    /// The RealLive `SEEN.TXT` in `folder` or a folder directly below it.
    private static func seen(in folder: String, files: [String], probe: FolderProbe) -> String? {
        files.first { file in
            guard file.name.lowercased() == "seen.txt" else { return false }
            let parent = file.folder
            guard parent == folder || !parent.isEmpty && parent.folder == folder else { return false }
            return probe.read(file, limit: 4).map { $0 != Data("PACL".utf8) } ?? false
        }
    }
}

nonisolated private extension String {
    /// Folders above the path, 0 for a file in the copied folder and -1 for the copied folder itself, "".
    var depth: Int { split(separator: "/").count - 1 }
    var name: String { split(separator: "/").last.map(String.init) ?? self }
    /// The folder holding the file, empty for the copied folder.
    var folder: String {
        guard let slash = lastIndex(of: "/") else { return "" }
        return String(self[..<slash])
    }
}
