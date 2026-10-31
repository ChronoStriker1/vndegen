import Foundation
import Testing
import WebKit
import WebRuntime
@testable import EngineRPGMaker

/// StorageManager.localFileDirectoryPath from MV 1.2.0's rpg_managers.js, which builds the folder from the page's address.
private let earlyCore = """
    function StorageManager() {}
    StorageManager.localFileDirectoryPath = function() {
        var path = window.location.pathname.replace(/(\\/www|)\\/[^\\/]*$/, '/save/');
        if (path.match(/^\\/([A-Z]\\:)/)) {
            path = path.slice(1);
        }
        return decodeURIComponent(path);
    };
    """

@Test func earlyMVCoresSaveBesideTheWebRoot() throws {
    let fm = FileManager.default
    let root = fm.temporaryDirectory.appendingPathComponent("vndegen-early-\(UUID().uuidString)", isDirectory: true)
    defer { try? fm.removeItem(at: root) }
    try fm.createDirectory(at: root.appendingPathComponent("www/js"), withIntermediateDirectories: true)
    try Data().write(to: root.appendingPathComponent("www/js/rpg_core.js"))
    try Data(earlyCore.utf8).write(to: root.appendingPathComponent("www/js/rpg_managers.js"))
    let plugin = RPGMakerPlugin(.mv)
    #expect(plugin.saveLocations(folder: root, dataDirectory: root).map(\.directory.path) == [root.appendingPathComponent("save").path])

    try Data("StorageManager.localFileDirectoryPath = function() { return path.join(base, 'save/'); };".utf8)
        .write(to: root.appendingPathComponent("www/js/rpg_managers.js"))
    #expect(plugin.saveLocations(folder: root, dataDirectory: root).map(\.directory.path) == [root.appendingPathComponent("www/save").path])
}

@Test @MainActor func earlyMVCoresSaveWhereThePCDoes() async throws {
    let source = URL(filePath: #filePath).deletingLastPathComponent()
        .appending(path: "../../Sources/EngineRPGMaker/Resources/rpgmaker.js").standardized
    let configuration = WKWebViewConfiguration()
    let shim = "window.__vndegen = {}; window.process = { mainModule: { filename: '/game/www/index.html' } };"
    for script in [shim, try String(contentsOf: source, encoding: .utf8)] {
        configuration.userContentController.addUserScript(WKUserScript(source: script, injectionTime: .atDocumentStart, forMainFrameOnly: true))
    }
    // The rpgmaker.js check runs when a script file loads, as the game's do, so the core comes from a data: URL.
    let script = { (source: String) in
        "<script src=\"data:text/javascript,\(source.addingPercentEncoding(withAllowedCharacters: .alphanumerics)!)\"></script>"
    }
    let page = "<html><body>\(script(earlyCore + "window.early = StorageManager.localFileDirectoryPath;"))</body></html>"
    let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
    webView.loadHTMLString(page, baseURL: URL(string: "https://localhost/www/index.html"))
    let deadline = ContinuousClock.now + .seconds(20)
    while try await webView.evaluateJavaScript("document.readyState === 'complete' && !!window.StorageManager") as? Bool != true {
        guard ContinuousClock.now < deadline else { throw CancellationError() }
        try await Task.sleep(for: .milliseconds(50))
    }
    let folders = try await webView.callAsyncJavaScript("""
        return [window.early(), StorageManager.localFileDirectoryPath()];
        """, arguments: [:], contentWorld: .page) as? [String]
    // The game's own rule gives /save/ for this page's address, outside the game folder.
    #expect(folders == ["/save/", "/game/save/"])
}
