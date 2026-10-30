import Foundation
import VNCore

extension EngineID {
    nonisolated public static let nScripter = EngineID("nscripter")
}

/// NScripter games, run on VNDegen's own NScripter runtime (the CNScripter target) inside the app.
nonisolated public struct NScripterPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .nScripter, displayName: "NScripter", shortName: "NScripter", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = NScripterLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: layout.confidence, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let layout = NScripterLayout(FolderProbe(scanning: launch.folder)) else { throw NScripterError.notNScripter }
        return NScripterSession(launch, root: layout.url(in: launch.folder))
    }

    /// NScripter reads the mouse and keyboard and no gamepad.
    public var defaultControls: ControlLayout { .keyboardAndMouse }
}

nonisolated public enum NScripterError: Error, LocalizedError {
    case notNScripter

    public var errorDescription: String? {
        switch self {
        case .notNScripter: "No NScripter game was found in this folder."
        }
    }
}
