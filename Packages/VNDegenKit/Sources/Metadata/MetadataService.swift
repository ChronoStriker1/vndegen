import Foundation
import VNCore

/// A search hit the user can confirm as "this is the game".
public struct MetadataCandidate: Identifiable, Hashable, Sendable {
    public var ref: ExternalRef
    public var title: String
    public var year: String?
    public var thumbnail: URL?
    public var id: String { "\(ref.source.rawValue):\(ref.id)" }

    public init(ref: ExternalRef, title: String, year: String? = nil, thumbnail: URL? = nil) {
        self.ref = ref
        self.title = title
        self.year = year
        self.thumbnail = thumbnail
    }
}

/// An image the user can pick for an artwork slot.
public struct ArtworkCandidate: Identifiable, Hashable, Sendable {
    public var slot: ArtworkSlot
    public var source: MetadataSource
    public var url: URL
    public var thumbnail: URL?
    public var id: URL { url }

    public init(slot: ArtworkSlot, source: MetadataSource, url: URL, thumbnail: URL? = nil) {
        self.slot = slot
        self.source = source
        self.url = url
        self.thumbnail = thumbnail
    }
}

/// Entry point the UI uses for metadata and artwork from VNDB, IGDB, Steam, and SteamGridDB.
@MainActor
public final class MetadataService {
    public init() {}

    /// Searches every configured source.
    public func search(_ title: String) async -> [MetadataCandidate] { [] }

    /// Fetches full metadata for the confirmed match, stores it on the game, and fills empty artwork slots.
    public func apply(_ candidate: MetadataCandidate, to id: GameID, in library: LibraryStore) async throws {}

    /// Finds a match by title and applies it, for games that have no metadata yet.
    public func autoMatch(_ id: GameID, in library: LibraryStore) async {}

    public func artworkCandidates(for game: Game, slot: ArtworkSlot) async -> [ArtworkCandidate] { [] }

    /// Downloads the candidate and stores it in the slot.
    public func setArtwork(_ candidate: ArtworkCandidate, for id: GameID, in library: LibraryStore) async throws {}
}
