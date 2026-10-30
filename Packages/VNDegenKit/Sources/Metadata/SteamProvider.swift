import Foundation
import VNCore

/// The Steam store. No key. Metadata comes from appdetails. Library art comes from IStoreBrowseService, because newer
/// apps keep their images under hashed folders that the classic `steam/apps/<id>/library_hero.jpg` paths miss.
struct SteamProvider: MetadataProvider {
    private static let assetRoot = "https://shared.akamai.steamstatic.com/store_item_assets/"

    let http: HTTP
    var source: MetadataSource { .steam }

    static func ref(_ appID: String) -> ExternalRef {
        ExternalRef(source: .steam, id: appID, url: URL(string: "https://store.steampowered.com/app/\(appID)"))
    }

    func search(_ title: String) async throws -> [MetadataCandidate] {
        var url = URLComponents(string: "https://store.steampowered.com/api/storesearch/")!
        url.queryItems = [.init(name: "term", value: title), .init(name: "cc", value: "us"), .init(name: "l", value: "english")]
        let result = try await http.decode(SearchResult.self, .get(url.url!))
        return result.items.filter { $0.type == "app" }.map { item in
            MetadataCandidate(ref: Self.ref(String(item.id)), title: item.name, thumbnail: item.tiny_image)
        }
    }

    func details(_ id: String) async throws -> GameMetadata {
        var url = URLComponents(string: "https://store.steampowered.com/api/appdetails")!
        url.queryItems = [.init(name: "appids", value: id), .init(name: "cc", value: "us"), .init(name: "l", value: "english")]
        let envelope = try await http.decode([String: AppDetails].self, .get(url.url!))
        guard let app = envelope[id], app.success, let data = app.data else { throw MetadataError.notFound }
        return GameMetadata(
            title: data.name,
            summary: data.short_description.map(Self.decodeEntities),
            developers: data.developers ?? [],
            publishers: data.publishers ?? [],
            releaseDate: data.release_date?.date.flatMap(Self.isoDate),
            genres: (data.genres ?? []).map(\.description),
            rating: data.metacritic.map { Double($0.score) },
            refs: [Self.ref(id)],
            screenshots: (data.screenshots ?? []).map { Screenshot(url: $0.path_full, thumbnail: $0.path_thumbnail) }
        )
    }

    func artwork(_ id: String, slot: ArtworkSlot) async throws -> [ArtworkCandidate] {
        guard slot != .icon else { return [] }
        guard let appID = Int(id) else { throw MetadataError.notFound }
        var url = URLComponents(string: "https://api.steampowered.com/IStoreBrowseService/GetItems/v1/")!
        let input = #"{"ids":[{"appid":\#(appID)}],"context":{"language":"english","country_code":"US"},"data_request":{"include_assets":true}}"#
        url.queryItems = [.init(name: "input_json", value: input)]
        let result = try await http.decode(StoreBrowse.self, .get(url.url!))
        guard let assets = result.response.store_items?.first?.assets else { return [] }

        let (files, thumbnail): ([String?], String?) = switch slot {
        case .capsule: ([assets.library_capsule_2x, assets.library_capsule], assets.library_capsule)
        case .hero: ([assets.library_hero_2x, assets.library_hero], assets.library_hero)
        case .header: ([assets.header_2x, assets.header], assets.header)
        // The asset list has no logo, but most apps still serve one at the classic path.
        case .logo: (["logo.png"], nil)
        case .icon: ([], nil)
        }
        let image = { (file: String) in URL(string: Self.assetRoot + assets.asset_url_format.replacing("${FILENAME}", with: file)) }
        for url in files.compactMap({ $0 }).compactMap(image) {
            if await http.exists(url) {
                return [ArtworkCandidate(slot: slot, source: .steam, url: url, thumbnail: thumbnail.flatMap(image))]
            }
        }
        return []
    }

    /// Steam formats US store dates as "Sep 22, 2017".
    static func isoDate(_ text: String) -> String? {
        let formatter = DateFormatter()
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.timeZone = .gmt
        for format in ["MMM d, yyyy", "d MMM, yyyy"] {
            formatter.dateFormat = format
            if let date = formatter.date(from: text) { return date.formatted(.iso8601.year().month().day()) }
        }
        return nil
    }

    static func decodeEntities(_ text: String) -> String {
        [("&quot;", "\""), ("&#39;", "'"), ("&lt;", "<"), ("&gt;", ">"), ("&amp;", "&")]
            .reduce(text) { $0.replacing($1.0, with: $1.1) }
    }
}

// Property names follow Steam's JSON.
private struct SearchResult: Decodable {
    struct Item: Decodable {
        var type: String
        var name: String
        var id: Int
        var tiny_image: URL?
    }
    var items: [Item]
}

private struct AppDetails: Decodable {
    struct App: Decodable {
        struct Release: Decodable { var date: String? }
        struct Genre: Decodable { var description: String }
        struct Metacritic: Decodable { var score: Int }
        struct Shot: Decodable {
            var path_thumbnail: URL
            var path_full: URL
        }
        var name: String
        var short_description: String?
        var developers: [String]?
        var publishers: [String]?
        var release_date: Release?
        var genres: [Genre]?
        var metacritic: Metacritic?
        var screenshots: [Shot]?
    }
    var success: Bool
    var data: App?
}

private struct StoreBrowse: Decodable {
    struct Response: Decodable { var store_items: [Item]? }
    struct Item: Decodable { var assets: Assets? }
    struct Assets: Decodable {
        var asset_url_format: String
        var header: String?
        var header_2x: String?
        var library_capsule: String?
        var library_capsule_2x: String?
        var library_hero: String?
        var library_hero_2x: String?
    }
    var response: Response
}
