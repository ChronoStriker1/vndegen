import Foundation
import VNCore

extension EngineID {
    nonisolated public static let rpgMakerXP = EngineID("rpgmaker.xp")
    nonisolated public static let rpgMakerVX = EngineID("rpgmaker.vx")
    nonisolated public static let rpgMakerVXAce = EngineID("rpgmaker.vxace")
}

/// RPG Maker XP, VX, and VX Ace, the RGSS engines, run on mkxp-z's libretro core inside VNDegen. One plugin per
/// version, as for MV and MZ, so each has its own badge.
nonisolated public struct RGSSPlugin: EnginePlugin {
    public enum Flavor: Sendable { case xp, vx, vxAce }

    public let flavor: Flavor
    /// The folder of an installed runtime package, or nil when it isn't installed.
    private let runtimeFolder: @Sendable (RuntimePackage) -> URL?

    public init(_ flavor: Flavor, runtimeFolder: @escaping @Sendable (RuntimePackage) -> URL?) {
        self.flavor = flavor
        self.runtimeFolder = runtimeFolder
    }

    /// The engine framework is arm64 only, so an Intel Mac lists these games but can't run them.
    static var platforms: Set<Platform> {
        #if arch(arm64)
        [.macOS, .iOS]
        #else
        [.iOS]
        #endif
    }

    public var descriptor: EngineDescriptor {
        switch flavor {
        case .xp: EngineDescriptor(id: .rpgMakerXP, displayName: "RPG Maker XP", shortName: "XP", platforms: Self.platforms)
        case .vx: EngineDescriptor(id: .rpgMakerVX, displayName: "RPG Maker VX", shortName: "VX", platforms: Self.platforms)
        case .vxAce: EngineDescriptor(id: .rpgMakerVXAce, displayName: "RPG Maker VX Ace", shortName: "VX Ace", platforms: Self.platforms)
        }
    }

    private var version: RGSSLayout.Version {
        switch flavor {
        case .xp: .xp
        case .vx: .vx
        case .vxAce: .vxAce
        }
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = RGSSLayout(probe), layout.version == version else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let layout = RGSSLayout(FolderProbe(scanning: launch.folder)) else { throw RGSSError.notRGSS }
        guard let engine = RGSSEngine.shared else { throw RGSSError.engineMissing }
        if let missing = missingRuntimes(layout).first { throw RGSSError.runtimeMissing(missing.name) }
        let paths = RGSSPaths(folder: launch.folder, dataDirectory: launch.dataDirectory, layout: layout,
                              runtimes: installedRuntimes(layout))
        return try RGSSSession(launch, paths: paths, engine: engine)
    }

    public func runtimePackages(for game: Game, folder: URL) -> [RuntimePackage] {
        guard let layout = RGSSLayout(FolderProbe(scanning: folder)) else { return [] }
        return layout.rtps.compactMap(RGSSRuntimes.package(named:))
    }

    public func compatibilityNote(for game: Game, folder: URL, dataDirectory: URL) -> String? {
        guard Self.platforms.contains(.current) else {
            return "\(descriptor.displayName) games need a Mac with Apple silicon. They run on iPhone and iPad too."
        }
        guard let layout = RGSSLayout(FolderProbe(scanning: folder)) else { return nil }
        var notes: [String] = []
        let unknown = layout.rtps.filter { RGSSRuntimes.package(named: $0) == nil }
        if !unknown.isEmpty {
            notes.append("The game names an RTP VNDegen doesn't know, \(unknown.joined(separator: ", ")). It may be missing graphics or sounds.")
        }
        if let problem = Self.lastProblem(dataDirectory) {
            notes.append("The last time it ran: \(problem)")
        }
        return notes.isEmpty ? nil : notes.joined(separator: "\n")
    }

    private func missingRuntimes(_ layout: RGSSLayout) -> [RuntimePackage] {
        layout.rtps.compactMap(RGSSRuntimes.package(named:)).filter { runtimeFolder($0) == nil }
    }

    /// The installed RTPs the game names, by the name it gives them.
    private func installedRuntimes(_ layout: RGSSLayout) -> [String: URL] {
        var folders: [String: URL] = [:]
        for name in layout.rtps {
            if let package = RGSSRuntimes.package(named: name), let folder = runtimeFolder(package) { folders[name] = folder }
        }
        return folders
    }

    // MARK: The last run's problem

    private static func problemFile(_ dataDirectory: URL) -> URL {
        dataDirectory.appendingPathComponent("logs/rgss-problem.txt")
    }

    /// Keeps why the last run ended with an error, for the game page, or clears it after a clean run.
    static func record(problem: String?, dataDirectory: URL) {
        let file = problemFile(dataDirectory)
        if let problem {
            try? FileManager.default.createDirectory(at: file.deletingLastPathComponent(), withIntermediateDirectories: true)
            try? Data(problem.utf8).write(to: file)
        } else {
            try? FileManager.default.removeItem(at: file)
        }
    }

    static func lastProblem(_ dataDirectory: URL) -> String? {
        (try? String(contentsOf: problemFile(dataDirectory), encoding: .utf8)).flatMap { $0.isEmpty ? nil : $0 }
    }
}

/// The files a launch hands the engine. The game's data directory holds its saves, the engine's log, and the engine's
/// own folder, where each RTP the game names links to its installed copy.
nonisolated struct RGSSPaths {
    var gameIni: URL
    var system: URL
    var saves: URL
    var log: URL
    /// Installed RTP folders by the name the game gives them.
    var runtimes: [String: URL]

    init(folder: URL, dataDirectory: URL, layout: RGSSLayout, runtimes: [String: URL]) {
        gameIni = folder.appendingPathComponent(layout.gameIni)
        system = dataDirectory.appendingPathComponent("rgss", isDirectory: true)
        saves = RGSSPlugin.saveDirectory(dataDirectory)
        log = dataDirectory.appendingPathComponent("logs/rgss.txt")
        self.runtimes = runtimes
    }

    /// The engine looks RTPs up by name in `mkxp-z/RTP` inside its folder.
    var rtpFolder: URL { system.appendingPathComponent("mkxp-z/RTP", isDirectory: true) }

    /// Creates the folders and links the RTPs, replacing the last launch's links.
    func prepare() throws {
        let fileManager = FileManager.default
        for folder in [system, saves, log.deletingLastPathComponent()] {
            try fileManager.createDirectory(at: folder, withIntermediateDirectories: true)
        }
        try? fileManager.removeItem(at: rtpFolder)
        try fileManager.createDirectory(at: rtpFolder, withIntermediateDirectories: true)
        for (name, folder) in runtimes {
            try fileManager.createSymbolicLink(at: rtpFolder.appendingPathComponent(name), withDestinationURL: folder)
        }
    }
}

nonisolated enum RGSSError: LocalizedError {
    case notRGSS
    case engineMissing
    case runtimeMissing(String)

    var errorDescription: String? {
        switch self {
        case .notRGSS: "The folder holds no RPG Maker XP, VX, or VX Ace game."
        case .engineMissing:
            #if os(macOS)
            "This Mac can't run RPG Maker XP, VX, or VX Ace games. They need a Mac with Apple silicon."
            #else
            "The RPG Maker XP, VX, and VX Ace engine is missing from this build of VNDegen."
            #endif
        case let .runtimeMissing(name): "The game needs the \(name). Install it on the game's page."
        }
    }
}
