import Foundation
import VNCore

extension EngineID {
    nonisolated public static let smileGame = EngineID("smilegame")
}

nonisolated public struct SmileGamePlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .smileGame, displayName: "Smile Game Builder", shortName: "SGB", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = SmileGameLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let layout = SmileGameLayout(FolderProbe(scanning: launch.folder)) else { throw SmileGameError.notSmileGame }
        return SmileGameSession(launch, root: layout.url(in: launch.folder))
    }

    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let layout = SmileGameLayout(FolderProbe(scanning: folder)) else { return [] }
        let directory = SmileGameLayout.saveDirectory(in: layout.url(in: folder))
        return [SaveLocation(directory: directory) { path in Self.saveName(path) }]
    }

    public var defaultControls: ControlLayout { .rpgMaker }

    public var defaultControlChanges: [ControlInput: ControlAction] {
        [.l: .nothing, .r: .nothing, .l2: .nothing, .r2: .nothing, .view: .nothing]
    }

    nonisolated static func saveName(_ path: String) -> String? {
        let parts = path.replacingOccurrences(of: "\\", with: "/").split(separator: "/").map(String.init)
        guard parts.count >= 2, parts[parts.count - 2].lowercased() == "savedata", let name = parts.last else { return nil }
        let lower = name.lowercased()
        if lower == "settings.dat" { return name }
        guard lower.hasPrefix("save_"), lower.hasSuffix(".sgs") else { return nil }
        let digits = lower.dropFirst("save_".count).dropLast(".sgs".count)
        return digits.count == 2 && digits.allSatisfy(\.isNumber) ? name : nil
    }
}

nonisolated public enum SmileGameError: Error, LocalizedError {
    case notSmileGame

    public var errorDescription: String? {
        switch self {
        case .notSmileGame: "No Smile Game Builder game was found in this folder."
        }
    }
}
