import Foundation
import VNCore

nonisolated struct YuRISLayout: Equatable, Sendable {
    var root: String
    var evidence: String

    init?(_ probe: FolderProbe) {
        let entries = probe.entries.sorted()
        var best: YuRISLayout?
        for entry in entries {
            let parts = entry.split(separator: "/", omittingEmptySubsequences: false).map(String.init)
            guard let name = parts.last?.lowercased(), parts.count <= 4 else { continue }
            var folderParts = Array(parts.dropLast())
            let candidate: YuRISLayout?
            switch name {
            case "yscfg.dat" where parts.count <= 3:
                let folder = folderParts.joined(separator: "/")
                candidate = Self.firstArchive(in: folder, entries).map { YuRISLayout(root: folder, evidence: $0) }
            case "ysbin.ypf":
                if folderParts.last?.lowercased() == "pac" { folderParts.removeLast() }
                guard folderParts.count <= 2 else { continue }
                candidate = YuRISLayout(root: folderParts.joined(separator: "/"), evidence: entry)
            default:
                continue
            }
            guard let candidate else { continue }
            if let current = best, candidate.root.count >= current.root.count { continue }
            best = candidate
        }
        guard let best else { return nil }
        self = best
    }

    private static func firstArchive(in folder: String, _ entries: [String]) -> String? {
        let prefix = folder.isEmpty ? "" : folder.lowercased() + "/"
        return entries.first { other in
            let lower = other.lowercased()
            guard lower.hasSuffix(".ypf"), lower.hasPrefix(prefix) else { return false }
            let rest = lower.dropFirst(prefix.count)
            return !rest.contains("/") || (rest.hasPrefix("pac/") && !rest.dropFirst(4).contains("/"))
        }
    }

    private init(root: String, evidence: String) {
        self.root = root
        self.evidence = evidence
    }

    func url(in folder: URL) -> URL {
        root.isEmpty ? folder : folder.appendingPathComponent(root, isDirectory: true)
    }
}
