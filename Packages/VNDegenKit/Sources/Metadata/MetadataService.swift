import Foundation
import Observation
import OSLog
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


private let log = Logger(subsystem: "com.chronostriker1.vndegen", category: "metadata")

/// Entry point the UI uses for metadata and artwork from VNDB, IGDB, Steam, and SteamGridDB.
@MainActor
@Observable
public final class MetadataService {
    /// Which sources to ask for each slot, best first.
    static let artworkPriority: [ArtworkSlot: [MetadataSource]] = [
        .capsule: [.steamGridDB, .steam, .igdb, .vndb],
        .hero: [.steamGridDB, .steam, .igdb, .vndb],
        .logo: [.steamGridDB, .steam],
        .header: [.steam, .steamGridDB],
        .icon: [.steamGridDB],
    ]
    /// Which sources fill the About section (summary, genres, and screenshots), best first. Each field comes from the
    /// first of these that has it, whichever source the user confirmed.
    static let aboutPriority: [MetadataSource] = [.steam, .igdb]

    @ObservationIgnored private let http: HTTP
    @ObservationIgnored private let credentials: any CredentialStore
    @ObservationIgnored private let keyless: [any MetadataProvider]
    private var providers: [any MetadataProvider] = []

    public convenience init() {
        self.init(credentials: KeychainCredentials(), http: .live())
    }

    init(credentials: any CredentialStore, http: HTTP) {
        self.http = http
        self.credentials = credentials
        keyless = [VNDBProvider(http: http), SteamProvider(http: http)]
        reloadProviders()
    }

    /// Sources that will be queried. VNDB and Steam always are. SteamGridDB and IGDB need keys.
    public var activeSources: [MetadataSource] { providers.map(\.source) }

    /// Searches every configured source.
    public func search(_ title: String) async -> [MetadataCandidate] {
        let providers = providers
        let found = await withTaskGroup(of: [MetadataCandidate].self) { group in
            for provider in providers {
                group.addTask { await Self.attempt(provider.source, "search") { try await provider.search(title) } ?? [] }
            }
            var all: [MetadataCandidate] = []
            for await hits in group { all += hits }
            return all
        }
        return TitleMatch.rank(found, for: title)
    }

    /// Fetches full metadata for the confirmed match, stores it on the game, and fills empty artwork slots.
    public func apply(_ candidate: MetadataCandidate, to id: GameID, in library: LibraryStore) async throws {
        guard let primary = provider(for: candidate.ref.source) else { throw MetadataError.sourceUnavailable(candidate.ref.source) }
        let first = try await primary.details(candidate.ref.id)
        let refs = await resolveRefs(title: first.title ?? candidate.title, known: first.refs)

        // The confirmed source wins every field it has, except the About fields. The others fill gaps in search priority order.
        let others = refs.filter { $0.source != candidate.ref.source }
        var details = await details(others)
        var merged = first
        merged.refs = refs
        for source in TitleMatch.searchPriority { merged = merged.filling(from: details[source]) }
        details[candidate.ref.source] = first
        let asked = Set(others.compactMap { provider(for: $0.source)?.source } + [candidate.ref.source])
        merged = Self.preferringAbout(merged, details: details, asked: asked)

        library.update(id) { game in
            // A title still equal to the folder name was never chosen by anyone.
            if game.title == game.id.rawValue, let title = merged.title { game.title = title }
            game.metadata = merged.filling(from: game.metadata)
        }
        await fillEmptyArtwork(id, refs: merged.refs, in: library)
    }

    /// Finds a match by title and applies it, for games that have no metadata yet.
    /// Only a confident match is applied. Anything less waits for the user to pick.
    public func autoMatch(_ id: GameID, in library: LibraryStore) async {
        guard let game = library.game(id), game.metadata == nil else { return }
        guard let match = TitleMatch.confidentMatch(in: await search(game.title), for: game.title) else { return }
        do {
            try await apply(match, to: id, in: library)
        } catch {
            log.error("auto-match of \(id.rawValue, privacy: .public) failed: \(error.localizedDescription, privacy: .public)")
        }
    }

    /// Brings the About section of a game matched before it took Steam's text and screenshots first up to date.
    /// Does nothing once the game has a screenshot list, even an empty one.
    public func fillAbout(_ id: GameID, in library: LibraryStore) async {
        guard let metadata = library.game(id)?.metadata, metadata.screenshots == nil else { return }
        let refs = metadata.refs.filter { Self.aboutPriority.contains($0.source) && provider(for: $0.source) != nil }
        let details = await details(refs)
        library.update(id) { game in
            guard let current = game.metadata, current.screenshots == nil else { return }
            game.metadata = Self.preferringAbout(current, details: details, asked: Set(refs.map(\.source)))
        }
    }

    public func artworkCandidates(for game: Game, slot: ArtworkSlot) async -> [ArtworkCandidate] {
        let refs = await resolveRefs(title: game.metadata?.title ?? game.title, known: game.metadata?.refs ?? [])
        return await candidates(slot, refs: refs)
    }

    /// Downloads the candidate and stores it in the slot.
    public func setArtwork(_ candidate: ArtworkCandidate, for id: GameID, in library: LibraryStore) async throws {
        try await store(candidate, for: id, in: library, onlyIfEmpty: false)
    }

    // MARK: Keys

    func credential(_ credential: Credential) -> String {
        credentials.read(credential) ?? ""
    }

    /// Stores trimmed values. An empty value deletes the key.
    func setCredentials(_ values: [Credential: String]) throws {
        defer { reloadProviders() }
        for (credential, value) in values {
            let trimmed = value.trimmingCharacters(in: .whitespacesAndNewlines)
            try credentials.write(credential, trimmed.isEmpty ? nil : trimmed)
        }
    }

