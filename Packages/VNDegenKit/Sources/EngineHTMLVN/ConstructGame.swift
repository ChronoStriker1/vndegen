import Foundation
import VNCore
import WebRuntime

nonisolated struct ConstructGame: Equatable, Sendable {
    let root: String
    let webRoot: String

    static let runtimeFile = "scripts/c3runtime.js"
    static let webRoots = ["www", ""]
    private static let rootMarkers: Set<String> = ["www", "index.html", "data.json"]

    var evidence: String {
        [root, webRoot, Self.runtimeFile].filter { !$0.isEmpty }.joined(separator: "/")
    }
}

nonisolated extension ConstructGame {
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
                guard ["/index.html", "/data.json", "/" + Self.runtimeFile].allSatisfy({ web.read(servedPath: $0, limit: 1) != nil })
                else { continue }
                self.init(root: root, webRoot: webRoot)
                return
            }
        }
        return nil
    }

    init?(folder: URL) {
        self.init(FolderProbe(scanning: folder))
    }

    func folder(in copied: URL) -> URL {
        root.isEmpty ? copied : copied.appendingPathComponent(root, isDirectory: true)
    }
}
