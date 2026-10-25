import Foundation

/// Network access shared by every source.
/// API responses are memoized in memory for half an hour so browsing candidates does not refetch them.
/// Image downloads skip the memo and go through the session's disk URLCache instead.
struct HTTP: Sendable {
    typealias Send = @Sendable (URLRequest) async throws -> (Data, URLResponse)

    struct Response: Sendable {
        var status: Int
        var data: Data
        var mimeType: String?
        var isSuccess: Bool { (200..<300).contains(status) }
    }

    private let send: Send
    private let memo = ResponseMemo()

    init(send: @escaping Send) { self.send = send }

    static func live() -> HTTP {
        let config = URLSessionConfiguration.default
        let caches = FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask)[0]
        config.urlCache = URLCache(memoryCapacity: 32 << 20, diskCapacity: 256 << 20,
                                   directory: caches.appendingPathComponent("VNDegenMetadata", isDirectory: true))
        config.timeoutIntervalForRequest = 20
        let session = URLSession(configuration: config)
        return HTTP { try await session.data(for: $0) }
    }

    /// A client that waits for `pacer` before each request that misses the memo.
    func paced(by pacer: Pacer) -> HTTP {
        let send = send
        return HTTP { request in
            try await pacer.wait()
            return try await send(request)
        }
    }

    func response(_ request: URLRequest, memoize: Bool = true) async throws -> Response {
        let key = memoize ? ResponseMemo.key(request) : nil
        if let key, let hit = await memo.lookup(key) { return hit }
        let (data, raw) = try await send(request)
        guard let http = raw as? HTTPURLResponse else { throw MetadataError.unexpectedResponse }
        let response = Response(status: http.statusCode, data: data, mimeType: http.mimeType)
        if let key, response.isSuccess || response.status == 404 { await memo.store(response, for: key) }
        return response
    }

    func data(_ request: URLRequest, memoize: Bool = true) async throws -> Data {
        let response = try await response(request, memoize: memoize)
        guard response.isSuccess else { throw MetadataError.http(response.status) }
        return response.data
    }

    func decode<T: Decodable>(_ type: T.Type, _ request: URLRequest, memoize: Bool = true) async throws -> T {
        try await JSONDecoder().decode(T.self, from: data(request, memoize: memoize))
    }

    /// HEAD check, for CDNs that only have some images for some games.
    func exists(_ url: URL) async -> Bool {
        var request = URLRequest(url: url)
        request.httpMethod = "HEAD"
        return (try? await response(request).isSuccess) ?? false
    }

    /// Downloads an image and returns its bytes and a file extension for storing it.
    func image(_ url: URL) async throws -> (data: Data, fileExtension: String) {
        let response = try await response(URLRequest(url: url), memoize: false)
        guard response.isSuccess else { throw MetadataError.http(response.status) }
        guard let mime = response.mimeType, mime.hasPrefix("image/") else { throw MetadataError.unexpectedResponse }
        let ext = switch mime {
        case "image/jpeg": "jpg"
        case "image/png": "png"
        case "image/webp": "webp"
        default: url.pathExtension.isEmpty ? String(mime.dropFirst("image/".count)) : url.pathExtension
        }
        return (response.data, ext)
    }
}

extension URLRequest {
    static func post(_ url: URL, body: Data, contentType: String, headers: [String: String] = [:]) -> URLRequest {
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.httpBody = body
        request.setValue(contentType, forHTTPHeaderField: "Content-Type")
        for (name, value) in headers { request.setValue(value, forHTTPHeaderField: name) }
        return request
    }

    static func get(_ url: URL, headers: [String: String] = [:]) -> URLRequest {
        var request = URLRequest(url: url)
        for (name, value) in headers { request.setValue(value, forHTTPHeaderField: name) }
        return request
    }
}

actor ResponseMemo {
    private static let lifetime: TimeInterval = 30 * 60
    private var entries: [String: (response: HTTP.Response, expires: Date)] = [:]

    static func key(_ request: URLRequest) -> String {
        let body = request.httpBody.map { String(decoding: $0, as: UTF8.self) } ?? ""
        let auth = request.value(forHTTPHeaderField: "Authorization") ?? ""
        return "\(request.httpMethod ?? "GET") \(request.url?.absoluteString ?? "") \(auth) \(body)"
    }

    func lookup(_ key: String) -> HTTP.Response? {
        guard let entry = entries[key], entry.expires > .now else { return nil }
        return entry.response
    }

    func store(_ response: HTTP.Response, for key: String) {
        entries[key] = (response, Date.now.addingTimeInterval(Self.lifetime))
    }
}

/// Spaces requests to one API at least `interval` apart.
actor Pacer {
    private let interval: Duration
    private var next = ContinuousClock.now

    init(interval: Duration) { self.interval = interval }

    func wait() async throws {
        let slot = max(next, ContinuousClock.now)
        next = slot + interval
        try await Task.sleep(until: slot, clock: .continuous)
    }
}
