import Foundation
import VNCore

nonisolated struct SmileGameLayout: Equatable, Sendable {
    static let package = "data.sgbpack"
    static let player = "sgb_rpgplayer.exe"
    static let magic = Data("YUKARPKG".utf8)

    var root: String
    var evidence: String

    init?(_ probe: FolderProbe) {
        var best: SmileGameLayout?
        for entry in probe.entries.sorted() {
            let parts = entry.split(separator: "/", omittingEmptySubsequences: false).map(String.init)
            guard parts.count <= 3, parts.last?.lowercased() == Self.package else { continue }
            let folder = parts.dropLast().joined(separator: "/")
            if let current = best, parts.count - 1 >= current.depth { continue }
            let prefix = folder.isEmpty ? "" : folder + "/"
            guard probe.contains(prefix + Self.player), probe.read(entry, limit: Self.magic.count) == Self.magic else { continue }
            best = SmileGameLayout(root: folder, evidence: entry)
        }
        guard let best else { return nil }
        self = best
    }

    private init(root: String, evidence: String) {
        self.root = root
        self.evidence = evidence
    }

    private var depth: Int { root.isEmpty ? 0 : root.split(separator: "/").count }

    func url(in folder: URL) -> URL {
        root.isEmpty ? folder : folder.appendingPathComponent(root, isDirectory: true)
    }

    static func saveDirectory(in root: URL) -> URL {
        FileCase.resolve("savedata", in: root)
    }
}
