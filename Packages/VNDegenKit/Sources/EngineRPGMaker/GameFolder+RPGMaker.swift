import Foundation
import WebRuntime

extension GameFolder {
    /// Picks the web root the way `RPGMakerPlugin.detect` does: the top level when its js/ holds the engine core,
    /// otherwise www/.
    nonisolated init(rpgMakerGameAt root: URL) {
        let top = GameFolder(root: root, webRoot: "")
        let hasCore = { (dir: String) in
            ["rmmz_core.js", "rpg_core.js"].contains { top.resolve(virtualPath: "\(dir)/js/\($0)").map(Self.isFile) ?? false }
        }
        self = !hasCore(Self.virtualRoot) && hasCore("\(Self.virtualRoot)/www") ? GameFolder(root: root, webRoot: "www") : top
    }

    /// Where NW.js builds save: `save/` beside index.html.
    nonisolated var saveDirectory: URL? { resolve(virtualPath: "\(virtualWebRoot)/save") }
}
