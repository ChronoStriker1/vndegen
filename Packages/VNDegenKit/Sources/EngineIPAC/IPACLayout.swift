import Foundation
import VNCore

nonisolated struct IPACLayout: Equatable, Sendable {
    var root: String
    var evidence: String

    init?(_ probe: FolderProbe) {
        var best: IPACLayout?
        for entry in probe.entries.sorted() {
            let parts = entry.split(separator: "/", omittingEmptySubsequences: false).map(String.init)
            guard parts.count >= 2, parts.count <= 3, parts.last?.lowercased() == "scr.pak", parts[parts.count - 2].lowercased() == "data"
            else { continue }
            let data = parts.dropLast().joined(separator: "/").lowercased()
            guard probe.entries.contains(where: { $0.lowercased() == data + "/parts.pak" }) else { continue }
            let folder = parts.dropLast(2).joined(separator: "/")
            if let current = best, parts.count - 2 >= current.depth { continue }
            best = IPACLayout(root: folder, evidence: entry)
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
}
