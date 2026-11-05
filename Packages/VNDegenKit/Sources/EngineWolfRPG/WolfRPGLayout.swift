import Foundation
import VNCore

/// Where a Wolf RPG Editor game's files are in the folder the player copied: the folder holding Game.exe (GamePro.exe
/// for the Pro editor) with its data beside it, at the top or up to three folders down.
nonisolated struct WolfRPGLayout: Equatable, Sendable {
    /// The folder holding the game, relative to the game folder, "" for the top.
    var root: String
    /// The file that identified the game, relative to the game folder.
    var evidence: String
    var confidence: DetectionConfidence

    /// Lists `folder` and finds its game, looking a level deeper when it needs to.
    init?(scanning folder: URL) {
        self.init(deepening: FolderProbe(scanning: folder))
    }

    /// `probe`'s game, or one three folders down, past the library's listing, such as Succubus Prison's
    /// `Succubus Prison/Succ Prison/sys` extracted with its outer folder kept. A wrapper holds little besides the game, so
    /// its listing is short. A long one is another engine's game, which shouldn't pay for this at each refresh. Only a
    /// folder holding Game.exe adds its names, so a short listing over a large game of another engine stays short.
    init?(deepening probe: FolderProbe) {
        if let layout = WolfRPGLayout(probe) {
            self = layout
            return
        }
        guard probe.entries.count <= 200 else { return nil }
        var entries = probe.entries
        for folder in probe.entries where folder.split(separator: "/").count == 3 {
            let path = probe.root.appendingPathComponent(folder).path
            let names = (try? FileManager.default.contentsOfDirectory(atPath: path)) ?? []
            guard names.contains(where: { ["game.exe", "gamepro.exe"].contains($0.lowercased()) }) else { continue }
            entries.formUnion(names.filter { !$0.hasPrefix(".") }.map { "\(folder)/\($0)" })
        }
        guard entries.count > probe.entries.count else { return nil }
        self.init(FolderProbe(root: probe.root, entries: entries))
    }

    init?(_ probe: FolderProbe) {
        var best: WolfRPGLayout?
        // Sorted, because a tie keeps the first found and a Set's order changes per launch.
        for entry in probe.entries.sorted() {
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
            guard folderDepth <= 3 else { continue }
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
