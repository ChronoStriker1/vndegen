import Foundation
import OSLog
import TestSupport
import Testing
import VNCore
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
