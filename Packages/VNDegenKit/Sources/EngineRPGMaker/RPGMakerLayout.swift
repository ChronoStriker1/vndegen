import Foundation
import VNCore

/// Where an RPG Maker MV or MZ game is in the folder the player copied: the folder holding `js/` or `www/js/` with the
/// engine core, at the top or inside wrapper folders. An archive with a readme or a launcher beside the game keeps the
/// game's folder when it's imported, and one with two folders above the game keeps the lower one.
nonisolated struct RPGMakerLayout: Equatable, Sendable {
    /// The game's folder, relative to the game folder, "" for the top.
    var root: String
    /// The engine core, relative to the game folder.
    var evidence: String

    /// The game holding one of `cores`, shallowest first, so a game wins over a copy of the engine in a folder below it.
    init?(_ probe: FolderProbe, cores: [String] = ["rmmz_core.js", "rpg_core.js"]) {
        guard let core = (Self.listed(probe, cores) + Self.deeper(probe, cores)).min(by: Self.shallower) else { return nil }
        let parts = core.split(separator: "/").dropLast(2)
        root = (parts.last?.lowercased() == "www" ? parts.dropLast() : parts).joined(separator: "/")
        evidence = core
    }

    /// Cores the listing shows, in a `js` folder up to two folders down.
    private static func listed(_ probe: FolderProbe, _ cores: [String]) -> [String] {
        probe.entries.filter { entry in
            let parts = entry.lowercased().split(separator: "/")
            return parts.count >= 2 && parts[parts.count - 2] == "js" && cores.contains(String(parts[parts.count - 1]))
        }
    }

    /// Cores past the listing's three levels. Every `js` and `www` folder it reaches gets a look. So does every other
    /// folder at its last level when the listing is short, as a wrapper holds little besides the game. A long one is
    /// another engine's game, which shouldn't pay for this at each refresh.
    private static func deeper(_ probe: FolderProbe, _ cores: [String]) -> [String] {
        let deepest = probe.entries.filter { $0.split(separator: "/").count == 3 }
        var found: [String] = []
        for folder in deepest {
            let name = folder.split(separator: "/").last?.lowercased()
            guard name == "js" || name == "www" || probe.entries.count <= 200 else { continue }
            for core in cores {
                for tail in name == "js" ? [core] : ["js/\(core)", "www/js/\(core)"] {
                    let path = FileCase.spelling(of: "\(folder)/\(tail)", in: probe.root)
                    if FileManager.default.fileExists(atPath: probe.root.appendingPathComponent(path).path) { found.append(path) }
                }
            }
        }
        return found
    }

    /// Sorted by name after depth, because a tie keeps the first found and a Set's order changes per launch.
    private static func shallower(_ a: String, _ b: String) -> Bool {
        let depth = { (path: String) in path.split(separator: "/").count }
        return (depth(a), a) < (depth(b), b)
    }
}
