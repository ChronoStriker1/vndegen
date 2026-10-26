import Foundation
import OSLog

private let log = Logger(subsystem: "com.chronostriker1.vndegen", category: "savesync")

/// Ludusavi's primary manifest, fetched from the address Ludusavi uses at most once a day, and only when its ETag
/// changed. The reduced index is kept on disk, so matching works offline after the first fetch.
actor ManifestStore {
    private let source: URL
    private let directory: URL
    private var index: ManifestIndex?
    private var loaded = false

    private struct State: Codable {
        var etag: String?
        var checked: Date
    }

    init(source: URL, directory: URL) {
        self.source = source
        self.directory = directory
    }

    private var indexFile: URL { directory.appendingPathComponent("manifest-index.json") }
    private var stateFile: URL { directory.appendingPathComponent("manifest-state.json") }

    /// The index on disk, read once.
    func current() -> ManifestIndex? {
        if !loaded {
            loaded = true
            index = try? JSONDecoder().decode(ManifestIndex.self, from: Data(contentsOf: indexFile))
        }
        return index
    }

    /// Fetches the manifest when the last check is a day old and its ETag changed. Failures keep the index on disk.
    func refresh(now: Date = .now) async -> ManifestIndex? {
        let state = try? JSONDecoder().decode(State.self, from: Data(contentsOf: stateFile))
        if let state, now.timeIntervalSince(state.checked) < 24 * 3600, current() != nil { return index }
        var request = URLRequest(url: source)
        if current() != nil, let etag = state?.etag { request.setValue(etag, forHTTPHeaderField: "If-None-Match") }
        do {
            let (data, response) = try await URLSession.shared.data(for: request)
            let status = (response as? HTTPURLResponse)?.statusCode ?? 200
            var etag = state?.etag
            if status == 200 {
                let text = String(decoding: data, as: UTF8.self)
                let fresh = await Task.detached(priority: .utility) { ManifestIndex(scanning: text) }.value
                guard !fresh.games.isEmpty else { throw URLError(.cannotParseResponse) }
                try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
                try JSONEncoder().encode(fresh).write(to: indexFile, options: .atomic)
                index = fresh
                etag = (response as? HTTPURLResponse)?.value(forHTTPHeaderField: "ETag")
                log.info("read Ludusavi's manifest: \(fresh.games.count) games")
            } else if status != 304 {
                throw URLError(.badServerResponse)
            }
            try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
            try JSONEncoder().encode(State(etag: etag, checked: now)).write(to: stateFile, options: .atomic)
        } catch {
            log.error("fetching Ludusavi's manifest failed: \(error.localizedDescription)")
        }
        return index
    }
}
