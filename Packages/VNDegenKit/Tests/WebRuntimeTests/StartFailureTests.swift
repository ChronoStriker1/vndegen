import Foundation
import OSLog
import TestSupport
import Testing
import VNCore
import WebKit
@testable import WebRuntime

@MainActor @Test func aMissingStartPageEndsTheSessionWithItsName() async throws {
    let root = TestFolders.root.appendingPathComponent("web-start-\(UUID().uuidString)", isDirectory: true)
    try FileManager.default.createDirectory(at: root.appendingPathComponent("js"), withIntermediateDirectories: true)
    try Data().write(to: root.appendingPathComponent("js/main.js"))
    let configuration = WebGameConfiguration(folder: GameFolder(root: root, webRoot: ""), appName: "test",
                                             buttonScript: { _, _ in nil }, scriptArgument: "test-script", log: Logger())
    let (exits, exited) = AsyncStream<Void>.makeStream()
    let launch = GameLaunch(game: Game(id: GameID("Test Game"), title: "Test Game"), folder: root, dataDirectory: root) {
        exited.yield()
    }
    let session = try configuration.makeSession(launch)
    let timeout = Task { try await Task.sleep(for: .seconds(15)); exited.finish() }
    var ended = false
    for await _ in exits { ended = true; break }
    timeout.cancel()
    #expect(ended)
    #expect(session.failedToStart)
    #expect(session.startFailureReason == "The game's index.html is missing or can't be read.")
}

@MainActor @Test func aPageThatCrashesAgainRightAfterItsReloadEndsTheSession() async throws {
    let root = TestFolders.root.appendingPathComponent("web-crash-\(UUID().uuidString)", isDirectory: true)
    try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
    defer { try? FileManager.default.removeItem(at: root) }
    try Data("<html><body></body></html>".utf8).write(to: root.appendingPathComponent("index.html"))
    let configuration = WebGameConfiguration(folder: GameFolder(root: root, webRoot: ""), appName: "test",
                                             buttonScript: { _, _ in nil }, scriptArgument: "test-script", log: Logger())
    let launch = GameLaunch(game: Game(id: GameID("Test Game"), title: "Test Game"), folder: root, dataDirectory: root) {}
    let session = try WebGameSession(launch: launch, configuration: configuration)
    let page = try PageHarness(storage: false) { _ in .value(.null) }
    try await page.load("<html><body></body></html>")

    session.webViewWebContentProcessDidTerminate(page.webView)
    #expect(!session.failedToStart)
    session.webViewWebContentProcessDidTerminate(page.webView)
    #expect(session.failedToStart)
    #expect(session.startFailureReason == "Its page crashed as it loaded, possibly from running out of memory")
    session.terminate()
}

@MainActor @Test func aPageThatCrashesBeforeLoadingEndsTheSession() throws {
    let root = TestFolders.root.appendingPathComponent("web-crash-\(UUID().uuidString)", isDirectory: true)
    let configuration = WebGameConfiguration(folder: GameFolder(root: root, webRoot: ""), appName: "test",
                                             buttonScript: { _, _ in nil }, scriptArgument: "test-script", log: Logger())
    let launch = GameLaunch(game: Game(id: GameID("Test Game"), title: "Test Game"), folder: root, dataDirectory: root) {}
    let session = try WebGameSession(launch: launch, configuration: configuration)
    session.webViewWebContentProcessDidTerminate(WKWebView())
    #expect(session.failedToStart)
    session.terminate()
}
