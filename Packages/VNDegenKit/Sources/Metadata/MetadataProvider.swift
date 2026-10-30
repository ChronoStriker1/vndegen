import Foundation
import VNCore

/// One metadata source. Each parses its API's JSON into these types at the boundary.
protocol MetadataProvider: Sendable {
    var source: MetadataSource { get }
    func search(_ title: String) async throws -> [MetadataCandidate]
    /// Metadata for one entry. `refs` holds this source's ref plus any cross references it knows, such as a Steam app ID.
    func details(_ id: String) async throws -> GameMetadata
    func artwork(_ id: String, slot: ArtworkSlot) async throws -> [ArtworkCandidate]
    /// Translates another source's ref into this source's, for sources indexed by Steam app ID.
    func lookup(_ ref: ExternalRef) async throws -> ExternalRef?
    /// Makes one authenticated request, so the settings screen can confirm the keys work.
    func checkAccess() async throws
}

extension MetadataProvider {
    func lookup(_ ref: ExternalRef) async throws -> ExternalRef? { nil }
    func checkAccess() async throws {}
}

enum MetadataError: LocalizedError, Equatable {
    case http(Int)
    case unexpectedResponse
    case notFound
    case sourceUnavailable(MetadataSource)

    var errorDescription: String? {
        switch self {
        case .http(401), .http(403): "The service rejected the key."
        case .http(429): "Rate limited. Try again in a minute."
        case let .http(status): "The request failed with HTTP \(status)."
        case .unexpectedResponse: "The service sent a response VNDegen does not understand."
        case .notFound: "The service has no entry for this game."
        case let .sourceUnavailable(source): "\(source.displayName) is not set up."
        }
    }
}

extension MetadataSource {
    public var displayName: String {
        switch self {
        case .vndb: "VNDB"
        case .igdb: "IGDB"
        case .steam: "Steam"
        case .steamGridDB: "SteamGridDB"
        }
    }
}

extension GameMetadata {
    /// Keeps every value already present and takes the rest from `other`. Refs from sources not yet present are added.
    func filling(from other: GameMetadata?) -> GameMetadata {
        guard let other else { return self }
        var merged = self
        merged.title = title ?? other.title
        merged.summary = summary ?? other.summary
        merged.developers = developers.isEmpty ? other.developers : developers
        merged.publishers = publishers.isEmpty ? other.publishers : publishers
        merged.releaseDate = releaseDate ?? other.releaseDate
        merged.genres = genres.isEmpty ? other.genres : genres
        merged.rating = rating ?? other.rating
        merged.screenshots = screenshots ?? other.screenshots
        let known = Set(refs.map(\.source))
        merged.refs = refs + other.refs.filter { !known.contains($0.source) }
        return merged
    }
}

/// Fuzzy title comparison for ranking search hits and deciding when a match is safe to apply unattended.
enum TitleMatch {
    static let confidentSimilarity = 0.92

    /// Lowercased, without diacritics or punctuation, words separated by single spaces.
    static func normalize(_ title: String) -> String {
        title.folding(options: [.caseInsensitive, .diacriticInsensitive, .widthInsensitive], locale: nil)
            .map { $0.isLetter || $0.isNumber ? String($0) : " " }
            .joined()
            .split(separator: " ")
            .joined(separator: " ")
    }

    /// 1 for equal normalized titles, falling toward 0 with edit distance.
    static func similarity(_ a: String, _ b: String) -> Double {
        let x = Array(normalize(a)), y = Array(normalize(b))
        let longest = max(x.count, y.count)
        guard longest > 0 else { return 1 }
        var previous = Array(0...y.count)
        for i in x.indices {
            var current = [i + 1] + Array(repeating: 0, count: y.count)
            for j in y.indices {
                current[j + 1] = min(previous[j + 1] + 1, current[j] + 1, previous[j] + (x[i] == y[j] ? 0 : 1))
            }
            previous = current
        }
        return 1 - Double(previous[y.count]) / Double(longest)
    }

    /// Sorts by similarity to `title`. Ties keep source priority, then each source's own order.
    static func rank(_ candidates: [MetadataCandidate], for title: String) -> [MetadataCandidate] {
        candidates.enumerated()
            .map { (offset: $0.offset, score: similarity($0.element.title, title), candidate: $0.element) }
            .sorted {
                if $0.score != $1.score { return $0.score > $1.score }
                let a = searchPriority.firstIndex(of: $0.candidate.ref.source) ?? .max
                let b = searchPriority.firstIndex(of: $1.candidate.ref.source) ?? .max
                return a != b ? a < b : $0.offset < $1.offset
            }
            .map(\.candidate)
    }

    /// The best candidate when it is safe to apply without asking: the same normalized title, or nearly so.
    static func confidentMatch(in candidates: [MetadataCandidate], for title: String) -> MetadataCandidate? {
        rank(candidates, for: title).first { similarity($0.title, title) >= confidentSimilarity }
    }

    /// Which source's entry becomes the primary match when titles tie. VNDB and IGDB have the richest text metadata.
    static let searchPriority: [MetadataSource] = [.vndb, .igdb, .steam, .steamGridDB]
}
