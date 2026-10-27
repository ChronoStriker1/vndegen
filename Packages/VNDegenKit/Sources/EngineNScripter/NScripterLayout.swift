import Foundation
import VNCore

/// Where an NScripter game's files are in the folder the player copied: the folder holding its script, at the top or
/// up to two folders down.
nonisolated struct NScripterLayout: Equatable, Sendable {
    /// The folder holding the script, relative to the game folder, "" for the top.
    var root: String
    /// The file that identified the game, relative to the game folder.
    var evidence: String
    var confidence: DetectionConfidence

    init?(_ probe: FolderProbe) {
        // Group the listing by folder, with lowercased names, as Windows compares them.
        var folders: [String: Set<String>] = [:]
        for entry in probe.entries {
            let parts = entry.split(separator: "/", omittingEmptySubsequences: false)
            guard parts.count <= 3 else { continue }
            let folder = parts.dropLast().joined(separator: "/")
            folders[folder, default: []].insert(parts.last!.lowercased())
        }
        var best: NScripterLayout?
        for (folder, names) in folders {
            guard let found = Self.match(names) else { continue }
            let candidate = NScripterLayout(root: folder, evidence: folder.isEmpty ? found.file : "\(folder)/\(found.file)",
                                            confidence: found.confidence)
            // The most confident match wins, then the shallowest folder.
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

    /// The script file and how sure it makes us. An encoded nscript.dat is NScripter's own format. A numbered text
    /// script counts only beside an archive or the engine's settings file, since 0.txt alone could be anything.
    private static func match(_ names: Set<String>) -> (file: String, confidence: DetectionConfidence)? {
        let hasArchive = names.contains { $0.hasSuffix(".nsa") || $0.hasSuffix(".ns2") || $0.hasSuffix(".sar") }
        let companion = hasArchive || names.contains("envdata") || names.contains("nscr.exe") || names.contains("nslua.dll")
            || names.contains("nsogg2.dll") || names.contains("nspng.dll")
        for script in ["0.txt", "00.txt"] where names.contains(script) && companion {
            return (script, .certain)
        }
        if names.contains("nscript.dat") { return ("nscript.dat", .certain) }
        return nil
    }

    /// The game's script folder.
    func url(in folder: URL) -> URL {
        root.isEmpty ? folder : folder.appendingPathComponent(root, isDirectory: true)
    }
}
