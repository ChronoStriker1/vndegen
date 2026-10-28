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
    /// What the game page says about how a game runs on this device, such as whether it is known to work. Nil when
    /// there is nothing to say.
    func compatibilityNote(for game: Game, folder: URL, dataDirectory: URL) -> String?
    /// Shown before VNDegen backs up a game's saves to Ludusavi, such as when the game's PC version can't load saves
    /// made here. Nil when there is nothing to warn about.
    func backupWarning(for game: Game, dataDirectory: URL) -> String?
    /// Another way to run a game, such as a different Ren'Py runtime, for the game page's advanced options. Nil when
    /// the engine has none on this device.
    func variants(for game: Game, dataDirectory: URL) -> EngineVariants?
    /// `variants(for:dataDirectory:)` for engines that find a game's variants in its folder, such as the movies a Flash
    /// game can start with. The default asks the other.
    func variants(for game: Game, folder: URL, dataDirectory: URL) -> EngineVariants?
    func selectVariant(_ id: String?, for game: Game, dataDirectory: URL) throws
    /// True when the engine's games read touches themselves, as Ren'Py's do. VNDegen shows no touch controls over
    /// them, so a touch device's control method changes nothing for them.
    var readsTouches: Bool { get }
    /// Runtime packages the game needs and doesn't ship, such as the RPG Maker RTP it names, whether or not they are
    /// installed. The game page offers to install the missing ones.
    func runtimePackages(for game: Game, folder: URL) -> [RuntimePackage]
    /// What a button remapped to `button` sent in this engine's games, from versions whose buttons sent
    /// engine-neutral actions, so VNDegen can convert those remaps. Nil means the Xbox button that sent `button` in the
    /// old standard layout, which is right for engines that read a gamepad or turn its buttons into the same keys.
    func legacyAction(for button: VirtualButton) -> ControlAction?
    /// The button layout a game follows until the player changes it. Engines with no gamepad support use the keyboard
    /// and mouse layout.
    var defaultControls: ControlLayout { get }
}

extension EnginePlugin {
    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] { [] }
    public func compatibilityNote(for game: Game, folder: URL, dataDirectory: URL) -> String? { nil }
    public func backupWarning(for game: Game, dataDirectory: URL) -> String? { nil }
    public func variants(for game: Game, dataDirectory: URL) -> EngineVariants? { nil }
    public func variants(for game: Game, folder: URL, dataDirectory: URL) -> EngineVariants? {
        variants(for: game, dataDirectory: dataDirectory)
    }
    public func selectVariant(_ id: String?, for game: Game, dataDirectory: URL) throws {}
    public var readsTouches: Bool { false }
    public func runtimePackages(for game: Game, folder: URL) -> [RuntimePackage] { [] }
    public func legacyAction(for button: VirtualButton) -> ControlAction? { nil }
    public var defaultControls: ControlLayout { .gamepad }
}

/// The ways an engine can run one game, such as the Ren'Py runtimes. Automatic, the engine's own choice, is the
/// default.
public struct EngineVariants: Sendable {
    public struct Option: Sendable, Hashable {
        public let id: String
        public let title: String

        public init(id: String, title: String) {
            self.id = id
            self.title = title
        }
    }

    /// Such as "Ren'Py runtime".
    public let title: String
    public let options: [Option]
    /// Nil while automatic.
    public let selected: String?
    /// What automatic picks for this game, such as "Ren'Py 7, from the game's version".
    public let automatic: String

    public init(title: String, options: [Option], selected: String?, automatic: String) {
        self.title = title
        self.options = options
        self.selected = selected
        self.automatic = automatic
    }
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
    /// True for a game that reads touches, the keyboard, and controllers itself and shows the VNDegen menu inside the
    /// game, as Ren'Py does. The hub then shows no touch controls or menu of its own, and its menu button reaches the
    /// game as a `.hub` press.
    var drawsOwnMenu: Bool { get }
    /// Typing from VNDegen's on-screen keyboard. The default presses the keys for letters, digits, and spaces, and
    /// Return and Backspace, through `send`. Engines that read text, such as for a name the player types, take it whole.
    func type(_ typed: TypedText)
}

extension EngineSession {
    public func waitUntilStopped() async {}
    public var drawsOwnMenu: Bool { false }

    public func type(_ typed: TypedText) {
        let keys: [KeyboardKey] = switch typed {
        case .newline: [KeyboardKey(key: "Enter", code: "Enter", keyCode: 13)]
        case .deleteBackward: [KeyboardKey(key: "Backspace", code: "Backspace", keyCode: 8)]
        case let .text(text): text.compactMap(KeyboardKey.init(typing:))
        }
        for key in keys {
            send(.key(key, pressed: true))
            send(.key(key, pressed: false))
        }
    }
}

extension KeyboardKey {
    /// The key that types `character` on a US keyboard without Shift, for letters, digits, and the space bar.
    init?(typing character: Character) {
        guard let ascii = character.asciiValue else { return nil }
        switch character {
        case "a"..."z", "A"..."Z":
            let upper = Character(character.uppercased())
            self.init(key: String(character).lowercased(), code: "Key\(upper)", keyCode: Int(upper.asciiValue!))
        case "0"..."9": self.init(key: String(character), code: "Digit\(character)", keyCode: Int(ascii))
        case " ": self.init(key: " ", code: "Space", keyCode: 32)
        default: return nil
        }
    }
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
