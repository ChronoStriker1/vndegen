import Foundation
import VNCore

/// Where a NekoSDK game's files are in the folder the player copied: the folder holding `cfg.pak` and `scr.pak`, at the
/// top or up to two folders down. Every NekoSDK game in the library has both, and no other engine VNDegen knows uses them.
nonisolated struct NekoSDKLayout: Equatable, Sendable {
    /// The folder holding the archives, relative to the game folder, "" for the top.
    var root: String
    /// The file that identified the game, relative to the game folder.
    var evidence: String
    var confidence: DetectionConfidence

    init?(_ probe: FolderProbe) {
        var best: NekoSDKLayout?
        for entry in probe.entries {
            let parts = entry.split(separator: "/", omittingEmptySubsequences: false).map(String.init)
            guard parts.last?.lowercased() == "cfg.pak", parts.count <= 3 else { continue }
            let folder = parts.dropLast().joined(separator: "/")
            let prefix = folder.isEmpty ? "" : folder.lowercased() + "/"
            guard probe.contains(prefix + "scr.pak") else { continue }
            let candidate = NekoSDKLayout(root: folder, evidence: entry, confidence: .certain)
            if let current = best, candidate.root.count >= current.root.count { continue }
            best = candidate
        }
        guard let best else { return nil }
        self = best
    }

    private init(root: String, evidence: String, confidence: DetectionConfidence) {
        self.root = root
        self.evidence = evidence
        self.confidence = confidence
    }

    /// The game's folder, the one holding the archives.
    func url(in folder: URL) -> URL {
        root.isEmpty ? folder : folder.appendingPathComponent(root, isDirectory: true)
    }
}
