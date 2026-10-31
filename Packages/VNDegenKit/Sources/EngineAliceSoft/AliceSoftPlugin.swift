import Foundation
import VNCore

extension EngineID {
    nonisolated public static let aliceSoft = EngineID("alicesoft")
}

/// AliceSoft's System 3.x and System 4 games, each run on VNDegen's own clean-room runtime inside the app: CSystem3
/// for System 3.0 to 3.9, CSystem4 for System 4.
nonisolated public struct AliceSoftPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .aliceSoft, displayName: "AliceSoft System", shortName: "AliceSoft", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = AliceSoftLayout(probe) else { return nil }
        let version = switch layout.system {
        case .system30: "System 3.0"
        case .system39: "System 3.9"
        case .system4: "System 4"
        }
        return EngineMatch(engine: descriptor.id, confidence: layout.confidence, version: version, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let layout = AliceSoftLayout(FolderProbe(scanning: launch.folder)) else { throw AliceSoftError.notAliceSoft }
        switch layout.system {
        case .system30, .system39: return System3Session(launch, root: layout.url(in: launch.folder))
        case .system4: return try System4Session.make(launch, root: layout.url(in: launch.folder))
        }
    }

    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let layout = AliceSoftLayout(FolderProbe(scanning: folder)) else { return [] }
        switch layout.system {
        case .system30, .system39: return system3SaveLocations(root: layout.url(in: folder))
        case .system4: return System4Session.saveLocations(root: layout.url(in: folder), dataDirectory: dataDirectory)
        }
    }

    public func compatibilityNote(for game: Game, folder: URL, dataDirectory: URL) -> String? {
        guard let layout = AliceSoftLayout(FolderProbe(scanning: folder)) else { return nil }
        switch layout.system {
        case .system30, .system39: return nil
        case .system4: return System4Session.compatibilityNote(root: layout.url(in: folder))
        }
    }

    /// AliceSoft games read the keyboard and mouse only.
    public var defaultControls: ControlLayout { .keyboardAndMouse }
}

nonisolated public enum AliceSoftError: Error, LocalizedError {
    case notAliceSoft

    public var errorDescription: String? {
        switch self {
        case .notAliceSoft: "No AliceSoft game was found in this folder."
        }
    }
}
