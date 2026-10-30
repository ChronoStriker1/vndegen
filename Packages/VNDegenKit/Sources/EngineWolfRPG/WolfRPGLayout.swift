import Foundation
import VNCore

/// Where a Wolf RPG Editor game's files are in the folder the player copied: the folder holding Game.exe (GamePro.exe
/// for the Pro editor) with its data beside it, at the top or up to two folders down.
nonisolated struct WolfRPGLayout: Equatable, Sendable {
    /// The folder holding the game, relative to the game folder, "" for the top.
    var root: String
    /// The file that identified the game, relative to the game folder.
    var evidence: String
    var confidence: DetectionConfidence

    init?(_ probe: FolderProbe) {
        var best: WolfRPGLayout?
        for entry in probe.entries {
            let lower = entry.lowercased()
            let parts = lower.split(separator: "/", omittingEmptySubsequences: false).map(String.init)
            // The game's data: Data.wolf, or the Data/BasicData folder, or Data/BasicData.wolf. A game two folders down
            // shows only as Game.exe beside Data and the MIDI player DLL every Wolf RPG Editor 2 game ships, since the
            // probe lists three levels.
            let folderDepth: Int
            if parts.last == "data.wolf" { folderDepth = parts.count - 1 }
            else if parts.suffix(2) == ["data", "basicdata"] { folderDepth = parts.count - 2 }
            else if parts.suffix(2) == ["data", "basicdata.wolf"] { folderDepth = parts.count - 2 }
            else if parts.last == "gurugurusmf4.dll" { folderDepth = parts.count - 1 }
            else { continue }
            guard folderDepth <= 2 else { continue }
            let folder = entry.split(separator: "/", omittingEmptySubsequences: false).prefix(folderDepth).joined(separator: "/")
            let prefix = folder.isEmpty ? "" : folder.lowercased() + "/"
            let hasExe = probe.contains(prefix + "game.exe") || probe.contains(prefix + "gamepro.exe")
            if parts.last == "gurugurusmf4.dll", !(hasExe && probe.contains(prefix + "data")) { continue }
            let candidate = WolfRPGLayout(root: folder, evidence: entry, confidence: hasExe ? .certain : .likely)
            if let current = best {
                if candidate.confidence < current.confidence { continue }
                if candidate.confidence == current.confidence, candidate.root.count >= current.root.count { continue }
            }
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

    /// The game's folder, the one holding Game.exe.
    func url(in folder: URL) -> URL {
        root.isEmpty ? folder : folder.appendingPathComponent(root, isDirectory: true)
    }
}
