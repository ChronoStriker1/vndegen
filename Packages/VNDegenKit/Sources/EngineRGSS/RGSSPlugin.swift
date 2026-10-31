import Foundation
import VNCore

extension EngineID {
    nonisolated public static let rpgMakerXP = EngineID("rpgmaker.xp")
    nonisolated public static let rpgMakerVX = EngineID("rpgmaker.vx")
    nonisolated public static let rpgMakerVXAce = EngineID("rpgmaker.vxace")
}

/// RPG Maker XP, VX, and VX Ace, the RGSS engines, run on VNDegen's own RGSS runtime (Engines/RGSSRuntime). One plugin
/// per version, as for MV and MZ, so each has its own badge.
nonisolated public struct RGSSPlugin: EnginePlugin {
    public enum Flavor: Sendable { case xp, vx, vxAce }

    public let flavor: Flavor
    /// The folder of an installed runtime package, or nil when it isn't installed.
    private let runtimeFolder: @Sendable (RuntimePackage) -> URL?

    public init(_ flavor: Flavor, runtimeFolder: @escaping @Sendable (RuntimePackage) -> URL?) {
        self.flavor = flavor
        self.runtimeFolder = runtimeFolder
    }

    static let platforms: Set<Platform> = [.macOS, .iOS]

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
        if let missing = missingRuntimes(layout).first { throw RGSSError.runtimeMissing(missing.name) }
        let paths = RGSSPaths(folder: launch.folder, dataDirectory: launch.dataDirectory, layout: layout,
                              runtimes: installedRuntimes(layout))
        return try RGSSSession(launch, paths: paths)
    }

    public func runtimePackages(for game: Game, folder: URL) -> [RuntimePackage] {
        guard let layout = RGSSLayout(FolderProbe(scanning: folder)) else { return [] }
        return layout.rtps.compactMap(RGSSRuntimes.package(named:))
    }

    public func compatibilityNote(for game: Game, folder: URL, dataDirectory: URL) -> String? {
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

    /// The installed RTPs the game names, in the order Game.ini names them.
    private func installedRuntimes(_ layout: RGSSLayout) -> [URL] {
        layout.rtps.compactMap { name in RGSSRuntimes.package(named: name).flatMap(runtimeFolder) }
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

/// The files a launch hands the runtime: the game's Game.ini, its saves and the runtime's log in the game's data
/// directory, and the installed RTP folders the game names.
nonisolated struct RGSSPaths {
    var gameIni: URL
    var saves: URL
    var log: URL
    var runtimes: [URL]

    init(folder: URL, dataDirectory: URL, layout: RGSSLayout, runtimes: [URL]) {
        gameIni = folder.appendingPathComponent(layout.gameIni)
        saves = RGSSPlugin.saveDirectory(dataDirectory)
        log = dataDirectory.appendingPathComponent("logs/rgss.txt")
        self.runtimes = runtimes
    }

    func prepare() throws {
        for folder in [saves, log.deletingLastPathComponent()] {
            try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        }
    }
}

nonisolated enum RGSSError: LocalizedError {
    case notRGSS
    case runtimeMissing(String)

    var errorDescription: String? {
        switch self {
        case .notRGSS: "The folder holds no RPG Maker XP, VX, or VX Ace game."
        case let .runtimeMissing(name): "The game needs the \(name). Install it on the game's page."
        }
    }
}
