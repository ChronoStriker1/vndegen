import Foundation
import VNCore

/// IGDB API v4. Authenticates with the user's Twitch client ID and secret through an app access token.
struct IGDBProvider: MetadataProvider {
    private static let api = URL(string: "https://api.igdb.com/v4/")!
    private static let gameFields = "fields name,summary,url,first_release_date,total_rating,genres.name,"
        + "involved_companies.developer,involved_companies.publisher,involved_companies.company.name,"
        + "external_games.external_game_source,external_games.uid,cover.image_id,artworks.image_id;"
    /// IGDB's external game source for Steam.
    private static let steamSource = 1

    let http: HTTP
    let auth: TwitchAuth
    var source: MetadataSource { .igdb }

    init(http: HTTP, clientID: String, clientSecret: String) {
        self.http = http
        auth = TwitchAuth(http: http, clientID: clientID, clientSecret: clientSecret)
    }

    func search(_ title: String) async throws -> [MetadataCandidate] {
        let escaped = title.replacing("\\", with: "\\\\").replacing("\"", with: "\\\"")
        let games = try await query("search \"\(escaped)\"; fields name,first_release_date,cover.image_id; limit 10;")
        return games.map { game in
            MetadataCandidate(ref: ref(game), title: game.name, year: game.releaseDate.map { String($0.prefix(4)) },
                              thumbnail: game.cover.map { Self.image($0.image_id, size: "t_cover_small") })
        }
    }

    func details(_ id: String) async throws -> GameMetadata {
        let game = try await game(id)
        let companies = game.involved_companies ?? []
        let steamID = game.external_games?.first { $0.external_game_source == Self.steamSource }?.uid
        return GameMetadata(
            title: game.name,
            summary: game.summary,
            developers: companies.filter(\.developer).map(\.company.name),
            publishers: companies.filter(\.publisher).map(\.company.name),
            releaseDate: game.releaseDate,
            genres: (game.genres ?? []).map(\.name),
            rating: game.total_rating,
            refs: [ref(game)] + (steamID.map { [SteamProvider.ref($0)] } ?? [])
        )
    }

    func artwork(_ id: String, slot: ArtworkSlot) async throws -> [ArtworkCandidate] {
        let game = try await game(id)
        let images: [(String, String, String)] = switch slot {
        case .capsule: game.cover.map { [($0.image_id, "t_cover_big_2x", "t_cover_small")] } ?? []
        case .hero: (game.artworks ?? []).map { ($0.image_id, "t_1080p", "t_screenshot_med") }
        case .logo, .header, .icon: []
        }
        return images.map { imageID, size, thumbnail in
            ArtworkCandidate(slot: slot, source: .igdb, url: Self.image(imageID, size: size), thumbnail: Self.image(imageID, size: thumbnail))
        }
    }

    func checkAccess() async throws {
        _ = try await query("fields name; limit 1;", memoize: false, freshToken: true)
    }

    private func game(_ id: String) async throws -> IGDBGame {
        guard let number = Int(id) else { throw MetadataError.notFound }
        guard let game = try await query("\(Self.gameFields) where id = \(number);").first else { throw MetadataError.notFound }
        return game
    }

    private func query(_ body: String, memoize: Bool = true, freshToken: Bool = false) async throws -> [IGDBGame] {
        var token = try await auth.token(refresh: freshToken)
        for attempt in 0..<2 {
            let request = URLRequest.post(Self.api.appendingPathComponent("games"), body: Data(body.utf8), contentType: "text/plain",
                                          headers: ["Client-ID": auth.clientID, "Authorization": "Bearer \(token)"])
            let response = try await http.response(request, memoize: memoize)
            // A revoked or expired token gets one retry with a new one.
            if response.status == 401, attempt == 0 {
                token = try await auth.token(refresh: true)
                continue
            }
            guard response.isSuccess else { throw MetadataError.http(response.status) }
            return try JSONDecoder().decode([IGDBGame].self, from: response.data)
        }
        throw MetadataError.http(401)
    }

    private func ref(_ game: IGDBGame) -> ExternalRef {
        ExternalRef(source: .igdb, id: String(game.id), url: game.url)
    }

    private static func image(_ id: String, size: String) -> URL {
        URL(string: "https://images.igdb.com/igdb/image/upload/\(size)/\(id).jpg")!
    }
}

/// Twitch app access token for IGDB, fetched once and reused until shortly before it expires.
actor TwitchAuth {
    nonisolated let clientID: String
    private let clientSecret: String
    private let http: HTTP
    private var current: Task<(token: String, expires: Date), any Error>?

    init(http: HTTP, clientID: String, clientSecret: String) {
        self.http = http
        self.clientID = clientID
        self.clientSecret = clientSecret
    }

    func token(refresh: Bool = false) async throws -> String {
        if !refresh, let current, let cached = try? await current.value, cached.expires > .now {
            return cached.token
        }
        let http = http, clientID = clientID, clientSecret = clientSecret
        let task = Task<(token: String, expires: Date), any Error> {
            var url = URLComponents(string: "https://id.twitch.tv/oauth2/token")!
            url.queryItems = [.init(name: "client_id", value: clientID), .init(name: "client_secret", value: clientSecret),
                              .init(name: "grant_type", value: "client_credentials")]
            let body = Data((url.percentEncodedQuery ?? "").utf8)
            url.queryItems = nil
            let request = URLRequest.post(url.url!, body: body, contentType: "application/x-www-form-urlencoded")
            let grant = try await http.decode(TokenGrant.self, request, memoize: false)
            return (grant.access_token, Date.now.addingTimeInterval(grant.expires_in - 60))
        }
        current = task
        return try await task.value.token
    }
}

private struct TokenGrant: Decodable {
    var access_token: String
    var expires_in: TimeInterval
}

// Property names follow IGDB's JSON.
private struct IGDBGame: Decodable {
    struct Image: Decodable { var image_id: String }
    struct Named: Decodable { var name: String }
    struct Involvement: Decodable {
        var company: Named
        var developer: Bool
        var publisher: Bool
    }
    struct External: Decodable {
        var external_game_source: Int?
        var uid: String?
    }

    var id: Int
    var name: String
    var summary: String?
    var url: URL?
    var first_release_date: TimeInterval?
    var total_rating: Double?
    var genres: [Named]?
    var involved_companies: [Involvement]?
    var external_games: [External]?
    var cover: Image?
    var artworks: [Image]?

    var releaseDate: String? {
        first_release_date.map { Date(timeIntervalSince1970: $0).formatted(.iso8601.year().month().day()) }
    }
}
