import Foundation
import VNCore

nonisolated struct EAGLSLayout: Equatable, Sendable {
    var root: String
    var evidence: String

    init?(_ probe: FolderProbe) {
        var best: EAGLSLayout?
        for entry in probe.entries.sorted() {
            let parts = entry.split(separator: "/", omittingEmptySubsequences: false).map(String.init)
            guard parts.count >= 2, parts.count <= 3, parts.last?.lowercased() == "scpack.idx", parts[parts.count - 2].lowercased() == "script"
            else { continue }
            let folder = parts.dropLast(2).joined(separator: "/")
            let prefix = folder.isEmpty ? "" : folder.lowercased() + "/"
            guard probe.entries.contains(where: { $0.lowercased() == prefix + "cg/cgpack.idx" }),
                  probe.entries.contains(where: { $0.lowercased() == prefix + "script/scpack.pak" })
            else { continue }
            if let current = best, parts.count - 2 >= current.depth { continue }
            best = EAGLSLayout(root: folder, evidence: entry)
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
