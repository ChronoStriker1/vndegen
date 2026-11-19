import Foundation
import VNCore

nonisolated struct ExHibitLayout: Equatable, Sendable {
    var root: String
    var evidence: String

    init?(_ probe: FolderProbe) {
        var best: ExHibitLayout?
        let lower = Set(probe.entries.map { $0.lowercased() })
        for entry in probe.entries.sorted() {
            let parts = entry.split(separator: "/", omittingEmptySubsequences: false).map(String.init)
            guard parts.count <= 2, parts.last?.lowercased() == "exhibit.ini" else { continue }
            let folder = parts.dropLast().joined(separator: "/")
            let prefix = folder.isEmpty ? "" : folder.lowercased() + "/"
            guard lower.contains(prefix + "resident.dll"), lower.contains(prefix + "rld/start.rld") else { continue }
            if let current = best, parts.count - 1 >= current.depth { continue }
            best = ExHibitLayout(root: folder, evidence: entry)
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
