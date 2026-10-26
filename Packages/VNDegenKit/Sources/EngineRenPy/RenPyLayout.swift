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
    /// folder is the base directory. Archives often unpack to one extra folder level, so that is checked too.
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

    public init?(_ probe: FolderProbe) {
        let topLevel = probe.entries.filter { !$0.contains("/") }.sorted()
        guard let base = ([""] + topLevel).first(where: { Self.isBase($0, probe) }) else { return nil }
        let prefix = base.isEmpty ? "" : "\(base)/"
        let text = { (path: String) in probe.read(prefix + path).map { String(decoding: $0, as: UTF8.self) } }
        let children = { (folder: String) in
            let lower = (prefix + folder + "/").lowercased()
            return probe.entries.compactMap { entry -> String? in
                let rest = entry.lowercased()
                guard rest.hasPrefix(lower) else { return nil }
                let name = rest.dropFirst(lower.count)
                return name.contains("/") ? nil : String(name)
            }
        }

        let vcVersion = text("renpy/vc_version.py").flatMap(Self.vcVersion)
        let tuples = vcVersion.map { [$0] } ?? Self.versionTuples(text("renpy/__init__.py"))
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

    private static func isBase(_ base: String, _ probe: FolderProbe) -> Bool {
        let prefix = base.isEmpty ? "" : "\(base)/".lowercased()
        guard probe.contains("\(prefix)renpy"), probe.contains("\(prefix)game") else { return false }
        return probe.entries.contains { entry in
            let lower = entry.lowercased()
            return lower.hasPrefix("\(prefix)game/") && [".rpy", ".rpyc", ".rpa"].contains { lower.hasSuffix($0) }
        }
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
