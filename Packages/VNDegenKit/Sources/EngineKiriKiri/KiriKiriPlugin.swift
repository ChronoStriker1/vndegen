import Foundation
import VNCore

extension EngineID {
    nonisolated public static let kiriKiri = EngineID("kirikiri")
}

/// KiriKiri 2 and KiriKiri Z games. Detection and saves only for now. The native engine arrives in a later phase.
nonisolated public struct KiriKiriPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .kiriKiri, displayName: "KiriKiri", shortName: "KiriKiri", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = KiriKiriLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        throw KiriKiriError.engineMissing
    }
}

nonisolated public enum KiriKiriError: Error, LocalizedError {
    case engineMissing

    public var errorDescription: String? {
        switch self {
        case .engineMissing: "The KiriKiri engine is missing from this build of VNDegen."
        }
    }
}
