import Foundation
import VNCore

extension EngineID {
    nonisolated public static let artemis = EngineID("artemis")
}

/// Artemis engine games, run on art3m1s-core inside VNDegen's process and drawn with Metal. The engine is a static
/// library the app links; the package only finds it at run time.
nonisolated public struct ArtemisPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .artemis, displayName: "Artemis", shortName: "Artemis", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = ArtemisLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let core = ArtemisCore.shared else { throw ArtemisError.engineMissing }
        guard !ArtemisSession.engineInUse else { throw ArtemisError.stillClosing }
        guard let layout = ArtemisLayout(scanning: launch.folder) else { throw ArtemisError.notArtemis }
        return try ArtemisSession(launch, layout: layout, core: core)
    }

    /// Artemis reads a mouse and keyboard and no gamepad. A click reads on and presses buttons, and the right click
    /// opens the game's menu.
    public var defaultControls: ControlLayout { .keyboardAndMouse }
}

nonisolated public enum ArtemisError: Error, LocalizedError {
    case engineMissing
    case notArtemis
    case stillClosing
    case unreadable(String)

    public var errorDescription: String? {
        switch self {
        case .engineMissing: "The Artemis engine is missing from this build of VNDegen."
        case .notArtemis: "No Artemis game was found in this folder."
        case .stillClosing: "The last Artemis game is still closing. Try again in a moment."
        case let .unreadable(what): "The game's \(what) could not be read."
        }
    }
}
