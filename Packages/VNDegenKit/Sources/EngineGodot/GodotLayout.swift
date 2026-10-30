import Foundation
import VNCore

/// Where a Godot game's pack and executable are inside the folder the user copied in. Paths are relative to that
/// folder, in their on-disk case.
nonisolated public struct GodotLayout: Equatable, Sendable {
    /// The pack: a `.pck`, or the `.exe` that holds it.
    public var pack: String
    /// The game's Windows executable. Godot games read files beside it through `OS.get_executable_path()`.
    public var executable: String
    /// The pack's header and file list.
    var header: GodotPack
    /// The game is a Godot C# game, which needs .NET: a `data_*` folder beside its `.exe` holds `GodotSharp.dll`, or in
    /// Godot 3 a `Mono` folder.
    var usesDotNet = false

    public var evidence: String { pack }
    public var version: String { header.version }
    public var major: Int { header.major }

    /// The shallowest game wins. A `.pck` wins over an executable with a pack inside it, and a `.pck` named after an
    /// executable beside it wins over one that isn't.
    public init?(_ probe: FolderProbe) {
        // Copies made on a Mac can hold AppleDouble files such as `._game.pck` beside the real ones.
        let files = probe.entries.filter { !Self.name($0).hasPrefix("._") }
        guard let found = Self.find(probe, files: files) else { return nil }
        self = found
        let root = Self.parent(executable).lowercased()
        usesDotNet = files.contains { path in
            let lower = path.lowercased()
            guard ["godotsharp.dll", "godotsharp", "mono"].contains(Self.name(lower)) else { return false }
            let data = Self.parent(lower)
            return Self.name(data).hasPrefix("data_") && Self.parent(data) == root
        }
    }

    private static func find(_ probe: FolderProbe, files: Set<String>) -> GodotLayout? {
        let packs = files.filter { $0.lowercased().hasSuffix(".pck") }
        let executables = files.filter { $0.lowercased().hasSuffix(".exe") }
        let depth = { (path: String) in path.split(separator: "/").count }

        for pck in packs.sorted(by: { (depth($0), $0) < (depth($1), $1) }) {
            let folder = Self.parent(pck)
            let base = Self.name(pck).dropLast(4).lowercased()
            let siblings = executables.filter { Self.parent($0) == folder }.sorted()
            let executable = siblings.first { Self.name($0).dropLast(4).lowercased() == base }
                ?? siblings.first { !$0.lowercased().contains("loader") && !$0.lowercased().contains("console") }
                ?? (folder.isEmpty ? "" : "\(folder)/") + Self.name(pck).dropLast(4) + ".exe"
            guard let header = GodotPack(contentsOf: probe.root.appendingPathComponent(pck),
                                         executable: probe.root.appendingPathComponent(executable)) else { continue }
            return GodotLayout(pack: pck, executable: executable, header: header)
        }
        // Executables carrying their pack. Godot's console wrapper beside them holds none.
        for exe in executables.sorted(by: { (depth($0), $0) < (depth($1), $1) }) {
            let url = probe.root.appendingPathComponent(exe)
            guard let header = GodotPack(contentsOf: url, executable: url) else { continue }
            return GodotLayout(pack: exe, executable: exe, header: header)
        }
        return nil
    }

    init(pack: String, executable: String, header: GodotPack) {
        self.pack = pack
        self.executable = executable
        self.header = header
    }

    static func name(_ path: String) -> String {
        path.split(separator: "/").last.map(String.init) ?? path
    }

    static func parent(_ path: String) -> String {
        guard let slash = path.lastIndex(of: "/") else { return "" }
        return String(path[..<slash])
    }
}
