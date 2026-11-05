import Foundation
import VNCore

extension EngineID {
    nonisolated public static let unity = EngineID("unity")
}

/// Recognizes Windows Unity games so their page can say why VNDegen can't run them. It runs on no platform, so Play
/// stays unavailable and the engine can't be chosen by hand. docs/unity.md has the study behind that.
nonisolated public struct UnityPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .unity, displayName: "Unity", shortName: "Unity", platforms: [])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = UnityLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: layout.confidence,
                           version: UnityLayout.version(probe, evidence: layout.evidence), evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        throw UnityError.cantRun
    }

    public func compatibilityNote(for game: Game, folder: URL, dataDirectory: URL) -> String? {
        let layout = UnityLayout(scanning: folder)
        var note = UnityError.cantRun.localizedDescription
        switch layout?.backend {
        case .mono: note += " This game's code is .NET built for Unity's Windows player (Mono)."
        case .il2cpp: note += " This game's code is compiled for Windows PCs (IL2CPP)."
        case nil: break
        }
        if let app = layout?.macApp {
            let name = app.split(separator: "/").last.map(String.init) ?? app
            #if os(macOS)
            note += " It includes its own Mac version, \(name), which you can open from the game's folder."
            #else
            note += " It includes its own Mac version, \(name), which runs on a Mac outside VNDegen."
            #endif
        }
        return note
    }
}

nonisolated public enum UnityError: Error, LocalizedError {
    case cantRun

    public var errorDescription: String? {
        "VNDegen can't run Unity games. Unity's terms don't let another app include its player, and the game's code and graphics are made for Windows, so it would need a whole new engine."
    }
}
