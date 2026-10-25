import Foundation
import VNCore

/// VNDB's Kana API. No key. The documented limit is 200 requests per 5 minutes, so requests go out 1.5 s apart.
struct VNDBProvider: MetadataProvider {
    private static let api = URL(string: "https://api.vndb.org/kana/")!
    private static let detailFields = "title,description,released,rating,image.url,image.thumbnail,image.sexual,"
        + "developers.name,tags.name,tags.rating,tags.category,tags.spoiler,screenshots.url,screenshots.thumbnail,screenshots.sexual"

    private let http: HTTP
    var source: MetadataSource { .vndb }

    init(http: HTTP) {
        self.http = http.paced(by: Pacer(interval: .milliseconds(1500)))
    }

    func search(_ title: String) async throws -> [MetadataCandidate] {
        let page: Page<VN> = try await query("vn", ["filters": ["search", "=", title], "fields": "title,released,image.thumbnail", "results": 10])
        return page.results.map { vn in
            MetadataCandidate(ref: ref(vn.id), title: vn.title, year: vn.released.flatMap(Self.year), thumbnail: vn.image?.thumbnail)
        }
    }

    func details(_ id: String) async throws -> GameMetadata {
        let vn = try await entry(id)
        let releases: Page<Release> = try await query("release", [
            "filters": ["and", ["vn", "=", ["id", "=", id]], ["extlink", "=", "steam"]],
            "fields": "extlinks.name,extlinks.id",
            "results": 10,
        ])
        let steamID = releases.results.lazy.flatMap(\.extlinks).first { $0.name == "steam" }?.id.value
        let genres = (vn.tags ?? [])
            .filter { $0.category == "cont" && $0.spoiler == 0 && $0.rating >= 2 }
            .sorted { $0.rating > $1.rating }
            .prefix(5)
            .map(\.name)
        return GameMetadata(
            title: vn.title,
            summary: vn.description.map(Self.plainText),
            developers: (vn.developers ?? []).map(\.name),
            releaseDate: vn.released.flatMap { Self.year($0) == nil ? nil : $0 },
            genres: genres,
            rating: vn.rating,
            refs: [ref(vn.id)] + (steamID.map { [SteamProvider.ref($0)] } ?? [])
        )
    }

    func artwork(_ id: String, slot: ArtworkSlot) async throws -> [ArtworkCandidate] {
        let vn = try await entry(id)
        switch slot {
        case .capsule:
            guard let image = vn.image, let url = image.url else { return [] }
            return [ArtworkCandidate(slot: slot, source: .vndb, url: url, thumbnail: image.thumbnail)]
        case .hero:
            // Screenshots are the only wide images VNDB has. Explicit ones stay out of the picker.
            return (vn.screenshots ?? []).filter { $0.sexual < 0.5 }
                .map { ArtworkCandidate(slot: slot, source: .vndb, url: $0.url, thumbnail: $0.thumbnail) }
        case .logo, .header, .icon:
            return []
        }
    }

    private func entry(_ id: String) async throws -> VN {
        let page: Page<VN> = try await query("vn", ["filters": ["id", "=", id], "fields": Self.detailFields])
        guard let vn = page.results.first else { throw MetadataError.notFound }
        return vn
    }

    private func query<T: Decodable>(_ endpoint: String, _ body: [String: Any]) async throws -> T {
        let data = try JSONSerialization.data(withJSONObject: body, options: .sortedKeys)
        return try await http.decode(T.self, .post(Self.api.appendingPathComponent(endpoint), body: data, contentType: "application/json"))
    }

    private func ref(_ id: String) -> ExternalRef {
        ExternalRef(source: .vndb, id: id, url: URL(string: "https://vndb.org/\(id)"))
    }

    /// "2017-09-22", "2017-09", or "2017". VNDB also returns "TBA".
    static func year(_ released: String) -> String? {
        let year = released.prefix(4)
        return year.count == 4 && year.allSatisfy(\.isNumber) ? String(year) : nil
    }

    /// Drops VNDB's BBCode-style markup, such as `[url=...]` and `[spoiler]`, keeping the text inside.
    static func plainText(_ description: String) -> String {
        let markup = try! Regex(#"\[/?[a-zA-Z]+(=[^\]]*)?\]"#)
        return description.replacing(markup, with: "").trimmingCharacters(in: .whitespacesAndNewlines)
    }
}

private struct Page<T: Decodable>: Decodable {
    var results: [T]
}

private struct VN: Decodable {
    struct Image: Decodable {
        /// Absent in search results, which only ask for the thumbnail.
        var url: URL?
        var thumbnail: URL?
        var sexual: Double?
    }
    struct Named: Decodable { var name: String }
    struct Tag: Decodable {
        var name: String
        var rating: Double
        var category: String
        var spoiler: Int
    }
    struct Screenshot: Decodable {
        var url: URL
        var thumbnail: URL?
        var sexual: Double
    }

    var id: String
    var title: String
    var released: String?
    var description: String?
    var rating: Double?
    var image: Image?
    var developers: [Named]?
    var tags: [Tag]?
    var screenshots: [Screenshot]?
}

private struct Release: Decodable {
    struct Link: Decodable {
        var name: String
        var id: LooseID
    }
    var extlinks: [Link]
}

/// VNDB sends link IDs as strings or numbers depending on the site.
private struct LooseID: Decodable {
    var value: String

    init(from decoder: any Decoder) throws {
        let container = try decoder.singleValueContainer()
        value = try (try? container.decode(String.self)) ?? String(container.decode(Int.self))
    }
}
