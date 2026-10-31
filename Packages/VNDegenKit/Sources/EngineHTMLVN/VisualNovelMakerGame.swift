import Foundation
import VNCore
import WebRuntime

/// Where a Visual Novel Maker game sits in the folder the user copied in, and which desktop runtime its PC build ships
/// in. Read from the folder once, for detection and for each launch.
nonisolated struct VisualNovelMakerGame: Equatable, Sendable {
    enum Packaging: Equatable, Sendable {
        /// NW.js, with index.html beside `Game.exe` or in package.nw/.
        case nwjs
        /// Electron, with the web root in resources/app/ or resources/app.asar.
        case electron
        /// A web export with no desktop runtime. Saves go to localStorage.
        case browser
    }

    /// The folder holding the game, its `Game.exe` on Windows, relative to the copied folder. Empty when the copied
    /// folder is the game's. The engine saves here, since the PC build's working directory is this folder.
    let root: String
    /// Where index.html lives, relative to `root`.
    let webRoot: String
    let packaging: Packaging

    static let engineFile = "data/ENGINE.js"
    /// Where exports put index.html, most common first.
    static let webRoots = ["", "resources/app", "resources/app.asar", "package.nw"]
    /// What a folder holds when it may be a game's, as the library's listing shows it.
    private static let rootMarkers: Set<String> = ["index.html", "package.json", "package.nw", "resources"]

    /// The path of the file that identified the game, relative to the copied folder.
    var evidence: String {
        [root, webRoot, Self.engineFile].filter { !$0.isEmpty }.joined(separator: "/")
    }
}

nonisolated extension VisualNovelMakerGame {
    /// The shallowest game in the folder, checking the copied folder and the folders up to two levels down that the
    /// library's listing shows holding a game's files. An archive import wraps a game in a folder named after the archive.
    init?(_ probe: FolderProbe) {
        let candidates = Set(probe.entries.compactMap { entry -> String? in
            let parts = entry.split(separator: "/")
            guard parts.count <= 3, Self.rootMarkers.contains(parts.last!.lowercased()) else { return nil }
            return parts.dropLast().joined(separator: "/")
        }).union([""])
        let roots = candidates.sorted { ($0.split(separator: "/").count, $0) < ($1.split(separator: "/").count, $1) }
        for root in roots {
            let folder = root.isEmpty ? probe.root : probe.root.appendingPathComponent(root, isDirectory: true)
            for webRoot in Self.webRoots {
                let web = GameFolder(root: folder, webRoot: webRoot)
                guard ["/index.html", "/" + Self.engineFile, "/data/SCRIPTS.js"].allSatisfy({ web.read(servedPath: $0, limit: 1) != nil })
                else { continue }
                let packaging: Packaging = webRoot.hasPrefix("resources/") ? .electron
                    : webRoot == "package.nw" || web.read(servedPath: "/package.json", limit: 1) != nil ? .nwjs : .browser
                self.init(root: root, webRoot: webRoot, packaging: packaging)
                return
            }
        }
        return nil
    }

    init?(folder: URL) {
        self.init(FolderProbe(scanning: folder))
    }

    /// The game's own folder on disk.
    func folder(in copied: URL) -> URL {
        root.isEmpty ? copied : copied.appendingPathComponent(root, isDirectory: true)
    }
}
