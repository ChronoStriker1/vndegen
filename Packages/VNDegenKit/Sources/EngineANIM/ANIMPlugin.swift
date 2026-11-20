import Foundation
import VNCore

extension EngineID {
    nonisolated public static let anim = EngineID("anim")
}

nonisolated public struct ANIMPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .anim, displayName: "ANIM", shortName: "ANIM", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = ANIMLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let layout = ANIMLayout(FolderProbe(scanning: launch.folder)) else { throw ANIMError.notANIM }
        let root = layout.url(in: launch.folder)
        return ANIMSession(launch, root: root, saveDirectory: root)
    }

    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let layout = ANIMLayout(FolderProbe(scanning: folder)) else { return [] }
        let base = layout.base
        return [SaveLocation(directory: layout.url(in: folder)) { path in Self.saveName(path, base: base) }]
    }

    public var defaultControls: ControlLayout { .visualNovel }

    nonisolated static func saveName(_ path: String, base: String) -> String? {
        let parts = path.replacingOccurrences(of: "\\", with: "/").split(separator: "/").map(String.init)
        guard let name = parts.last else { return nil }
        let lower = name.lowercased()
        let stem = base.lowercased()
        if lower == "\(stem)_save.dat" || lower == "\(stem)_system.dat" || lower == "qsavedata.dat" { return name }
        guard parts.count >= 2, parts[parts.count - 2].lowercased() == "cg", lower.hasPrefix("psavethumb"), lower.hasSuffix(".png")
        else { return nil }
        let digits = lower.dropFirst(10).dropLast(4)
        guard digits.count == 3, digits.allSatisfy(\.isNumber) else { return nil }
        return "\(parts[parts.count - 2])/\(name)"
    }
}

nonisolated public enum ANIMError: Error, LocalizedError {
    case notANIM

    public var errorDescription: String? {
        switch self {
        case .notANIM: "No ANIM game was found in this folder."
        }
    }
}
