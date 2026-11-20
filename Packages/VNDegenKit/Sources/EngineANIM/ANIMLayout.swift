import Foundation
import VNCore

nonisolated struct ANIMLayout: Equatable, Sendable {
    var root: String
    var base: String
    var evidence: String

    init?(_ probe: FolderProbe) {
        var best: ANIMLayout?
        for entry in probe.entries.sorted() {
            let parts = entry.split(separator: "/", omittingEmptySubsequences: false).map(String.init)
            guard parts.count <= 2, let file = parts.last, file.lowercased().hasSuffix("_sce.dat"), file.count > 8 else { continue }
            let base = String(file.dropLast(8))
            let folder = parts.dropLast().joined(separator: "/")
            let prefix = folder.isEmpty ? "" : folder.lowercased() + "/"
            let names = Set(probe.entries.filter { $0.lowercased().hasPrefix(prefix) }.map { String($0.lowercased().dropFirst(prefix.count)) })
            let stem = base.lowercased()
            guard names.contains("\(stem)_define.dat"), names.contains("\(stem)_system.dat"), names.contains("cg/psbpack.dat") else { continue }
            if let current = best, parts.count - 1 >= current.depth { continue }
            best = ANIMLayout(root: folder, base: base, evidence: entry)
        }
        guard let best else { return nil }
        self = best
    }

    private init(root: String, base: String, evidence: String) {
        self.root = root
        self.base = base
        self.evidence = evidence
    }

    private var depth: Int { root.isEmpty ? 0 : root.split(separator: "/").count }

    func url(in folder: URL) -> URL {
        root.isEmpty ? folder : folder.appendingPathComponent(root, isDirectory: true)
    }
}
