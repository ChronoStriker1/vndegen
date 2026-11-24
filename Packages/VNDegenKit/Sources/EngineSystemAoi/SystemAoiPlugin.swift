import Foundation
import VNCore

extension EngineID {
    nonisolated public static let systemAoi = EngineID("systemaoi")
}

nonisolated public struct SystemAoiPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .systemAoi, displayName: "SystemAoi", shortName: "SystemAoi", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = SystemAoiLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let layout = SystemAoiLayout(FolderProbe(scanning: launch.folder)) else { throw SystemAoiError.notSystemAoi }
        return SystemAoiSession(launch, root: layout.url(in: launch.folder))
    }

    public var defaultControls: ControlLayout { .visualNovel }

    public var defaultControlChanges: [ControlInput: ControlAction] {
        [.x: .key(KeyboardKey(key: "Control", code: "ControlLeft", keyCode: 17))]
    }
}

nonisolated public enum SystemAoiError: Error, LocalizedError {
    case notSystemAoi

    public var errorDescription: String? {
        switch self {
        case .notSystemAoi: "No SystemAoi game was found in this folder."
        }
    }
}
