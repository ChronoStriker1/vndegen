import Foundation
import Synchronization
@testable import Metadata

/// Answers requests from the recorded fixtures in `Fixtures/`. Image URLs answer with their own URL string as the bytes,
/// so a test can tell which candidate ended up in a slot.
final class StubNetwork: Sendable {
    static let steamGridDBKey = "test-key"
    /// A Steam app whose store page answers with a server error.
    static let failingAppID = "500"
    /// An IGDB game that names no Steam app: No Socks RPG's entry without its external games.
    static let igdbOnlyGameID = "1"
    private let log = Mutex<[URLRequest]>([])
    /// Runs before each answer, so a test can change app state while a request is in flight.
    private let beforeAnswer: @Sendable (URLRequest) async -> Void

    init(beforeAnswer: @escaping @Sendable (URLRequest) async -> Void = { _ in }) {
        self.beforeAnswer = beforeAnswer
    }

    var http: HTTP {
        HTTP { [self] request in
            log.withLock { $0.append(request) }
            await beforeAnswer(request)
            let (status, mime, body) = Self.route(request)
            let response = HTTPURLResponse(url: request.url!, statusCode: status, httpVersion: nil,
                                           headerFields: mime.map { ["Content-Type": $0] } ?? [:])!
            return (body, response)
        }
    }

    func requests(to host: String) -> [URLRequest] {
        log.withLock { $0.filter { $0.url?.host == host } }
    }

    static func fixture(_ name: String) -> Data {
        let url = URL(filePath: #filePath).deletingLastPathComponent().appending(path: "Fixtures/\(name).json")
        return try! Data(contentsOf: url)
    }

    private static func route(_ request: URLRequest) -> (Int, String?, Data) {
        let url = request.url!
        let body = request.httpBody.map { String(decoding: $0, as: UTF8.self) } ?? ""
        let query = URLComponents(url: url, resolvingAgainstBaseURL: false)?.queryItems ?? []
        let json = { (name: String) in (200, "application/json", fixture(name)) }
        let missing = (404, "text/html", Data())

        switch url.host! {
        case "api.vndb.org":
            if url.lastPathComponent == "release" { return json("vndb-release-v21905") }
            if body.contains(#""search""#) { return json(body.contains("Doki") ? "vndb-search-ddlc" : "vndb-search-nosocks") }
            return body.contains("v21905") ? json("vndb-vn-v21905") : missing
        case "store.steampowered.com":
            if url.path.contains("storesearch") {
                let term = query.first { $0.name == "term" }?.value ?? ""
                return json(term.contains("Doki") ? "steam-search-ddlc" : "steam-search-nosocks")
            }
            let appID = query.first { $0.name == "appids" }!.value!
            return appID == failingAppID ? (500, nil, Data()) : json("steam-appdetails-\(appID)")
        case "api.steampowered.com":
            let input = query.first { $0.name == "input_json" }!.value!
            return json(input.contains("698780") ? "steam-assets-698780" : "steam-assets-4929970")
        case "www.steamgriddb.com":
            guard request.value(forHTTPHeaderField: "Authorization") == "Bearer \(steamGridDBKey)" else {
                return (401, "application/json", Data(#"{"success":false,"errors":["Invalid key"]}"#.utf8))
            }
            let path = url.path.replacing("/api/v2/", with: "")
            let dimensions = query.first { $0.name == "dimensions" }?.value
            let parts = path.split(separator: "/")
            switch (parts.first, parts.last) {
            case ("search", _): return json("sgdb-search-ddlc")
            case ("games", let id?) where parts[1] == "steam": return json("sgdb-steam-\(id)")
            case ("grids", let id?): return json("sgdb-\(dimensions == "600x900" ? "grids" : "headers")-\(id)")
            case (let kind?, let id?) where ["heroes", "logos", "icons"].contains(kind): return json("sgdb-\(kind)-\(id)")
            default: return missing
            }
        case "id.twitch.tv":
            return json("igdb-token")
        case "api.igdb.com":
            guard request.value(forHTTPHeaderField: "Authorization") == "Bearer fixture-token" else { return (401, nil, Data()) }
            if body.hasPrefix("search") { return json(body.contains("No Socks") ? "igdb-search-nosocks" : "igdb-search-ddlc") }
            if body.contains("id = \(igdbOnlyGameID);") {
                var games = try! JSONSerialization.jsonObject(with: fixture("igdb-game-411478")) as! [[String: Any]]
                games[0]["external_games"] = nil
                return (200, "application/json", try! JSONSerialization.data(withJSONObject: games))
            }
            if body.contains("id = 55935") { return json("igdb-game-55935") }
            if body.contains("id = 411478") { return json("igdb-game-411478") }
            return (200, "application/json", Data("[]".utf8))
        default:
            // Steam's classic logo path is missing for No Socks RPG, as it is live.
            if url.path.hasSuffix("4929970/logo.png") { return missing }
            let mime = url.pathExtension == "png" ? "image/png" : "image/jpeg"
            return (200, mime, request.httpMethod == "HEAD" ? Data() : Data(url.absoluteString.utf8))
        }
    }
}

final class MemoryCredentials: CredentialStore {
    var values: [Credential: String]
    init(_ values: [Credential: String] = [:]) { self.values = values }
    func read(_ credential: Credential) -> String? { values[credential] }
    func write(_ credential: Credential, _ value: String?) throws { values[credential] = value }
}
