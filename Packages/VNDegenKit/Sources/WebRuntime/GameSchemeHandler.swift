import FontPack
import Foundation
import OSLog
import WebKit

/// Serves `vndegen://game/...` from the game's web root, and the font substitutes from `WebGameSession.fontPath`. Reads run off the main thread, one chunk at a time, and stop
/// when WebKit stops the task. Replies are matched back by a serial, not by task identity, because WebKit throws if a
/// task is touched after `stop`, and a stopped task's address can be reused by a new one before a slow read returns.
final class GameSchemeHandler: NSObject, WKURLSchemeHandler {
    private let folder: GameFolder
    private let log: Logger
    private var pending: [Int: any WKURLSchemeTask] = [:]
    private var nextSerial = 0
    private let alpha: Bool
    private let cancellation = AlphaVideoCancellation()
    private static let stashRoot = FileManager.default.temporaryDirectory.appendingPathComponent("AlphaVideo-stash", isDirectory: true)
    private static var leftoversRemoved = false
    private let stashFolder = GameSchemeHandler.stashRoot.appendingPathComponent(UUID().uuidString, isDirectory: true)
    private var reads: [Int: Task<Void, Never>] = [:]
    private weak var page: WKWebView?

    init(folder: GameFolder, log: Logger, alpha: Bool = false) {
        self.folder = folder
        self.log = log
        self.alpha = alpha
    }

    func webView(_ webView: WKWebView, start task: any WKURLSchemeTask) {
        let request = task.request
        guard let url = request.url else {
            task.didFailWithError(URLError(.badURL))
            return
        }
        nextSerial += 1
        page = webView
        let serial = nextSerial
        pending[serial] = task
        let path = url.path
        let method = request.httpMethod ?? "GET"
        let range = request.value(forHTTPHeaderField: "Range")
        let folder = folder
        let alpha = alpha, cancellation = cancellation, log = log, stash = stashFolder
        reads[serial] = Task.detached(priority: .userInitiated) {
            do {
                let stashed = alpha ? Self.stashKey(path: path) : nil
                let movie: FileResponse? = if let stashed {
                    try await AlphaVideoCache.shared.stashedResponse(key: stashed, stash: stash, method: method, range: range,
                                                                     cancellation: cancellation, log: log) ?? .empty(404)
                } else if alpha && path.lowercased().hasSuffix(".webm") {
                    try await AlphaVideoCache.shared.response(folder: folder, path: path,
                        method: method, range: range, cancellation: cancellation, log: log)
                } else {
                    nil
                }
                let response = movie ?? Self.serveFont(path: path, method: method, range: range)
                    ?? folder.serve(path: path, method: method, range: range)
                guard await self.respond(serial, url: url, with: response) else { return }
                try await response.body?.stream { await self.send($0, to: serial) }
                await self.finish(serial)
            } catch {
                await self.finish(serial, failing: error)
            }
        }
    }

    func cancel() {
        cancellation.cancel()
        for read in reads.values { read.cancel() }
    }

    func waitUntilStopped() async {
        for read in reads.values { await read.value }
        try? FileManager.default.removeItem(at: stashFolder)
    }

    /// Keeps a WebM the page holds in memory for conversion, and answers with the address that serves it converted, or
    /// null when the clip has no alpha or this game's movies aren't converted.
    func stash(base64: String) -> BridgeReply {
        guard alpha else { return .value(.null) }
        guard let clip = Data(base64Encoded: base64) else { return .failure(BridgeError(code: "EINVAL", message: "data is not base64")) }
        if !Self.leftoversRemoved {
            Self.leftoversRemoved = true
            try? FileManager.default.removeItem(at: Self.stashRoot)
        }
        do {
            guard let key = try AlphaVideoCache.shared.stash(clip, in: stashFolder, log: log) else { return .value(.null) }
            return .value(.string(GameFolder.origin + GameFolder.alphaPath + key + ".webm"))
        } catch {
            return .failure(BridgeError(code: "EIO", message: error.localizedDescription))
        }
    }

    nonisolated static func stashKey(path: String) -> String? {
        guard path.hasPrefix(GameFolder.alphaPath) else { return nil }
        let name = path.dropFirst(GameFolder.alphaPath.count)
        guard name.hasSuffix(".webm") else { return nil }
        let key = name.dropLast(5)
        guard key.count == 64, key.allSatisfy({ $0.isHexDigit && !$0.isUppercase }) else { return nil }
        return String(key)
    }

    /// The response for a font substitute's path, or nil when the path is not under the font path.
    nonisolated static func serveFont(path: String, method: String, range: String?) -> FileResponse? {
        guard path.hasPrefix(WebGameSession.fontPath) else { return nil }
        let name = String(path.dropFirst(WebGameSession.fontPath.count))
        var info = stat()
        guard let file = FontPack.file(named: name), let handle = try? FileHandle(forReadingFrom: file),
              fstat(handle.fileDescriptor, &info) == 0 else { return .empty(404) }
        return GameFolder.response(handle, base: 0, size: Int64(info.st_size),
                                   contentType: GameFolder.mimeType(forExtension: file.pathExtension), method: method, range: range)
    }

    func webView(_ webView: WKWebView, stop task: any WKURLSchemeTask) {
        if let serial = pending.first(where: { $0.value === task })?.key {
            pending.removeValue(forKey: serial)
        }
    }

    /// Sends the status and headers. False when WebKit has stopped the task.
    private func respond(_ serial: Int, url: URL, with response: FileResponse) async -> Bool {
        if response.headers["X-VNDegen-Alpha"] == "1" {
            let json = String(decoding: (try? JSONEncoder().encode(url.absoluteString)) ?? Data("null".utf8), as: UTF8.self)
            do { _ = try await page?.evaluateJavaScript("window.__vndegen.alphaVideo(\(json))") }
            catch { finish(serial, failing: error); return false }
        }
        guard !Task.isCancelled, let task = pending[serial] else { reads.removeValue(forKey: serial); return false }
        if response.status >= 400 {
            log.error("served \(response.status) for \(url.path, privacy: .public)")
            if task.request.mainDocumentURL == url {
                finish(serial, failing: URLError(.fileDoesNotExist))
                return false
            }
        }
        task.didReceive(HTTPURLResponse(url: url, statusCode: response.status, httpVersion: "HTTP/1.1", headerFields: response.headers)!)
        return true
    }

    /// False when WebKit has stopped the task, which ends the read.
    private func send(_ data: Data, to serial: Int) -> Bool {
        guard !Task.isCancelled, let task = pending[serial] else { return false }
        task.didReceive(data)
        return true
    }

    private func finish(_ serial: Int, failing error: (any Error)? = nil) {
        reads.removeValue(forKey: serial)
        guard let task = pending.removeValue(forKey: serial) else { return }
        if let error {
            log.error("read failed: \(error.localizedDescription, privacy: .public)")
            task.didFailWithError(error)
        } else {
            task.didFinish()
        }
    }
}
