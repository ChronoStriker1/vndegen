import Foundation
import VNCore

extension EngineID {
    nonisolated public static let renPy = EngineID("renpy")
}

/// Ren'Py games. On macOS each game runs on a bundled runtime in a helper process with its own window. iOS detects
/// Ren'Py games but cannot run them yet.
nonisolated public struct RenPyPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .renPy, displayName: "Ren'Py", shortName: "Ren'Py", platforms: [.macOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = RenPyLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, version: layout.version, evidence: layout.path("renpy/"))
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        #if os(macOS)
        guard let layout = RenPyLayout(FolderProbe(scanning: launch.folder)) else { throw RenPyError.notRenPy }
        let runtime = Self.runtimeOverride(dataDirectory: launch.dataDirectory) ?? layout.runtime
        return try RenPySession(launch, layout: layout, runtime: runtime)
        #else
        throw RenPyError.unsupportedPlatform
        #endif
    }

    /// The runtime the player picked for a game, which wins over the one its version calls for. Nil when they have not
    /// picked one. An advanced option: detection gets it right for almost every game.
    public static func runtimeOverride(dataDirectory: URL) -> RenPyRuntime? {
        guard let data = try? Data(contentsOf: settingsFile(dataDirectory)) else { return nil }
        return (try? JSONDecoder().decode(Settings.self, from: data))?.runtime
    }

    public static func setRuntimeOverride(_ runtime: RenPyRuntime?, dataDirectory: URL) throws {
        let file = settingsFile(dataDirectory)
        guard let runtime else {
            try? FileManager.default.removeItem(at: file)
            return
        }
        try FileManager.default.createDirectory(at: dataDirectory, withIntermediateDirectories: true)
        try JSONEncoder().encode(Settings(runtime: runtime)).write(to: file, options: .atomic)
    }

    private struct Settings: Codable {
        var runtime: RenPyRuntime?
    }

    private static func settingsFile(_ dataDirectory: URL) -> URL { dataDirectory.appendingPathComponent("renpy.json") }
}

nonisolated public enum RenPyError: Error, LocalizedError {
    case notRenPy
    case runtimeMissing(RenPyRuntime)
    case unsupportedPlatform

    public var errorDescription: String? {
        switch self {
        case .notRenPy: "The folder does not hold a Ren'Py game."
        case let .runtimeMissing(runtime): "The \(runtime.displayName) runtime is missing from this build of VNDegen."
        case .unsupportedPlatform: "Ren'Py does not run on this device yet."
        }
    }
}
