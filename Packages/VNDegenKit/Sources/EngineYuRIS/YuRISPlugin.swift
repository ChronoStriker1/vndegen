import Foundation
import VNCore

extension EngineID {
    nonisolated public static let yuris = EngineID("yuris")
}

nonisolated public struct YuRISPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .yuris, displayName: "YU-RIS", shortName: "YU-RIS", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = YuRISLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let layout = YuRISLayout(FolderProbe(scanning: launch.folder)) else { throw YuRISError.notYuRIS }
        return YuRISSession(launch, root: layout.url(in: launch.folder))
    }

    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let layout = YuRISLayout(FolderProbe(scanning: folder)) else { return [] }
        let directory = layout.url(in: folder).appending(path: "save", directoryHint: .isDirectory)
        return [SaveLocation(directory: directory) { path in
            guard let name = path.split(separator: "/").last.map(String.init), name.lowercased().hasSuffix(".sd") else { return nil }
            return name
        }]
    }

    public var defaultControls: ControlLayout { .visualNovel }
}

nonisolated public enum YuRISError: Error, LocalizedError {
    case notYuRIS

    public var errorDescription: String? {
        switch self {
        case .notYuRIS: "No YU-RIS game was found in this folder."
        }
    }
}
