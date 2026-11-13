import Foundation

/// A game's identity in the library, and the name of its artwork and data folders. It is the game's folder name, so
/// the same game copied to the Mac and the iPhone keeps one identity, except for a game whose folder name a game in
/// another library folder already has, whose ID adds its library folder's name. `Game.folderName` then holds the
/// folder's own name.
public struct GameID: RawRepresentable, Hashable, Codable, Sendable, CustomStringConvertible {
    public let rawValue: String
    public init(rawValue: String) { self.rawValue = rawValue }
    public init(_ rawValue: String) { self.rawValue = rawValue }
    public var description: String { rawValue }
}

/// Steam-style artwork slots. Sizes are the Steam library sizes, used for aspect ratios and fetch targets.
public enum ArtworkSlot: String, CaseIterable, Codable, CodingKeyRepresentable, Sendable {
    /// Portrait library tile, 600x900.
    case capsule
    /// Wide banner behind the game page, 3840x1240.
    case hero
    /// Transparent title logo drawn over the hero.
    case logo
    /// Landscape tile, 920x430.
    case header
    /// Square icon.
    case icon

    public var nominalSize: CGSize {
        switch self {
        case .capsule: CGSize(width: 600, height: 900)
        case .hero: CGSize(width: 3840, height: 1240)
        case .logo: CGSize(width: 1280, height: 720)
        case .header: CGSize(width: 920, height: 430)
        case .icon: CGSize(width: 256, height: 256)
        }
    }
}

public enum MetadataSource: String, Codable, CaseIterable, Sendable {
    case vndb, igdb, steam, steamGridDB
}

public struct ExternalRef: Codable, Hashable, Sendable {
    public var source: MetadataSource
    public var id: String
    public var url: URL?

    public init(source: MetadataSource, id: String, url: URL? = nil) {
        self.source = source
        self.id = id
        self.url = url
    }
}

public struct Screenshot: Codable, Hashable, Sendable {
    public var url: URL
    public var thumbnail: URL

    public init(url: URL, thumbnail: URL) {
        self.url = url
        self.thumbnail = thumbnail
    }
}

public struct GameMetadata: Codable, Hashable, Sendable {
    public var title: String?
    public var summary: String?
    public var developers: [String]
    public var publishers: [String]
    /// ISO 8601 date, possibly partial ("2024" or "2024-03").
    public var releaseDate: String?
    public var genres: [String]
    /// 0 to 100.
    public var rating: Double?
    public var refs: [ExternalRef]
    /// Nil until a lookup finishes, so games matched before screenshots existed fetch them once.
    public var screenshots: [Screenshot]?

    public init(title: String? = nil, summary: String? = nil, developers: [String] = [], publishers: [String] = [],
                releaseDate: String? = nil, genres: [String] = [], rating: Double? = nil, refs: [ExternalRef] = [],
                screenshots: [Screenshot]? = nil) {
        self.title = title
        self.summary = summary
        self.developers = developers
        self.publishers = publishers
        self.releaseDate = releaseDate
        self.genres = genres
        self.rating = rating
        self.refs = refs
        self.screenshots = screenshots
    }

    public init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        title = container.lenient(String.self, forKey: .title)
        summary = container.lenient(String.self, forKey: .summary)
        developers = container.lenient([String].self, forKey: .developers) ?? []
        publishers = container.lenient([String].self, forKey: .publishers) ?? []
        releaseDate = container.lenient(String.self, forKey: .releaseDate)
        genres = container.lenient([String].self, forKey: .genres) ?? []
        rating = container.lenient(Double.self, forKey: .rating)
        refs = container.lenient([Lossy<ExternalRef>].self, forKey: .refs)?.compactMap(\.value) ?? []
        screenshots = container.lenient([Lossy<Screenshot>].self, forKey: .screenshots)?.compactMap(\.value)
    }
}

/// How the interface is oriented while a game plays on iPhone and iPad.
public enum PlayOrientation: String, Codable, CaseIterable, Sendable {
    case landscape, portrait, followDevice
}

