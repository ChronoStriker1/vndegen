import Foundation
import WebKit

/// Serves `vndegen://game/...` from the game's web root. Reads run off the main thread. Replies are matched back by a
/// serial, not by task identity, because WebKit throws if a task is touched after `stop`, and a stopped task's address
/// can be reused by a new one before a slow read returns.
final class GameSchemeHandler: NSObject, WKURLSchemeHandler {
    private let folder: GameFolder
    private var pending: [Int: any WKURLSchemeTask] = [:]
    private var nextSerial = 0

    init(folder: GameFolder) {
        self.folder = folder
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
            await self.finish(serial, url: url, with: response)
        }
    }

    func webView(_ webView: WKWebView, stop task: any WKURLSchemeTask) {
        if let serial = pending.first(where: { $0.value === task })?.key {
            pending.removeValue(forKey: serial)
        }
    }

    private func finish(_ serial: Int, url: URL, with response: FileResponse) {
        guard let task = pending.removeValue(forKey: serial) else { return }
        let http = HTTPURLResponse(url: url, statusCode: response.status, httpVersion: "HTTP/1.1", headerFields: response.headers)!
        task.didReceive(http)
        if !response.body.isEmpty { task.didReceive(response.body) }
        task.didFinish()
    }
}
