import Foundation

/// Stable identifier of an engine plugin. Persisted in the library, so never rename one.
public struct EngineID: RawRepresentable, Hashable, Codable, Sendable, CustomStringConvertible {
    public let rawValue: String
    public init(rawValue: String) { self.rawValue = rawValue }
    public init(_ rawValue: String) { self.rawValue = rawValue }
    public var description: String { rawValue }
}

public enum Platform: String, Hashable, Codable, Sendable, CaseIterable {
    case macOS, iOS

    public static var current: Platform {
        #if os(macOS)
        .macOS
        #else
        .iOS
        #endif
    }
}

public enum DetectionConfidence: Int, Comparable, Codable, Sendable {
    case weak, likely, certain
    public static func < (a: Self, b: Self) -> Bool { a.rawValue < b.rawValue }
}

public struct EngineMatch: Codable, Hashable, Sendable {
    public var engine: EngineID
    public var confidence: DetectionConfidence
    public var version: String?
    /// The file or folder that identified the engine, shown in the UI.
    public var evidence: String

    public init(engine: EngineID, confidence: DetectionConfidence, version: String? = nil, evidence: String) {
        self.engine = engine
        self.confidence = confidence
        self.version = version
        self.evidence = evidence
    }
}

public struct EngineDescriptor: Sendable {
    public let id: EngineID
    public let displayName: String
    /// Short name for badges, such as "MZ" or "Ren'Py".
    public let shortName: String
    public let platforms: Set<Platform>

    public init(id: EngineID, displayName: String, shortName: String, platforms: Set<Platform>) {
        self.id = id
        self.displayName = displayName
        self.shortName = shortName
        self.platforms = platforms
    }
}

/// Everything an engine needs to start a game.
public struct GameLaunch {
    public let game: Game
    /// The game's folder, as the user copied it in.
    public let folder: URL
    /// Writable per-game directory owned by the app, for engines that keep state outside the game folder.
    public let dataDirectory: URL
    /// Called by the session when the game quits itself, such as the title screen's "Quit Game".
    public let onExit: @MainActor () -> Void

    public init(game: Game, folder: URL, dataDirectory: URL, onExit: @escaping @MainActor () -> Void) {
        self.game = game
        self.folder = folder
        self.dataDirectory = dataDirectory
        self.onExit = onExit
    }
}

/// An engine plugin. Each engine lives in its own module and is registered once in the app's engine list.
public protocol EnginePlugin: Sendable {
    var descriptor: EngineDescriptor { get }
    /// Inspects a folder listing. Returns nil when the folder is not this engine. Must not touch UI state.
    func detect(_ probe: FolderProbe) -> EngineMatch?
    @MainActor func makeSession(_ launch: GameLaunch) throws -> any EngineSession
    /// Where this engine keeps a game's saves on this device. Save sync uses it to restore and back up.
    func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation]
}

extension EnginePlugin {
    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] { [] }
}

/// A directory where an engine keeps saves, plus the rule that recognizes the same saves on other machines.
public struct SaveLocation: Sendable {
    public let directory: URL
    /// Maps an absolute path with forward slashes, as another machine's backup records it, to a path relative to
    /// `directory`. Returns nil when the file is not a save of this kind. The rule also accepts paths inside
    /// `directory` itself, which is how local files are recognized as saves.
    public let relativePath: @Sendable (String) -> String?

    public init(directory: URL, relativePath: @escaping @Sendable (String) -> String?) {
        self.directory = directory
        self.relativePath = relativePath
    }
}

/// A running game. The hub embeds `view` full screen and forwards controller input that the view does not receive natively.
@MainActor
public protocol EngineSession: AnyObject {
    var view: PlatformView { get }
    func send(_ event: ControllerEvent)
    func setPaused(_ paused: Bool)
    func terminate()
    /// Returns once the game has really stopped after `terminate()`, so its saves are final. A game in a helper
    /// process can take a moment to save and quit.
    func waitUntilStopped() async
}

extension EngineSession {
    public func waitUntilStopped() async {}
}

public enum EngineDetector {
    /// Picks the most confident match. Ties go to the plugin registered first.
    public static func detect(_ probe: FolderProbe, using plugins: [any EnginePlugin]) -> EngineMatch? {
        var best: EngineMatch?
        for plugin in plugins {
            guard let match = plugin.detect(probe) else { continue }
            if best == nil || match.confidence > best!.confidence { best = match }
        }
        return best
    }
}
