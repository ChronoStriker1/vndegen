import CIkura
import Foundation
import VNCore

extension EngineID {
    nonisolated public static let ikura = EngineID("ikura")
}

nonisolated public struct IkuraPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .ikura, displayName: "Ikura GDL", shortName: "Ikura", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = IkuraLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let layout = IkuraLayout(FolderProbe(scanning: launch.folder)) else { throw IkuraError.notIkura }
        let root = layout.url(in: launch.folder)
        guard vnikura_probe(root.path, nil, 0) else { throw IkuraError.noStartScript }
        return IkuraSession(launch, root: root, saveDirectory: root)
    }

    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let layout = IkuraLayout(FolderProbe(scanning: folder)) else { return [] }
        return [SaveLocation(directory: layout.url(in: folder)) { path in Self.saveFileName(path) }]
    }

    public var defaultControls: ControlLayout { .visualNovel }

    nonisolated static func saveFileName(_ path: String) -> String? {
        guard let name = path.replacingOccurrences(of: "\\", with: "/").split(separator: "/").last.map(String.init) else { return nil }
        let lower = name.lowercased()
        return lower.hasSuffix(".sav") && lower.count > 4 ? name : nil
    }
}

nonisolated public enum IkuraError: Error, LocalizedError {
    case notIkura
    case noStartScript

    public var errorDescription: String? {
        switch self {
        case .notIkura: "No Ikura GDL game was found in this folder."
        case .noStartScript: "This game's SNR archive has no START.SNR."
        }
    }
}
