import Foundation
import OSLog
import TestSupport
import Testing
import VNCore
import WebKit
@testable import WebRuntime

@MainActor @Test func aGamesLinkAsksFirstOneAtATimeAndNothingElseOpens() async throws {
    let root = TestFolders.root.appendingPathComponent("web-links-\(UUID().uuidString)", isDirectory: true)
    try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
    defer { try? FileManager.default.removeItem(at: root) }
    try Data("<html><body></body></html>".utf8).write(to: root.appendingPathComponent("index.html"))
    let configuration = WebGameConfiguration(folder: GameFolder(root: root, webRoot: ""), appName: "test",
                                             buttonScript: { _, _ in nil }, scriptArgument: "test-script", log: Logger())
    let launch = GameLaunch(game: Game(id: GameID("Test Game"), title: "Test Game"), folder: root, dataDirectory: root) {}
    let session = try WebGameSession(launch: launch, configuration: configuration)
    defer { session.terminate() }
    var asked: [String] = []
    var finish: (() -> Void)?
    session.askToOpen = { url, done in
        asked.append(url.absoluteString)
        finish = done
    }
    let webView = try #require(session.view as? WKWebView)
    let deadline = ContinuousClock.now + .seconds(15)
    while (try? await webView.evaluateJavaScript("document.readyState === 'complete' && !!window.nw")) as? Bool != true,
          ContinuousClock.now < deadline {
        try await Task.sleep(for: .milliseconds(50))
    }
    func open(_ url: String) async throws {
        _ = try await session.callFunction("nw.Shell.openExternal(url)", arguments: ["url": url])
    }
    for url in ["file:///etc/hosts", "/Applications", "steam://run/1", "javascript:alert(1)", "mailto:",
                "https://example.com/first", "https://example.com/second"] {
        try await open(url)
    }
    #expect(asked == ["https://example.com/first"])
    finish?()
    try await open("mailto:someone@example.com")
    #expect(asked == ["https://example.com/first", "mailto:someone@example.com"])
}
