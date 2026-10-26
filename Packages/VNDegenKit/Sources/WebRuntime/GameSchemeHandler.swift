import Foundation
import OSLog
import WebKit

/// Serves `vndegen://game/...` from the game's web root. Reads run off the main thread, one chunk at a time, and stop
/// when WebKit stops the task. Replies are matched back by a serial, not by task identity, because WebKit throws if a
/// task is touched after `stop`, and a stopped task's address can be reused by a new one before a slow read returns.
final class GameSchemeHandler: NSObject, WKURLSchemeHandler {
    private let folder: GameFolder
    private let log: Logger
    private var pending: [Int: any WKURLSchemeTask] = [:]
    private var nextSerial = 0

    init(folder: GameFolder, log: Logger) {
        self.folder = folder
        self.log = log
    }

    func webView(_ webView: WKWebView, start task: any WKURLSchemeTask) {
        let request = task.request
        guard let url = request.url else {
            task.didFailWithError(URLError(.badURL))
            return
        }
        nextSerial += 1
        let serial = nextSerial
        pending[serial] = task
        let path = url.path
        let method = request.httpMethod ?? "GET"
        let range = request.value(forHTTPHeaderField: "Range")
        let folder = folder
        Task.detached(priority: .userInitiated) {
            let response = folder.serve(path: path, method: method, range: range)
            guard await self.respond(serial, url: url, with: response) else { return }
            do {
                try await response.body?.stream { await self.send($0, to: serial) }
                await self.finish(serial)
            } catch {
                await self.finish(serial, failing: error)
            }
        }
    }

    func webView(_ webView: WKWebView, stop task: any WKURLSchemeTask) {
        if let serial = pending.first(where: { $0.value === task })?.key {
            pending.removeValue(forKey: serial)
        }
    }

    /// Sends the status and headers. False when WebKit has stopped the task.
    private func respond(_ serial: Int, url: URL, with response: FileResponse) -> Bool {
        guard let task = pending[serial] else { return false }
        if response.status >= 400 {
            log.error("served \(response.status) for \(url.path, privacy: .public)")
        }
        task.didReceive(HTTPURLResponse(url: url, statusCode: response.status, httpVersion: "HTTP/1.1", headerFields: response.headers)!)
        return true
    }

    /// False when WebKit has stopped the task, which ends the read.
    private func send(_ data: Data, to serial: Int) -> Bool {
        guard let task = pending[serial] else { return false }
        task.didReceive(data)
        return true
    }

    private func finish(_ serial: Int, failing error: (any Error)? = nil) {
        guard let task = pending.removeValue(forKey: serial) else { return }
        if let error {
            log.error("read failed: \(error.localizedDescription, privacy: .public)")
            task.didFailWithError(error)
        } else {
            task.didFinish()
        }
    }
}
