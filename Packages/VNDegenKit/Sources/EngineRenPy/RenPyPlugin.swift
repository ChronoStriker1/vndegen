import Foundation
import OSLog
import VNCore

nonisolated let renPyLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "renpy")

extension EngineID {
    nonisolated public static let renPy = EngineID("renpy")
}

/// Ren'Py games. On macOS each game runs on a bundled runtime in a helper process with its own window. On iPhone and
/// iPad every game runs inside the app on Ren'Py 8, Ren'Py 7 games included.
nonisolated public struct RenPyPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .renPy, displayName: "Ren'Py", shortName: "Ren'Py", platforms: [.macOS, .iOS])
    }

    /// The game window takes touches directly, with no touch controls over it.
    public var readsTouches: Bool { true }

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
        guard let layout = RenPyLayout(FolderProbe(scanning: launch.folder)) else { throw RenPyError.notRenPy }
        return try RenPyEmbeddedSession(launch, layout: layout)
        #endif
    }

    nonisolated public func compatibilityNote(for game: Game, folder: URL, dataDirectory: URL) -> String? {
        guard let layout = RenPyLayout(FolderProbe(scanning: folder)) else { return nil }
        #if os(macOS)
        let runtime = Self.runtimeOverride(dataDirectory: dataDirectory) ?? layout.runtime
        return "Runs on the bundled Ren'Py \(runtime.version)."
        #else
        let base = folder.appendingPathComponent(layout.path(""), isDirectory: true)
        return RenPyCompatibility.note(layout: layout, fingerprint: RenPyLayout.fingerprint(base: base),
                                       record: RenPyBootRecord.read(dataDirectory))
        #endif
    }

    nonisolated public func backupWarning(for game: Game, dataDirectory: URL) -> String? {
        RenPyCompatibility.backupWarning(gameVersion: game.detected?.version, runtime: Self.runtime(for: game, dataDirectory: dataDirectory))
    }

    /// The runtime choice is macOS only, since iOS has only Ren'Py 8. An advanced option: detection gets it right for
    /// almost every game.
    nonisolated public func variants(for game: Game, dataDirectory: URL) -> EngineVariants? {
        #if os(macOS)
        let automatic = game.detected?.version.map { Self.line(of: $0) }
        return EngineVariants(
            title: "Ren'Py runtime",
            options: RenPyRuntime.allCases.map { .init(id: $0.rawValue, title: "Ren'Py \($0.version)") },
            selected: Self.runtimeOverride(dataDirectory: dataDirectory)?.rawValue,
            automatic: automatic.map { "Ren'Py \($0.version), for the game's version" } ?? "Picked from the game's files")
        #else
        nil
        #endif
    }

    nonisolated public func selectVariant(_ id: String?, for game: Game, dataDirectory: URL) throws {
        try Self.setRuntimeOverride(id.flatMap(RenPyRuntime.init(rawValue:)), dataDirectory: dataDirectory)
    }

    /// The runtime a game runs on here: always Ren'Py 8 on iOS, and on macOS the player's choice or its own line.
    static func runtime(for game: Game, dataDirectory: URL) -> RenPyRuntime {
        #if os(macOS)
        return runtimeOverride(dataDirectory: dataDirectory) ?? game.detected?.version.map(line) ?? .renPy8
        #else
        return .renPy8
        #endif
    }

    static func line(of version: String) -> RenPyRuntime { (Int(version.prefix { $0.isNumber }) ?? 8) >= 8 ? .renPy8 : .renPy7 }

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
    case savesNotImported(String)
    case busy

    public var errorDescription: String? {
        switch self {
        case .notRenPy: "The folder does not hold a Ren'Py game."
        case let .runtimeMissing(runtime): "The \(runtime.displayName) runtime is missing from this build of VNDegen."
        case let .savesNotImported(reason): "The saves in the game's game/saves folder could not be copied in. \(reason)"
        case .busy: "Another Ren'Py game is still closing. Try again in a moment."
        }
    }
}
