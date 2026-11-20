import Foundation
import VNCore

extension EngineID {
    nonisolated public static let pjadv = EngineID("pjadv")
}

nonisolated public struct PJADVPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .pjadv, displayName: "PJADV", shortName: "PJADV", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = PJADVLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let layout = PJADVLayout(FolderProbe(scanning: launch.folder)) else { throw PJADVError.notPJADV }
        let root = layout.url(in: launch.folder)
        return PJADVSession(launch, root: root, saveDirectory: FileCase.resolve("save", in: root))
    }

    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let layout = PJADVLayout(FolderProbe(scanning: folder)) else { return [] }
        let directory = FileCase.resolve("save", in: layout.url(in: folder))
        return [SaveLocation(directory: directory) { path in Self.saveName(path) }]
    }

    public var defaultControls: ControlLayout { .mouse }

    nonisolated static func saveName(_ path: String) -> String? {
        guard let name = path.replacingOccurrences(of: "\\", with: "/").split(separator: "/").last.map(String.init) else { return nil }
        return ["syssave.dat", "syssvpo.dat", "savedata.dat", "pconfig.dat"].contains(name.lowercased()) ? name : nil
    }
}

nonisolated public enum PJADVError: Error, LocalizedError {
    case notPJADV

    public var errorDescription: String? {
        switch self {
        case .notPJADV: "No PJADV game was found in this folder."
        }
    }
}

nonisolated enum PJADVLanguage: Int32 {
    case english = 1
    case japanese = 2
    case chinese = 3

    static func choose(preferred: [String] = Locale.preferredLanguages) -> PJADVLanguage {
        for tag in preferred {
            switch Locale.Language(identifier: tag).languageCode?.identifier {
            case "en": return .english
            case "ja": return .japanese
            case "zh": return .chinese
            default: continue
            }
        }
        return .english
    }
}
