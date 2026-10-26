import CryptoKit
import Foundation
import VNCore

/// A bundled Ren'Py runtime. A runtime only runs the `renpy/` package of its own release, so VNDegen ignores the game's
/// copy and runs the game's `game/` folder on the newest release of its Python line, as Ren'Py's own SDK launcher does.
nonisolated public enum RenPyRuntime: String, Codable, CaseIterable, Sendable {
    /// Ren'Py 8.5.3, on Python 3.
    case renPy8 = "8"
    /// Ren'Py 7.8.7, on Python 2. Also runs Ren'Py 6.99 games.
    case renPy7 = "7"

    public var displayName: String { "Ren'Py \(rawValue)" }

    /// The runtime's macOS binaries, inside its `lib/` folder.
    var macFolder: String { self == .renPy8 ? "py3-mac-universal" : "py2-mac-universal" }
}

/// Where a Ren'Py game sits inside the folder the user copied in, its version, and the runtime it runs on.
nonisolated public struct RenPyLayout: Equatable, Sendable {
    /// The Ren'Py base directory (the one holding `renpy/` and `game/`), relative to the game folder. Empty when the
    /// folder is the base directory. Archives often unpack to one or two extra folder levels, such as
    /// `Game/Game-1.0-pc/`, so those are checked too.
    public var base: String
    /// Such as "8.5.3", read from the game's `renpy/` package.
    public var version: String?
    public var runtime: RenPyRuntime
    /// What decided `runtime`.
    public var evidence: RuntimeEvidence

    public enum RuntimeEvidence: String, Sendable {
        /// The version in `renpy/vc_version.py` or `renpy/__init__.py`.
        case version
        /// The `lib/` folder names, such as `py3-linux-x86_64`, `py2-windows-i686`, or `python2.7`.
        case libraries
        /// The engine's compiled Python files: `.pyo` for Python 2, `__pycache__` for Python 3.
        case compiledFiles
        /// Nothing told the lines apart. Ren'Py 8 is the default.
        case fallback
    }

    public init(base: String, version: String?, runtime: RenPyRuntime, evidence: RuntimeEvidence) {
        self.base = base
        self.version = version
        self.runtime = runtime
        self.evidence = evidence
    }

    public func path(_ relative: String) -> String { base.isEmpty ? relative : "\(base)/\(relative)" }

    public init?(_ probe: FolderProbe) { self.init(probe, deepScanLimit: 20_000) }

    /// `deepScanLimit` caps the entries the `game/` walks of bases two folders down visit, all of them together.
    init?(_ probe: FolderProbe, deepScanLimit: Int) {
        var deepBudget = deepScanLimit
        guard let base = Self.candidates(probe).first(where: { Self.isBase($0, probe, deepBudget: &deepBudget) }) else { return nil }
        let prefix = base.isEmpty ? "" : "\(base)/"
        // The probe stops above the contents of `renpy/` and `lib/` in a base two folders down. The folder's name, in its
        // on-disk case, comes from the probe, and what is inside is read from disk.
        let folder = { (name: String) in probe.entries.first { $0.lowercased() == (prefix + name).lowercased() } ?? prefix + name }
        let text = { (name: String, file: String) in probe.read("\(folder(name))/\(file)").map { String(decoding: $0, as: UTF8.self) } }
        let children = { (name: String) in
            let names = (try? FileManager.default.contentsOfDirectory(atPath: probe.root.appendingPathComponent(folder(name)).path)) ?? []
            return names.map { $0.lowercased() }
        }

        let vcVersion = text("renpy", "vc_version.py").flatMap(Self.vcVersion)
        let tuples = vcVersion.map { [$0] } ?? Self.versionTuples(text("renpy", "__init__.py"))
        let runtime: RenPyRuntime, evidence: RuntimeEvidence
        if let line = Self.runtime(versions: tuples) {
            (runtime, evidence) = (line, .version)
        } else if let line = Self.runtime(libraries: children("lib")) {
            (runtime, evidence) = (line, .libraries)
        } else if let line = Self.runtime(engineFiles: children("renpy")) {
            (runtime, evidence) = (line, .compiledFiles)
        } else {
            (runtime, evidence) = (.renPy8, .fallback)
        }
        // Ren'Py 7.5 to 8.1 ship one `renpy/` for both lines, with a 7.x and an 8.x version tuple.
        let version = tuples.first { ($0[0] >= 8) == (runtime == .renPy8) } ?? tuples.first
        self.init(base: base, version: version?.map(String.init).joined(separator: "."), runtime: runtime, evidence: evidence)
    }

    /// Folders that may be the base directory, shallowest first and then by name: the game folder and any folder up to
    /// two levels down that holds a `renpy` entry. The probe already lists those levels, so this reads nothing from
    /// disk. Each candidate can cost a walk of its `game/` folder, so only the first `limit` are tried.
    static func candidates(_ probe: FolderProbe, limit: Int = 16) -> [String] {
        let bases = probe.entries.compactMap { entry -> String? in
            let parts = entry.split(separator: "/", omittingEmptySubsequences: false)
            guard parts.count <= 3, parts.last?.lowercased() == "renpy" else { return nil }
            return parts.dropLast().joined(separator: "/")
        }
        let depth = { (base: String) in base.count { $0 == "/" } }
        return Array(Set(bases).sorted { (depth($0), $0) < (depth($1), $1) }.prefix(limit))
    }

    /// The probe lists the scripts directly in `game/` for a base up to one folder down, so its walk looks only for
    /// deeper scripts and has no limit. The probe shows nothing inside `game/` for a base two folders down, so those
    /// walks share `deepBudget`, and a large `game/` without scripts can't stall detection.
    private static func isBase(_ base: String, _ probe: FolderProbe, deepBudget: inout Int) -> Bool {
        let prefix = base.isEmpty ? "" : "\(base)/".lowercased()
        guard probe.contains("\(prefix)renpy"), let game = probe.entries.first(where: { $0.lowercased() == "\(prefix)game" })
        else { return false }
        if probe.entries.contains(where: { $0.lowercased().hasPrefix("\(prefix)game/") && isScript($0) }) { return true }
        let url = probe.root.appendingPathComponent(game, isDirectory: true)
        return base.contains("/") ? containsScript(url, budget: &deepBudget) : containsScript(url)
    }

    private static func isScript(_ path: String) -> Bool {
        [".rpy", ".rpyc", ".rpa"].contains { path.lowercased().hasSuffix($0) }
    }

    /// Ren'Py loads scripts from anywhere under game/, such as game/scripts/act1/scenes/, deeper than the probe looks.
    /// The walk lists one level at a time and stops at the first script, so the usual shallow scripts turn up before
    /// deep asset folders are listed. It does not follow symlinked folders, so a link loop can't keep it going.
    static func containsScript(_ folder: URL) -> Bool {
        var unlimited = Int.max
        return containsScript(folder, budget: &unlimited)
    }

    /// Gives up once it has visited `budget` entries, and takes them from `budget`.
    static func containsScript(_ folder: URL, budget: inout Int) -> Bool {
        let keys: Set<URLResourceKey> = [.isDirectoryKey, .isSymbolicLinkKey]
        var level = [folder]
        while !level.isEmpty {
            var next: [URL] = []
            for directory in level {
                guard let items = try? FileManager.default.contentsOfDirectory(
                    at: directory, includingPropertiesForKeys: Array(keys), options: .skipsHiddenFiles) else { continue }
                for item in items {
                    guard budget > 0 else { return false }
                    budget -= 1
                    if isScript(item.lastPathComponent) { return true }
                    let values = try? item.resourceValues(forKeys: keys)
                    if values?.isDirectory == true, values?.isSymbolicLink != true { next.append(item) }
                }
            }
            level = next
        }
        return false
    }

    /// Identifies one release of a game without naming it, for the list of games tested on Ren'Py 8: the first 16 hex
    /// digits of a SHA-256 over the names and sizes of the scripts and archives directly in `game/`. Another release or
    /// a modded copy gets another fingerprint. Nil when `game/` holds none of those files.
    public static func fingerprint(base: URL) -> String? {
        let game = base.appendingPathComponent("game", isDirectory: true)
        let keys: Set<URLResourceKey> = [.fileSizeKey, .isRegularFileKey]
        guard let items = try? FileManager.default.contentsOfDirectory(at: game, includingPropertiesForKeys: Array(keys)) else { return nil }
        let lines = items.compactMap { item -> String? in
            guard ["rpa", "rpyc", "rpy"].contains(item.pathExtension.lowercased()),
                  let values = try? item.resourceValues(forKeys: keys), values.isRegularFile == true else { return nil }
            return "\(item.lastPathComponent)\t\(values.fileSize ?? 0)\n"
        }.sorted()
        guard !lines.isEmpty else { return nil }
        let digest = SHA256.hash(data: Data(lines.joined().utf8))
        return digest.prefix(8).map { String(format: "%02x", $0) }.joined()
    }

    /// Decides only when every version names the same line.
    static func runtime(versions: [[Int]]) -> RenPyRuntime? {
        let lines = Set(versions.map { $0[0] >= 8 ? RenPyRuntime.renPy8 : .renPy7 })
        return lines.count == 1 ? lines.first : nil
    }

    /// `py3-*` and `python3.x` are Ren'Py 8, `py2-*` and `python2.7` are Ren'Py 7.4 to 7.8, and unprefixed platform
    /// folders such as `linux-x86_64` are Ren'Py 7.3 and older. Names are lowercased.
    static func runtime(libraries names: [String]) -> RenPyRuntime? {
        if names.contains(where: { $0.hasPrefix("py3-") || $0.hasPrefix("python3") }) { return .renPy8 }
        let platforms = ["py2-", "python2", "linux-", "windows-", "darwin-", "mac-"]
        if names.contains(where: { name in platforms.contains { name.hasPrefix($0) } }) { return .renPy7 }
        return nil
    }

    /// Ren'Py 7 ships `renpy/*.pyo`, Ren'Py 8 `renpy/__pycache__/*.pyc`. Names are lowercased.
    static func runtime(engineFiles names: [String]) -> RenPyRuntime? {
        if names.contains("__pycache__") { return .renPy8 }
        if names.contains(where: { $0.hasSuffix(".pyo") }) { return .renPy7 }
        return nil
    }

    /// `version = '8.5.3.26051504'` in Ren'Py 8.2 and later.
    static func vcVersion(_ source: String) -> [Int]? {
        guard let match = source.firstMatch(of: #/(?m)^version\s*=\s*['"](\d+)\.(\d+)\.(\d+)/#) else { return nil }
        return [match.1, match.2, match.3].compactMap { Int($0) }
    }

    /// Every `version_tuple = (7, 4, 2, vc_version)` in `renpy/__init__.py`, in file order.
    static func versionTuples(_ source: String?) -> [[Int]] {
        guard let source else { return [] }
        return source.matches(of: #/version_tuple\s*=\s*\((\d+),\s*(\d+),\s*(\d+)/#).map { match in
            [match.1, match.2, match.3].compactMap { Int($0) }
        }
    }
}
