import Foundation
import VNCore

extension EngineID {
    nonisolated public static let godot = EngineID("godot")
}

/// Godot 3 and Godot 4 games, run on VNDegen's builds of Godot 3.6 and Godot 4.7 inside VNDegen's process. Each
/// engine is a framework the app embeds. The package only finds it at run time.
nonisolated public struct GodotPlugin: EnginePlugin {
    public init() {}

    /// The engines are arm64 only, so an Intel Mac lists these games but can't run them.
    static var platforms: Set<Platform> {
        #if arch(arm64)
        [.macOS, .iOS]
        #else
        [.iOS]
        #endif
    }

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .godot, displayName: "Godot", shortName: "Godot", platforms: Self.platforms)
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = GodotLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, version: layout.version, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let layout = GodotLayout(FolderProbe(scanning: launch.folder)) else { throw GodotError.notGodot }
        if let reason = Self.unsupported(layout, folder: launch.folder) { throw GodotError.unsupported(reason) }
        guard let engine = GodotEngine.engine(major: layout.major) else { throw GodotError.engineMissing(layout.major) }
        guard !GodotSession.engineInUse else { throw GodotError.stillClosing }
        let paths = GodotPaths(folder: launch.folder, dataDirectory: launch.dataDirectory, layout: layout)
        return try GodotSession(launch, paths: paths, engine: engine)
    }

    public func compatibilityNote(for game: Game, folder: URL, dataDirectory: URL) -> String? {
        guard let layout = GodotLayout(FolderProbe(scanning: folder)) else { return nil }
        return Self.unsupported(layout, folder: folder)
    }

    /// Why VNDegen can't run a game, or nil when it can.
    static func unsupported(_ layout: GodotLayout, folder: URL) -> String? {
        let pack = layout.header
        // GodotSteam built into the game's own copy of Godot, which then links Steam's library. Such games call Steam
        // from their scripts and won't run without it.
        if WindowsExecutable.importedLibraries(folder.appendingPathComponent(layout.executable)).contains(where: { $0.hasPrefix("steam_api") }) {
            return "This game needs Steam: Steam is built into its own copy of Godot, and that only runs on a PC with Steam."
        }
        if pack.encryptedDirectory || pack.encryptedFiles > 0 {
            return "This game's files are encrypted with a key built into its own copy of Godot, so VNDegen can't read them."
        }
        if !pack.nativeLibraries.isEmpty {
            return "This game needs Windows libraries that only run on a PC (\(pack.nativeLibraries.map(GodotLayout.name).joined(separator: ", ")))."
        }
        if !(3...4).contains(pack.major) || (pack.major == 3 && pack.minor < 2) {
            return "VNDegen runs games made with Godot 3.2 and later. This one was made with Godot \(pack.version)."
        }
        return nil
    }

    /// The Windows AppData/Roaming folder's stand-in in a game's data directory. Godot keeps `user://` there, in
    /// `Godot/app_userdata/<project name>` or the project's own folder name.
    nonisolated public static func appDataDirectory(_ dataDirectory: URL) -> URL {
        dataDirectory.appendingPathComponent("AppData/Roaming", isDirectory: true)
    }
}

nonisolated public enum GodotError: Error, LocalizedError {
    case notGodot
    case engineMissing(Int)
    case unsupported(String)
    case stillClosing

    public var errorDescription: String? {
        switch self {
        case .notGodot: "No Godot game was found in this folder."
        case let .engineMissing(major): "The Godot \(major) engine is missing from this build of VNDegen."
        case let .unsupported(reason): reason
        case .stillClosing: "The last Godot game is still closing. Try again in a moment."
        }
    }
}
