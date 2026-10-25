import Foundation
import VNCore

/// SteamGridDB API v2, with the user's bearer key. Artwork only. Games are found by Steam app ID when one is known.
struct SteamGridDBProvider: MetadataProvider {
    private static let api = "https://www.steamgriddb.com/api/v2/"

    let http: HTTP
    let key: String
    var source: MetadataSource { .steamGridDB }

    func search(_ title: String) async throws -> [MetadataCandidate] {
        let term = title.addingPercentEncoding(withAllowedCharacters: .alphanumerics) ?? title
        let games: [GridGame] = try await get("search/autocomplete/\(term)")
        return games.map { MetadataCandidate(ref: ref($0.id), title: $0.name, year: $0.year) }
    }

    func details(_ id: String) async throws -> GameMetadata {
        guard let gameID = Int(id) else { throw MetadataError.notFound }
        let game: GridGame = try await get("games/id/\(gameID)")
        return GameMetadata(title: game.name, refs: [ref(game.id)])
    }

    func lookup(_ ref: ExternalRef) async throws -> ExternalRef? {
        guard ref.source == .steam, let appID = Int(ref.id) else { return nil }
        do {
            let game: GridGame = try await get("games/steam/\(appID)")
            return self.ref(game.id)
        } catch MetadataError.http(404) {
            return nil
        }
    }

    func artwork(_ id: String, slot: ArtworkSlot) async throws -> [ArtworkCandidate] {
        guard let gameID = Int(id) else { throw MetadataError.notFound }
        let path = switch slot {
        case .capsule: "grids/game/\(gameID)?dimensions=600x900"
        case .header: "grids/game/\(gameID)?dimensions=920x430,460x215"
        case .hero: "heroes/game/\(gameID)"
        case .logo: "logos/game/\(gameID)"
        case .icon: "icons/game/\(gameID)"
        }
        let images: [GridImage] = try await get(path)
        return images.map { image in
            // Many icons are .ico files. Their thumbnail is a 512 px PNG render, which every platform can draw.
            let url = image.mime == "image/vnd.microsoft.icon" ? image.thumb ?? image.url : image.url
            return ArtworkCandidate(slot: slot, source: .steamGridDB, url: url, thumbnail: image.thumb)
        }
    }

    func checkAccess() async throws {
        _ = try await http.data(request("search/autocomplete/test"), memoize: false)
    }

    private func get<T: Decodable>(_ path: String) async throws -> T {
        try await http.decode(Envelope<T>.self, request(path)).data
    }

    private func request(_ path: String) -> URLRequest {
        .get(URL(string: Self.api + path)!, headers: ["Authorization": "Bearer \(key)"])
    }

    private func ref(_ id: Int) -> ExternalRef {
        ExternalRef(source: .steamGridDB, id: String(id), url: URL(string: "https://www.steamgriddb.com/game/\(id)"))
    }
}

private struct Envelope<T: Decodable>: Decodable {
    var data: T
}

private struct GridGame: Decodable {
    var id: Int
    var name: String
    var release_date: TimeInterval?

    var year: String? {
        release_date.map { Date(timeIntervalSince1970: $0).formatted(.iso8601.year()) }
    }
}

private struct GridImage: Decodable {
    var url: URL
    var thumb: URL?
    var mime: String?
}
