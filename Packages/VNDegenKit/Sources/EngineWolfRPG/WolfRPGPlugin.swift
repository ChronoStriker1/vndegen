import Foundation
import VNCore

extension EngineID {
    nonisolated public static let wolfRPG = EngineID("wolfrpg")
}

/// Wolf RPG Editor games, run on VNDegen's own Wolf RPG Editor runtime (the CWolfRPG target) inside the app.
nonisolated public struct WolfRPGPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .wolfRPG, displayName: "Wolf RPG Editor", shortName: "Wolf RPG", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = WolfRPGLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: layout.confidence, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let layout = WolfRPGLayout(FolderProbe(scanning: launch.folder)) else { throw WolfRPGError.notWolfRPG }
        return WolfRPGSession(launch, root: layout.url(in: launch.folder))
    }

    /// Wolf RPG Editor reads gamepads itself: the first button confirms, the second cancels, the third is sub.
    public var defaultControls: ControlLayout { .gamepad }
}

nonisolated public enum WolfRPGError: Error, LocalizedError {
    case notWolfRPG

    public var errorDescription: String? {
        switch self {
        case .notWolfRPG: "No Wolf RPG Editor game was found in this folder."
        }
    }
}
