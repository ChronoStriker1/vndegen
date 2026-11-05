import Foundation
import VNCore

/// Where a Windows Unity game's files are in the folder the player copied: the folder holding the player, `Game.exe`
/// beside `Game_Data`, at the top or up to two folders down. `Game_Data` holds `globalgamemanagers`, or `mainData` in
/// Unity 4 and older, or `data.unity3d` when the build packed its data into one file. docs/unity.md has the survey.
nonisolated struct UnityLayout: Equatable, Sendable {
    enum Backend: String, Sendable {
        /// The game's code ships as .NET assemblies in `Game_Data/Managed`.
        case mono = "Mono"
        /// The game's code is compiled to x86-64 in `GameAssembly.dll`.
        case il2cpp = "IL2CPP"
    }

    /// The folder holding the player, relative to the game folder, "" for the top.
    var root: String
    /// The data folder, relative to the game folder, such as `Game_Data`.
    var dataFolder: String
    /// The file that identified the game, relative to the game folder.
    var evidence: String
    /// Never more than likely, so another engine that matches the folder keeps it: an RPG Maker game in a wrapper folder
    /// is only likely, and may ship a Unity tool beside it. Likely when the player is beside the data, weak otherwise.
    var confidence: DetectionConfidence
    var backend: Backend?
    /// A Mac build the game ships beside the Windows one, such as `SiNiSistar2.app`, relative to the game folder.
    var macApp: String?

    static let dataFiles = ["globalgamemanagers", "maindata", "data.unity3d"]

    /// Lists `folder` and finds its game.
    init?(scanning folder: URL) {
        self.init(FolderProbe(scanning: folder))
    }

    /// `list` names what a folder of the game holds, for folders deeper than the probe lists. The default reads the
    /// disk, and the detection comparison replaces it with a listing taken elsewhere.
    init?(_ probe: FolderProbe, list: ((String) -> [String])? = nil) {
        let fallback = list ?? { path in
            let names = (try? FileManager.default.contentsOfDirectory(atPath: probe.root.appendingPathComponent(path).path)) ?? []
            return names.filter { !$0.hasPrefix(".") }
        }
        let list = { (path: String) in Self.listing(probe, path, fallback: fallback) }
        var best: UnityLayout?
        // Sorted, because a tie keeps the first found and a Set's order changes per launch.
        for entry in probe.entries.sorted() {
            let parts = entry.split(separator: "/").map(String.init)
            guard let name = parts.last, name.count > 5, name.lowercased().hasSuffix("_data"), parts.count <= 3 else { continue }
            let root = parts.dropLast().joined(separator: "/")
            if let current = best, current.root.count <= root.count, current.confidence == .likely { continue }
            // The probe lists three levels, so a data folder two folders down shows without its contents.
            let contents = list(entry)
            let data = Self.dataFiles.lazy.compactMap { file in contents.first { $0.lowercased() == file } }.first
            guard let data else { continue }
            let prefix = root.isEmpty ? "" : root.lowercased() + "/"
            // Unity names the data folder after the player's exe. UnityPlayer.dll sits beside it from Unity 2017.2.
            let exe = prefix + name.dropLast(5).lowercased() + ".exe"
            let player = probe.contains(exe) || probe.contains(prefix + "unityplayer.dll")
            let backend: Backend? = if probe.contains(prefix + "gameassembly.dll") || contents.contains(where: { $0.lowercased() == "il2cpp_data" }) {
                .il2cpp
            } else if contents.contains(where: { $0.lowercased() == "managed" }) {
                .mono
            } else {
                nil
            }
            let candidate = UnityLayout(root: root, dataFolder: entry, evidence: "\(entry)/\(data)",
                                        confidence: player ? .likely : .weak, backend: backend, macApp: nil)
            if let current = best {
                if candidate.confidence < current.confidence { continue }
                if candidate.confidence == current.confidence, candidate.root.count >= current.root.count { continue }
            }
            best = candidate
        }
        guard var best else { return nil }
        best.macApp = Self.macApp(probe, list: list)
        self = best
    }

    private init(root: String, dataFolder: String, evidence: String, confidence: DetectionConfidence, backend: Backend?, macApp: String?) {
        self.root = root
        self.dataFolder = dataFolder
        self.evidence = evidence
        self.confidence = confidence
        self.backend = backend
        self.macApp = macApp
    }

    /// The names in `path`, from the probe when it lists that deep, otherwise from `fallback`.
    private static func listing(_ probe: FolderProbe, _ path: String, fallback: (String) -> [String]) -> [String] {
        let prefix = path + "/"
        let names = probe.entries.compactMap { entry -> String? in
            guard entry.hasPrefix(prefix) else { return nil }
            let rest = entry.dropFirst(prefix.count)
            return rest.contains("/") ? nil : String(rest)
        }
        return names.isEmpty ? fallback(path) : names
    }

    /// A Unity Mac build anywhere in the listing, such as SiNiSistar 2's `MAC/SiNiSistar2.app` beside its `WIN` folder.
    /// `Game.app/Contents/Resources/Data` holds the same data files as the Windows build's data folder. Each folder is
    /// matched whatever its case, as a copy on a disk that minds case may have it otherwise.
    private static func macApp(_ probe: FolderProbe, list: (String) -> [String]) -> String? {
        let apps = probe.entries.filter { $0.lowercased().hasSuffix(".app") }
            .sorted { ($0.split(separator: "/").count, $0) < ($1.split(separator: "/").count, $1) }
        return apps.first { app in
            var path = app
            for folder in ["contents", "resources", "data"] {
                guard let name = list(path).first(where: { $0.lowercased() == folder }) else { return false }
                path += "/\(name)"
            }
            return list(path).contains { dataFiles.contains($0.lowercased()) }
        }
    }

    /// The Unity version the data file's header names, such as `2022.3.62f2`, or `2021.3.21f1c1` from Unity China. Every format puts it in the first bytes:
    /// serialized files after their header, and packed `data.unity3d` bundles after `UnityFS` and the format version.
    static func version(_ probe: FolderProbe, evidence: String) -> String? {
        guard let head = probe.read(evidence, limit: 128) else { return nil }
        let text = String(decoding: head.map { (0x20..<0x7f).contains($0) ? $0 : 0x20 }, as: UTF8.self)
        guard let match = text.firstMatch(of: /\b\d{1,4}\.\d{1,2}\.\d{1,2}[abfpx]\d{1,3}(?:c\d{1,2})?\b/) else { return nil }
        return String(match.output)
    }
}
