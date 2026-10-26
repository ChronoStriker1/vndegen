import Foundation
import Testing
@testable import WebRuntime

/// A game folder whose data/ directory a malicious game swaps, between a path check and the open that follows it, for a
/// symlink to a directory outside the game.
private struct SwapFixture {
    let root: URL
    let outside: URL
    let folder: GameFolder

    init() throws {
        let fm = FileManager.default
        let base = fm.temporaryDirectory.appendingPathComponent("vndegen-swap-\(UUID().uuidString)", isDirectory: true)
        root = base.appendingPathComponent("game", isDirectory: true)
        outside = base.appendingPathComponent("outside", isDirectory: true)
        try fm.createDirectory(at: root.appendingPathComponent("data"), withIntermediateDirectories: true)
        try fm.createDirectory(at: outside, withIntermediateDirectories: true)
        try Data("inside".utf8).write(to: root.appendingPathComponent("data/page.txt"))
        try Data("secret".utf8).write(to: outside.appendingPathComponent("page.txt"))
        folder = GameFolder(root: root, webRoot: "")
    }

    /// What the Node bridge's rename lets a game do: move data/ aside and put an escaping symlink in its place.
    func swap() throws {
        let fm = FileManager.default
        try fm.moveItem(at: root.appendingPathComponent("data"), to: root.appendingPathComponent("data-old"))
        try fm.createSymbolicLink(at: root.appendingPathComponent("data"), withDestinationURL: outside)
    }

    func remove() { try? FileManager.default.removeItem(at: root.deletingLastPathComponent()) }
}

@Test func aServedFileStaysTheOneThatWasChecked() async throws {
    let fixture = try SwapFixture()
    defer { fixture.remove() }
    let response = fixture.folder.serve(path: "/data/page.txt")
    #expect(response.status == 200)
    try fixture.swap()

    var body = Data()
    try await response.body?.stream { body.append($0); return true }
    #expect(String(decoding: body, as: UTF8.self) == "inside")
}

@Test func anOpenAfterTheSwapIsRefused() throws {
    let fixture = try SwapFixture()
    defer { fixture.remove() }
    let page = try #require(fixture.folder.resolve(virtualPath: "/game/data/page.txt"))
    let newFile = try #require(fixture.folder.resolve(virtualPath: "/game/data/new.txt"))
    try fixture.swap()

    #expect(throws: OpenFailure.outside) { try fixture.folder.open(page) }
    #expect(throws: OpenFailure.outside) { try fixture.folder.openParent(of: newFile) }
    #expect(try FileManager.default.contentsOfDirectory(atPath: fixture.outside.path) == ["page.txt"])
}

@Test func theBridgeRefusesTheSwappedDirectory() throws {
    let fixture = try SwapFixture()
    defer { fixture.remove() }
    let bridge = NodeBridge(folder: fixture.folder)
    try fixture.swap()

    for request: BridgeRequest in [
        .readFile(path: "/game/data/page.txt", text: true),
        .writeFile(path: "/game/data/new.txt", data: "x", text: true),
        .copyFile(from: "/game/data-old/page.txt", to: "/game/data/copy.txt"),
        .rename(from: "/game/data-old/page.txt", to: "/game/data/moved.txt"),
        .mkdir(path: "/game/data/dir", recursive: true),
        .readdir(path: "/game/data"),
    ] {
        guard case .failure(let error) = bridge.handle(request) else { Issue.record("\(request) reached outside"); continue }
        #expect(error.code == "EACCES")
    }
    #expect(try FileManager.default.contentsOfDirectory(atPath: fixture.outside.path) == ["page.txt"])
}
