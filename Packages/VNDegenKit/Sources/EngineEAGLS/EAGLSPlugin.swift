import Foundation
import VNCore

extension EngineID {
    nonisolated public static let eagls = EngineID("eagls")
}

nonisolated public struct EAGLSPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .eagls, displayName: "EAGLS", shortName: "EAGLS", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = EAGLSLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let layout = EAGLSLayout(FolderProbe(scanning: launch.folder)) else { throw EAGLSError.notEAGLS }
        let root = layout.url(in: launch.folder)
        return EAGLSSession(launch, root: root, saveDirectory: root)
    }

    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let layout = EAGLSLayout(FolderProbe(scanning: folder)) else { return [] }
        let directory = FileCase.resolve("Save", in: layout.url(in: folder))
        return [SaveLocation(directory: directory) { path in Self.saveName(path) }]
    }

    public var defaultControls: ControlLayout { .visualNovel }

    nonisolated static func saveName(_ path: String) -> String? {
        guard let name = path.replacingOccurrences(of: "\\", with: "/").split(separator: "/").last.map(String.init) else { return nil }
        let lower = name.lowercased()
        if lower == "globalsave.sav" || lower == "vndegen.cfg" || lower == "vndegenquick.sav" { return name }
        guard lower.hasPrefix("save"), lower.hasSuffix(".sav") else { return nil }
        let digits = lower.dropFirst(4).dropLast(4)
        return !digits.isEmpty && digits.count <= 2 && digits.allSatisfy(\.isNumber) ? name : nil
    }
}

nonisolated public enum EAGLSError: Error, LocalizedError {
    case notEAGLS

    public var errorDescription: String? {
        switch self {
        case .notEAGLS: "No EAGLS game was found in this folder."
        }
    }
}
