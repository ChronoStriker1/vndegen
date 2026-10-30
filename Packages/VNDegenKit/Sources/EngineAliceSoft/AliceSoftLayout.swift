import Foundation
import VNCore

/// Which AliceSoft engine a folder holds, and where: at the top or up to two folders down.
nonisolated struct AliceSoftLayout: Equatable, Sendable {
    enum System: String, Sendable {
        /// The Windows 3.1 SYSTEM3.EXE games: scenario in ADISK.DAT.
        case system30
        /// System 3.5 to 3.9 (System39.exe): System39.ain and link archives such as RanceSA.ALD.
        case system39
        /// System 4: a compiled .ain beside System40.ini, AliceStart.ini or System40.exe.
        case system4
    }

    var system: System
    /// The folder holding the game, relative to the game folder, "" for the top.
    var root: String
    /// The file that identified the game, relative to the game folder.
    var evidence: String
    var confidence: DetectionConfidence

    init?(_ probe: FolderProbe) {
        var best: AliceSoftLayout?
        for entry in probe.entries {
            let parts = entry.split(separator: "/", omittingEmptySubsequences: false).map(String.init)
            guard let name = parts.last?.lowercased(), parts.count <= 3 else { continue }
            let folder = parts.dropLast().joined(separator: "/")
            let prefix = folder.isEmpty ? "" : folder.lowercased() + "/"
            // Names in the same folder, lower case. Built only for the few names that can identify a game.
            var siblings: [String] {
                probe.entries.filter { $0.lowercased().hasPrefix(prefix) && !$0.dropFirst(prefix.count).contains("/") }
                    .map { String($0.dropFirst(prefix.count)).lowercased() }
            }
            let candidate: AliceSoftLayout?
            if name == "system39.ain" {
                let hasScenario = siblings.contains { $0.hasSuffix("sa.ald") }
                candidate = hasScenario ? AliceSoftLayout(system: .system39, root: folder, evidence: entry, confidence: .certain) : nil
            } else if name == "adisk.dat" {
                let hasExe = probe.contains(prefix + "system3.exe")
                candidate = AliceSoftLayout(system: .system30, root: folder, evidence: entry, confidence: hasExe ? .certain : .likely)
            } else if name.hasSuffix(".ain") {
                let markers = ["system40.ini", "alicestart.ini", "system40.exe"]
                candidate = markers.contains(where: { probe.contains(prefix + $0) })
                    ? AliceSoftLayout(system: .system4, root: folder, evidence: entry, confidence: .certain) : nil
            } else {
                candidate = nil
            }
            guard let candidate else { continue }
            if let current = best {
                if candidate.confidence < current.confidence { continue }
                if candidate.confidence == current.confidence, candidate.root.count >= current.root.count { continue }
            }
            best = candidate
        }
        guard let best else { return nil }
        self = best
    }

    private init(system: System, root: String, evidence: String, confidence: DetectionConfidence) {
        self.system = system
        self.root = root
        self.evidence = evidence
        self.confidence = confidence
    }

    /// The folder holding the game's files.
    func url(in folder: URL) -> URL {
        root.isEmpty ? folder : folder.appendingPathComponent(root, isDirectory: true)
    }
}
