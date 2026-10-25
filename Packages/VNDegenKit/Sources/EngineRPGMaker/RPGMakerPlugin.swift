import Foundation
import VNCore

extension EngineID {
    nonisolated public static let rpgMakerMV = EngineID("rpgmaker.mv")
    nonisolated public static let rpgMakerMZ = EngineID("rpgmaker.mz")
}

/// RPG Maker MV and MZ. Both are HTML5 games shipped inside NW.js, so one WebKit runtime runs both.
nonisolated public struct RPGMakerPlugin: EnginePlugin {
    public let flavor: Flavor

    public enum Flavor: Sendable { case mv, mz }

    public init(_ flavor: Flavor) { self.flavor = flavor }

    public var descriptor: EngineDescriptor {
        switch flavor {
        case .mv: EngineDescriptor(id: .rpgMakerMV, displayName: "RPG Maker MV", shortName: "MV", platforms: [.macOS, .iOS])
        case .mz: EngineDescriptor(id: .rpgMakerMZ, displayName: "RPG Maker MZ", shortName: "MZ", platforms: [.macOS, .iOS])
        }
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        let core = flavor == .mz ? "rmmz_core.js" : "rpg_core.js"
        // Deployed MV games keep the web root in www/. MZ always deploys it at the top level.
        for root in ["", "www/"] where probe.contains("\(root)js/\(core)") {
            return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: "\(root)js/\(core)")
        }
        return nil
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        try RPGMakerSession(launch: launch)
    }
}

nonisolated enum RPGMakerError: LocalizedError {
    case shimMissing

    var errorDescription: String? {
        switch self {
        case .shimMissing: "The NW.js compatibility script is missing from the app bundle."
        }
    }
}
