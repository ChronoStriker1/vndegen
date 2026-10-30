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
        let requirements = try Self.requirements(layout, folder: launch.folder).get()
        guard let engine = GodotEngine.engine(major: layout.major) else { throw GodotError.engineMissing(layout.major) }
        guard !GodotSession.engineInUse else { throw GodotError.stillClosing }
        guard !GodotSession.unusable.contains(layout.major) else { throw GodotError.engineUnusable(layout.major) }
        let paths = GodotPaths(folder: launch.folder, dataDirectory: launch.dataDirectory, layout: layout)
        return try GodotSession(launch, paths: paths, requirements: requirements, engine: engine)
    }

    public func compatibilityNote(for game: Game, folder: URL, dataDirectory: URL) -> String? {
        guard let layout = GodotLayout(FolderProbe(scanning: folder)),
              case let .failure(.unsupported(reason)) = Self.requirements(layout, folder: folder) else { return nil }
        return reason
    }

    /// What a game needs from VNDegen's Godot beyond its pack, or why VNDegen can't run it.
    static func requirements(_ layout: GodotLayout, folder: URL) -> Result<GodotRequirements, GodotError> {
        let pack = layout.header
        if layout.usesDotNet {
            return .failure(.unsupported("This is a Godot C# game, which needs .NET, and VNDegen runs Godot games written in GDScript only."))
        }
        if !(3...4).contains(pack.major) || (pack.major == 3 && pack.minor < 2) {
            return .failure(.unsupported("VNDegen runs games made with Godot 3.2 and later. This one was made with Godot \(pack.version)."))
        }
        var requirements = GodotRequirements()
        var libraries: [String] = []
        for library in pack.nativeLibraries.map(GodotLayout.name) {
            switch library.lowercased() {
            case "godotsteam.gdextension": requirements.steam = true
            case "eosg.gdextension": requirements.epicOnlineServices = true
            default: libraries.append(library)
            }
        }
        if !libraries.isEmpty {
            return .failure(.unsupported("This game needs Windows libraries that only run on a PC (\(libraries.joined(separator: ", ")))."))
        }
        // GodotSteam can also be built into the game's own copy of Godot, which then links Steam's library.
        let executable = folder.appendingPathComponent(layout.executable)
        if WindowsExecutable(executable)?.importedLibraries.contains(where: { $0.hasPrefix("steam_api") }) == true {
            requirements.steam = true
        }
        if pack.encrypted && pack.encryptionKey == nil {
            return .failure(.unsupported("This game's files are encrypted with a key built into its own copy of Godot, and VNDegen couldn't find the key in its .exe."))
        }
        requirements.encryptionKey = pack.encryptionKey
        return .success(requirements)
    }

    /// The Windows AppData/Roaming folder's stand-in in a game's data directory. Godot keeps `user://` there, in
    /// `Godot/app_userdata/<project name>` or the project's own folder name.
    nonisolated public static func appDataDirectory(_ dataDirectory: URL) -> URL {
        dataDirectory.appendingPathComponent("AppData/Roaming", isDirectory: true)
    }
}

/// What a game needs from VNDegen's Godot beyond its pack: stand-ins for what the game's own copy of Godot has in it,
/// and the key its pack is encrypted with.
nonisolated struct GodotRequirements: Sendable {
    /// GodotSteam, built into the game's Godot or loaded as an extension. VNDegen's Godot answers as Steam offline.
    var steam = false
    /// The Epic Online Services extension, whose class names VNDegen's Godot 4 provides without the services.
    var epicOnlineServices = false
    var encryptionKey: Data?
}

nonisolated public enum GodotError: Error, LocalizedError {
    case notGodot
    case engineMissing(Int)
    case unsupported(String)
    case stillClosing
    case engineUnusable(Int)

    public var errorDescription: String? {
        switch self {
        case .notGodot: "No Godot game was found in this folder."
        case let .engineMissing(major): "The Godot \(major) engine is missing from this build of VNDegen."
        case let .unsupported(reason): reason
        case .stillClosing: "The last Godot game is still closing. Try again in a moment."
        case let .engineUnusable(major): "Godot \(major) couldn't start the last game, and can't start another until VNDegen is opened again."
        }
    }
}
