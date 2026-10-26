import Foundation
import VNCore

/// Where an RGSS game sits in its folder and what it needs. The game's root holds `Game.ini`, at the top of the folder
/// or up to two folders down, as games unpacked from an archive often are.
nonisolated struct RGSSLayout: Equatable, Sendable {
    enum Version: String, CaseIterable, Sendable {
        case xp, vx, vxAce

        /// The packed archive and the scripts file each version's editor writes.
        var archive: String {
            switch self {
            case .xp: "Game.rgssad"
            case .vx: "Game.rgss2a"
            case .vxAce: "Game.rgss3a"
            }
        }

        var scripts: String {
            switch self {
            case .xp: "Data/Scripts.rxdata"
            case .vx: "Data/Scripts.rvdata"
            case .vxAce: "Data/Scripts.rvdata2"
            }
        }

        /// The RGSS major version in `Game.ini`'s `Library=RGSS<n>xx.dll`.
        var rgss: Character {
            switch self {
            case .xp: "1"
            case .vx: "2"
            case .vxAce: "3"
            }
        }
    }

    /// The folder holding `Game.ini`, relative to the game's folder, empty for the folder itself.
    var root: String
    var version: Version
    /// The file that told the version apart, relative to the game's folder.
    var evidence: String
    /// RTP names `Game.ini` lists, such as `RPGVXAce`, in its order.
    var rtps: [String]

    init?(_ probe: FolderProbe) {
        // Shallowest first, so a game's own Game.ini wins over one in a subfolder such as a bundled tool.
        let inis = probe.entries.filter { ($0 as NSString).lastPathComponent.lowercased() == "game.ini" }
            .sorted { ($0.split(separator: "/").count, $0) < ($1.split(separator: "/").count, $1) }
        for ini in inis {
            let root = (ini as NSString).deletingLastPathComponent
            guard let settings = probe.read(ini).map(Self.settings) else { continue }
            if let (version, evidence) = Self.version(probe, root: root, settings: settings) {
                self.root = root
                self.version = version
                self.evidence = evidence
                rtps = Self.rtpNames(settings)
                return
            }
        }
        return nil
    }

    /// `Game.ini` relative to the game's folder.
    var gameIni: String { root.isEmpty ? "Game.ini" : "\(root)/Game.ini" }

    private static func version(_ probe: FolderProbe, root: String, settings: [String: String]) -> (Version, String)? {
        func path(_ name: String) -> String { root.isEmpty ? name : "\(root)/\(name)" }
        // The scan reads three levels, so an unpacked game two folders down needs a direct look for its scripts.
        func exists(_ name: String) -> Bool {
            probe.contains(path(name)) || FileManager.default.fileExists(atPath: probe.root.appendingPathComponent(path(name)).path)
        }
        for version in Version.allCases.reversed() {
            if exists(version.archive) { return (version, path(version.archive)) }
            if exists(version.scripts) { return (version, path(version.scripts)) }
        }
        // A game whose archive has another name still names its library and scripts file.
        let library = settings["library"]?.lowercased() ?? ""
        let scripts = settings["scripts"]?.lowercased() ?? ""
        for version in Version.allCases.reversed() {
            let ext = (version.scripts as NSString).pathExtension
            if library.contains("rgss\(version.rgss)") || scripts.hasSuffix(".\(ext)") {
                return (version, path("Game.ini"))
            }
        }
        return nil
    }

    /// `RTP=` in VX and VX Ace, `RTP1=` to `RTP3=` in XP. Empty values mean none.
    private static func rtpNames(_ settings: [String: String]) -> [String] {
        ["rtp", "rtp1", "rtp2", "rtp3"].compactMap { settings[$0] }
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .filter { !$0.isEmpty }
    }

    /// The `[Game]` section's keys, lowercased. Japanese games write Game.ini in Shift-JIS.
    static func settings(_ data: Data) -> [String: String] {
        let text = String(data: data, encoding: .utf8) ?? String(data: data, encoding: .shiftJIS) ?? String(decoding: data, as: UTF8.self)
        var section = ""
        var values: [String: String] = [:]
        for line in text.split(whereSeparator: \.isNewline) {
            let line = line.trimmingCharacters(in: .whitespaces)
            if line.hasPrefix("[") && line.hasSuffix("]") {
                section = line.dropFirst().dropLast().lowercased()
            } else if section == "game", let equals = line.firstIndex(of: "=") {
                let key = line[..<equals].trimmingCharacters(in: .whitespaces).lowercased()
                values[key] = String(line[line.index(after: equals)...])
            }
        }
        return values
    }
}
