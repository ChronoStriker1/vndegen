import Foundation
import VNCore

/// Where a KiriKiri game sits inside the folder the user copied in, and where its project data is.
nonisolated public struct KiriKiriLayout: Equatable, Sendable {
    /// The game's root, the folder holding its `.exe`, relative to the copied folder. Empty when the copied folder is
    /// the root. Games often sit one or two folders down.
    public var root: String
    public var data: ProjectData

    /// The project the engine starts. Paths are relative to the copied folder, in their on-disk case.
    public enum ProjectData: Equatable, Sendable {
        /// `data.xp3` in the root.
        case archive(String)
        /// An unpacked `data/` folder in the root, holding `startup.tjs`.
        case folder(String)
        /// The project archive appended to the game's `.exe`, with other `.xp3` archives beside it.
        case embedded(String)
    }

    public init(root: String, data: ProjectData) {
        self.root = root
        self.data = data
    }

    public func path(_ relative: String) -> String { root.isEmpty ? relative : "\(root)/\(relative)" }

    /// The file that identified the game.
    public var evidence: String {
        switch data {
        case let .archive(path), let .embedded(path): path
        case let .folder(path): "\(path)/startup.tjs"
        }
    }

    /// Lists `folder` and finds its game, looking deeper when it needs to.
    public init?(scanning folder: URL) {
        self.init(deepening: FolderProbe(scanning: folder))
    }

    /// `probe`'s game, also where the library's three-level listing ends inside it. A `data` folder at the listing's last
    /// level adds its `startup.tjs`, for an unpacked game two folders down, `outer/inner/data/startup.tjs`. A short
    /// listing also has each folder at its last level looked into, for a game three folders down, such as Violated Hero
    /// IV's `Violated Hero 4/127GRJ132/vh4`. Only a folder holding `data.xp3`, an `.exe` or a `data` folder adds its
    /// names. A wrapper holds little besides the game, so its listing is short. A long one is another engine's game,
    /// which shouldn't pay for this at each refresh.
    init?(deepening probe: FolderProbe) {
        let fm = FileManager.default
        func startup(in data: String) -> String? {
            let path = FileCase.spelling(of: "\(data)/startup.tjs", in: probe.root)
            return fm.fileExists(atPath: probe.root.appendingPathComponent(path).path) ? path : nil
        }
        var entries = probe.entries
        for folder in probe.entries.sorted() where folder.split(separator: "/").count == 3 {
            if Self.name(folder).lowercased() == "data" {
                if let script = startup(in: folder) { entries.insert(script) }
                continue
            }
            guard probe.entries.count <= 200,
                  let names = try? fm.contentsOfDirectory(atPath: probe.root.appendingPathComponent(folder).path),
                  names.contains(where: { ["data.xp3", "data"].contains($0.lowercased()) || $0.lowercased().hasSuffix(".exe") })
            else { continue }
            for name in names where !name.hasPrefix(".") {
                entries.insert("\(folder)/\(name)")
                if name.lowercased() == "data", let script = startup(in: "\(folder)/\(name)") { entries.insert(script) }
            }
        }
        self.init(FolderProbe(root: probe.root, entries: entries))
    }

    /// The shallowest root wins. Within a root, `data.xp3` wins over `data/`, which wins over an embedded archive.
    public init?(_ probe: FolderProbe) {
        // Copies made on a Mac can hold AppleDouble files such as `._data.xp3` beside the real ones.
        let files = probe.entries.filter { !Self.name($0).hasPrefix("._") }
        let roots = Set(files.compactMap { entry -> String? in
            let lower = entry.lowercased()
            if Self.name(lower) == "data.xp3" || lower.hasSuffix(".exe") { return Self.parent(entry) }
            if lower == "data/startup.tjs" || lower.hasSuffix("/data/startup.tjs") { return Self.parent(Self.parent(entry)) }
            return nil
        })
        let ordered = roots.sorted { ($0.split(separator: "/").count, $0) < ($1.split(separator: "/").count, $1) }
        for root in ordered {
            if let data = Self.projectData(root: root, files: files, probe: probe) {
                self.init(root: root, data: data)
                return
            }
        }
        return nil
    }

    private static func projectData(root: String, files: Set<String>, probe: FolderProbe) -> ProjectData? {
        let children = files.filter { parent($0) == root }.sorted()
        if let archive = children.first(where: { name($0).lowercased() == "data.xp3" }) { return .archive(archive) }
        let startup = (root.isEmpty ? "data/startup.tjs" : "\(root)/data/startup.tjs").lowercased()
        if let script = files.first(where: { $0.lowercased() == startup }) { return .folder(parent(script)) }

        let executables = children.filter { $0.lowercased().hasSuffix(".exe") }
        guard !executables.isEmpty,
              children.contains(where: { $0.lowercased().hasSuffix(".xp3") && probe.read($0, limit: xp3Magic.count) == xp3Magic })
        else { return nil }
        // The executable that carries the project archive is far larger than a config tool or an uninstaller.
        let size = { (path: String) in
            (try? FileManager.default.attributesOfItem(atPath: probe.root.appendingPathComponent(path).path)[.size] as? Int) ?? 0
        }
        return executables.max { size($0) < size($1) }.map(ProjectData.embedded)
    }

    /// Every XP3 archive starts with these bytes.
    static let xp3Magic = Data([0x58, 0x50, 0x33, 0x0D, 0x0A, 0x20, 0x0A, 0x1A, 0x8B, 0x67, 0x01])

    private static func name(_ path: String) -> String {
        path.split(separator: "/").last.map(String.init) ?? path
    }

    private static func parent(_ path: String) -> String {
        guard let slash = path.lastIndex(of: "/") else { return "" }
        return String(path[..<slash])
    }
}
