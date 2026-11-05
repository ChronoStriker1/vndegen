import Foundation
import VNCore

extension EngineID {
    nonisolated public static let nekoSDK = EngineID("nekosdk")
}

/// NekoSDK games, run on VNDegen's own NekoSDK runtime (the CNekoSDK target) inside the app.
nonisolated public struct NekoSDKPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .nekoSDK, displayName: "NekoSDK", shortName: "NekoSDK", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = NekoSDKLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: layout.confidence, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let layout = NekoSDKLayout(FolderProbe(scanning: launch.folder)) else { throw NekoSDKError.notNekoSDK }
        return NekoSDKSession(launch, root: layout.url(in: launch.folder))
    }
}

nonisolated public enum NekoSDKError: Error, LocalizedError {
    case notNekoSDK

    public var errorDescription: String? {
        switch self {
        case .notNekoSDK: "No NekoSDK game was found in this folder."
        }
    }
}
