import Foundation
import Testing
@testable import EngineRPGMaker

private func temporaryGame() throws -> URL {
    let root = FileManager.default.temporaryDirectory.appendingPathComponent("vndegen-\(UUID().uuidString)", isDirectory: true)
    try FileManager.default.createDirectory(at: root.appendingPathComponent("js"), withIntermediateDirectories: true)
    try Data("<html></html>".utf8).write(to: root.appendingPathComponent("index.html"))
    try Data("0123456789".utf8).write(to: root.appendingPathComponent("js/main.js"))
    return root
}

@Test func pathsThatEscapeTheGameFolderAreRejected() {
    let folder = GameFolder(root: URL(fileURLWithPath: "/tmp/game"), webRoot: "")
    #expect(folder.resolve(virtualPath: "/game/../etc/passwd") == nil)
    #expect(folder.resolve(virtualPath: "/game/save/../../etc/passwd") == nil)
    #expect(folder.resolve(virtualPath: "/etc/passwd") == nil)
    #expect(folder.resolve(virtualPath: "/") == nil)
    #expect(folder.resolve(virtualPath: "save/file1.rmmzsave") == nil)
    #expect(folder.resolve(virtualPath: "/gamer/save") == nil)
    #expect(folder.resolve(servedPath: "/../index.html") == nil)
}

@Test func pathsInsideTheGameFolderResolveToDisk() {
    let folder = GameFolder(root: URL(fileURLWithPath: "/tmp/game"), webRoot: "")
    #expect(folder.resolve(virtualPath: "/game/save/file1.rmmzsave")?.path == "/tmp/game/save/file1.rmmzsave")
    #expect(folder.resolve(virtualPath: "/game/save/../data/./Map001.json")?.path == "/tmp/game/data/Map001.json")
    #expect(folder.resolve(virtualPath: "/game/save/")?.path == "/tmp/game/save")
    #expect(folder.resolve(virtualPath: "/game")?.path == "/tmp/game")
    #expect(folder.mainModule == "/game/index.html")
    #expect(folder.resolve(servedPath: "/")?.path == "/tmp/game/index.html")
    #expect(folder.resolve(servedPath: "/img/system/Balloon.png_")?.path == "/tmp/game/img/system/Balloon.png_")
}

@Test func deployedMVGamesServeFromWww() {
    let folder = GameFolder(root: URL(fileURLWithPath: "/tmp/game"), webRoot: "www")
    #expect(folder.mainModule == "/game/www/index.html")
    #expect(folder.resolve(servedPath: "/js/rpg_core.js")?.path == "/tmp/game/www/js/rpg_core.js")
    #expect(folder.resolve(virtualPath: "/game/www/save/file1.rpgsave")?.path == "/tmp/game/www/save/file1.rpgsave")
}

@Test func webRootIsDetectedFromIndexLocation() throws {
    let root = try temporaryGame()
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(GameFolder(root: root).webRoot == "")
    try FileManager.default.moveItem(at: root.appendingPathComponent("index.html"), to: root.appendingPathComponent("moved.html"))
    #expect(GameFolder(root: root).webRoot == "www")
}

@Test func mimeTypesCoverWhatTheEngineLoads() {
    #expect(GameFolder.mimeType(forExtension: "wasm") == "application/wasm")
    #expect(GameFolder.mimeType(forExtension: "JS") == "text/javascript")
    #expect(GameFolder.mimeType(forExtension: "webm") == "video/webm")
    #expect(GameFolder.mimeType(forExtension: "png_") == "application/octet-stream")
    #expect(GameFolder.mimeType(forExtension: "ogg_") == "application/octet-stream")
    #expect(GameFolder.mimeType(forExtension: "woff") == "font/woff")
}

@Test func byteRangesFollowTheHTTPRules() {
    #expect(GameFolder.byteRange(header: nil, size: 100) == .full)
    #expect(GameFolder.byteRange(header: "bytes=0-9", size: 100) == .partial(start: 0, end: 9))
    #expect(GameFolder.byteRange(header: "bytes=90-", size: 100) == .partial(start: 90, end: 99))
    #expect(GameFolder.byteRange(header: "bytes=0-500", size: 100) == .partial(start: 0, end: 99))
    #expect(GameFolder.byteRange(header: "bytes=-10", size: 100) == .partial(start: 90, end: 99))
    #expect(GameFolder.byteRange(header: "bytes=100-", size: 100) == .unsatisfiable)
    #expect(GameFolder.byteRange(header: "bytes=5-2", size: 100) == .full)
    #expect(GameFolder.byteRange(header: "items=0-1", size: 100) == .full)
    #expect(GameFolder.byteRange(header: "bytes=0-1", size: 0) == .full)
}

@Test func servingReturnsWholeFilesRangesAndMisses() throws {
    let root = try temporaryGame()
    defer { try? FileManager.default.removeItem(at: root) }
    let folder = GameFolder(root: root)

    let whole = folder.serve(path: "/js/main.js")
    #expect(whole.status == 200)
    #expect(whole.headers["Content-Type"] == "text/javascript")
    #expect(whole.headers["Content-Length"] == "10")
    #expect(whole.headers["Accept-Ranges"] == "bytes")
    #expect(String(decoding: whole.body, as: UTF8.self) == "0123456789")

    let part = folder.serve(path: "/js/main.js", range: "bytes=2-4")
    #expect(part.status == 206)
    #expect(part.headers["Content-Range"] == "bytes 2-4/10")
    #expect(String(decoding: part.body, as: UTF8.self) == "234")

    let head = folder.serve(path: "/js/main.js", method: "HEAD")
    #expect(head.status == 200 && head.body.isEmpty && head.headers["Content-Length"] == "10")

    #expect(folder.serve(path: "/js/main.js", range: "bytes=10-").status == 416)
    #expect(folder.serve(path: "/js/missing.js").status == 404)
    #expect(folder.serve(path: "/js").status == 404)
    #expect(folder.serve(path: "/../../../../etc/passwd").status == 404)
}
