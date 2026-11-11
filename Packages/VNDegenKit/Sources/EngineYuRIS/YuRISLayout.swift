import Foundation
import VNCore

nonisolated struct YuRISLayout: Equatable, Sendable {
    var root: String
    var evidence: String

    init?(_ probe: FolderProbe) {
        var best: YuRISLayout?
        for entry in probe.entries {
            let parts = entry.split(separator: "/", omittingEmptySubsequences: false).map(String.init)
            guard parts.last?.lowercased() == "yscfg.dat", parts.count <= 3 else { continue }
            let folder = parts.dropLast().joined(separator: "/")
            let prefix = folder.isEmpty ? "" : folder.lowercased() + "/"
            let archive = probe.entries.sorted().first { other in
                let lower = other.lowercased()
                guard lower.hasSuffix(".ypf"), lower.hasPrefix(prefix) else { return false }
                let rest = lower.dropFirst(prefix.count)
                return !rest.contains("/") || (rest.hasPrefix("pac/") && !rest.dropFirst(4).contains("/"))
            }
            guard let archive else { continue }
            let candidate = YuRISLayout(root: folder, evidence: archive)
            if let current = best, candidate.root.count >= current.root.count { continue }
            best = candidate
        }
        guard let best else { return nil }
        self = best
    }

    private init(root: String, evidence: String) {
        self.root = root
        self.evidence = evidence
    }

    func url(in folder: URL) -> URL {
        root.isEmpty ? folder : folder.appendingPathComponent(root, isDirectory: true)
    }
}