public struct Game: Identifiable, Codable, Hashable, Sendable {
    public let id: GameID
    /// Display title. Starts as the folder name. The user or metadata can change it.
    public var title: String
    public var detected: EngineMatch?
    /// Set when the user picks an engine by hand. Wins over detection.
    public var engineOverride: EngineID?
    public var metadata: GameMetadata?
    /// Artwork file names inside the game's artwork directory.
    public var artwork: [ArtworkSlot: String]
    /// Locked games stay hidden until the user unlocks the library with Face ID, Touch ID, or the passcode.
    public var isLocked: Bool
    public var addedAt: Date
    public var lastPlayedAt: Date?
    public var playTime: TimeInterval
    /// Overrides the app's in-game orientation setting for this game. Nil follows the setting.
    public var orientation: PlayOrientation?
    /// Overrides the app's control method setting for this game. Nil follows the setting.
    public var controlMethod: ControlMethod?
    /// Buttons the player remapped for this game, each against the Xbox layout, where every button sends its own Xbox
    /// button. Buttons missing here keep that layout. Nil follows the engine's default layout instead, so an empty
    /// mapping is the plain Xbox layout in an engine whose default is another.
    public var buttonMapping: [ControlInput: ControlAction]?
    /// The cheats that stay on for this game. Nil while the player has never changed one.
    public var cheats: CheatSettings?
    /// The added library folder that holds the game. Nil is the built-in games folder.
    public var libraryFolder: LibraryFolderID?
    /// The game's folder name, when it isn't its ID because another library folder has a game of the same name.
    public var folderName: String?

    public var engine: EngineID? { engineOverride ?? detected?.engine }

    public init(id: GameID, title: String? = nil, addedAt: Date = .now) {
        self.id = id
        self.title = title ?? id.rawValue
        self.detected = nil
        self.engineOverride = nil
        self.metadata = nil
        self.artwork = [:]
        self.isLocked = false
        self.addedAt = addedAt
        self.lastPlayedAt = nil
        self.playTime = 0
    }

    public init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        id = try container.decode(GameID.self, forKey: .id)
        title = container.lenient(String.self, forKey: .title) ?? id.rawValue
        detected = container.lenient(EngineMatch.self, forKey: .detected)
        engineOverride = container.lenient(EngineID.self, forKey: .engineOverride)
        metadata = container.lenient(GameMetadata.self, forKey: .metadata)
        artwork = container.lenient([ArtworkSlot: String].self, forKey: .artwork)
            ?? Self.known(container.lenient([String: Lossy<String>].self, forKey: .artwork), as: ArtworkSlot.self)
            ?? [:]
        isLocked = container.lenient(Bool.self, forKey: .isLocked) ?? false
        addedAt = container.lenient(Date.self, forKey: .addedAt) ?? .now
        lastPlayedAt = container.lenient(Date.self, forKey: .lastPlayedAt)
        playTime = container.lenient(TimeInterval.self, forKey: .playTime) ?? 0
        orientation = container.lenient(PlayOrientation.self, forKey: .orientation)
        controlMethod = container.lenient(ControlMethod.self, forKey: .controlMethod)
        buttonMapping = container.lenient([ControlInput: ControlAction].self, forKey: .buttonMapping)
            ?? Self.known(container.lenient([String: Lossy<ControlAction>].self, forKey: .buttonMapping), as: ControlInput.self)
        cheats = container.lenient(CheatSettings.self, forKey: .cheats)
        libraryFolder = container.lenient(LibraryFolderID.self, forKey: .libraryFolder)
        folderName = container.lenient(String.self, forKey: .folderName)
    }

    private static func known<Key: RawRepresentable<String> & Hashable, Value>(_ entries: [String: Lossy<Value>]?,
                                                                               as _: Key.Type) -> [Key: Value]? {
        guard let entries else { return nil }
        return Dictionary(uniqueKeysWithValues: entries.compactMap { key, entry in
            Key(rawValue: key).flatMap { key in entry.value.map { (key, $0) } }
        })
    }
}

struct Lossy<Value: Decodable>: Decodable {
    let value: Value?

    init(from decoder: any Decoder) throws {
        value = try? decoder.singleValueContainer().decode(Value.self)
    }
}

extension KeyedDecodingContainer {
    func lenient<Value: Decodable>(_ type: Value.Type, forKey key: Key) -> Value? {
        try? decodeIfPresent(type, forKey: key)
    }
}