    func checkAccess(_ source: MetadataSource) async throws {
        guard let provider = provider(for: source) else { throw MetadataError.sourceUnavailable(source) }
        try await provider.checkAccess()
    }

    // MARK: Internals

    private func reloadProviders() {
        var list = keyless
        if let key = credentials.read(.steamGridDBKey) {
            list.append(SteamGridDBProvider(http: http, key: key))
        }
        if let clientID = credentials.read(.igdbClientID), let secret = credentials.read(.igdbClientSecret) {
            list.append(IGDBProvider(http: http, clientID: clientID, clientSecret: secret))
        }
        providers = list
    }

    private func provider(for source: MetadataSource) -> (any MetadataProvider)? {
        providers.first { $0.source == source }
    }

    /// Details from each ref's source, keyed by source. Sources that are not set up or fail are left out.
    private func details(_ refs: [ExternalRef]) async -> [MetadataSource: GameMetadata] {
        await withTaskGroup(of: (MetadataSource, GameMetadata?).self) { group in
            for ref in refs {
                guard let provider = provider(for: ref.source) else { continue }
                group.addTask { (ref.source, await Self.attempt(ref.source, "details") { try await provider.details(ref.id) }) }
            }
            var bySource: [MetadataSource: GameMetadata] = [:]
            for await (source, metadata) in group { bySource[source] = metadata }
            return bySource
        }
    }

    /// Takes each About field from the first source in `aboutPriority` that has it. `details` holds the answers from
    /// the sources in `asked`. When a source was asked and did not answer before one with screenshots, the screenshots
    /// stay unknown so `fillAbout` tries again later.
    private static func preferringAbout(_ metadata: GameMetadata, details: [MetadataSource: GameMetadata],
                                        asked: Set<MetadataSource>) -> GameMetadata {
        let ordered = aboutPriority.filter(asked.contains)
        var result = metadata
        if let summary = ordered.lazy.compactMap({ details[$0]?.summary }).first(where: { !$0.isEmpty }) { result.summary = summary }
        if let genres = ordered.lazy.compactMap({ details[$0]?.genres }).first(where: { !$0.isEmpty }) { result.genres = genres }
        result.screenshots = []
        for source in ordered {
            guard let answer = details[source] else {
                result.screenshots = nil
                break
            }
            if let screenshots = answer.screenshots, !screenshots.isEmpty {
                result.screenshots = screenshots
                break
            }
        }
        return result
    }

    /// `known` plus a ref for every other active source: translated from a known ref where the source supports it,
    /// otherwise its confident title match.
    private func resolveRefs(title: String, known: [ExternalRef]) async -> [ExternalRef] {
        let missing = providers.filter { provider in !known.contains { $0.source == provider.source } }
        let found = await withTaskGroup(of: ExternalRef?.self) { group in
            for provider in missing {
                group.addTask {
                    for ref in known {
                        if let translated = await Self.attempt(provider.source, "lookup", { try await provider.lookup(ref) }) ?? nil {
                            return translated
                        }
                    }
                    let hits = await Self.attempt(provider.source, "search") { try await provider.search(title) } ?? []
                    return TitleMatch.confidentMatch(in: hits, for: title)?.ref
                }
            }
            var refs: [ExternalRef] = []
            for await case let ref? in group { refs.append(ref) }
            return refs
        }
        return known + found
    }

    /// Every candidate for `slot` from sources that know the game, best source first.
    private func candidates(_ slot: ArtworkSlot, refs: [ExternalRef]) async -> [ArtworkCandidate] {
        let jobs: [(Int, any MetadataProvider, String)] = (Self.artworkPriority[slot] ?? []).enumerated().compactMap { rank, source in
            provider(for: source).flatMap { provider in refs.first { $0.source == source }.map { (rank, provider, $0.id) } }
        }
        let results = await withTaskGroup(of: (Int, [ArtworkCandidate]).self) { group in
            for (rank, provider, id) in jobs {
                group.addTask { (rank, await Self.attempt(provider.source, "artwork") { try await provider.artwork(id, slot: slot) } ?? []) }
            }
            var ranked: [(Int, [ArtworkCandidate])] = []
            for await result in group { ranked.append(result) }
            return ranked
        }
        return results.sorted { $0.0 < $1.0 }.flatMap(\.1)
    }

    private func fillEmptyArtwork(_ id: GameID, refs: [ExternalRef], in library: LibraryStore) async {
        for slot in ArtworkSlot.allCases where library.game(id)?.artwork[slot] == nil {
            for candidate in await candidates(slot, refs: refs) {
                do {
                    try await store(candidate, for: id, in: library, onlyIfEmpty: true)
                    break
                } catch {
                    log.error("\(candidate.url.absoluteString, privacy: .public) failed: \(error.localizedDescription, privacy: .public)")
                }
            }
        }
    }

    private func store(_ candidate: ArtworkCandidate, for id: GameID, in library: LibraryStore, onlyIfEmpty: Bool) async throws {
        let image = try await http.image(candidate.url)
        // The user may have picked art for this slot while the download ran.
        guard !onlyIfEmpty || library.game(id)?.artwork[candidate.slot] == nil else { return }
        try library.setArtwork(image.data, fileExtension: image.fileExtension, slot: candidate.slot, for: id)
    }

    /// Runs one source request. A failing source is logged and skipped so the others still answer.
    nonisolated private static func attempt<T: Sendable>(_ source: MetadataSource, _ what: String,
                                                         _ body: @Sendable () async throws -> T) async -> T? {
        do {
            return try await body()
        } catch {
            log.error("\(source.displayName, privacy: .public) \(what, privacy: .public) failed: \(error.localizedDescription, privacy: .public)")
            return nil
        }
    }
}
