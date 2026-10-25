import Foundation

/// A game's folder name. Stable across devices, so the same game copied to the Mac and the iPhone keeps one identity.
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

    public init(title: String? = nil, summary: String? = nil, developers: [String] = [], publishers: [String] = [],
                releaseDate: String? = nil, genres: [String] = [], rating: Double? = nil, refs: [ExternalRef] = []) {
        self.title = title
        self.summary = summary
        self.developers = developers
        self.publishers = publishers
        self.releaseDate = releaseDate
        self.genres = genres
        self.rating = rating
        self.refs = refs
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
}
