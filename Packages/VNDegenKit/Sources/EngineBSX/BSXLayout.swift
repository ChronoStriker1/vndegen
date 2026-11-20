import Foundation
import VNCore

nonisolated struct BSXLayout: Equatable, Sendable {
    var root: String
    var evidence: String

    init?(_ probe: FolderProbe) {
        var best: BSXLayout?
        for entry in probe.entries.sorted() {
            let parts = entry.split(separator: "/", omittingEmptySubsequences: false).map(String.init)
            guard parts.count <= 2, parts.last?.lowercased() == "bsxx.dat" else { continue }
            let folder = parts.dropLast().joined(separator: "/")
            let prefix = folder.isEmpty ? "" : folder.lowercased() + "/"
            let names = probe.entries.filter { $0.lowercased().hasPrefix(prefix) }.map { $0.lowercased().dropFirst(prefix.count) }
            guard names.contains("bsxx.cfg"), names.contains(where: { $0.hasPrefix("graphics") && $0.hasSuffix(".bsa") && !$0.contains("/") })
            else { continue }
            if let current = best, parts.count - 1 >= current.depth { continue }
            best = BSXLayout(root: folder, evidence: entry)
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
