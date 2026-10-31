import Foundation
import Testing
@testable import WebRuntime

/// `VNDEGEN_TEST_DIR` puts the games on another volume, such as a case-sensitive disk image.
private let scratch = ProcessInfo.processInfo.environment["VNDEGEN_TEST_DIR"].map { URL(fileURLWithPath: $0, isDirectory: true) }
    ?? FileManager.default.temporaryDirectory

private func temporaryGame() throws -> URL {
    let root = scratch.appendingPathComponent("vndegen-\(UUID().uuidString)", isDirectory: true)
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

private func text(of response: FileResponse) async throws -> String {
    var body = Data()
    try await response.body?.stream { body.append($0); return true }
    return String(decoding: body, as: UTF8.self)
}

@Test func servingReturnsWholeFilesRangesAndMisses() async throws {
    let root = try temporaryGame()
    defer { try? FileManager.default.removeItem(at: root) }
    let folder = GameFolder(root: root, webRoot: "")

    let whole = folder.serve(path: "/js/main.js")
    #expect(whole.status == 200)
    #expect(whole.headers["Content-Type"] == "text/javascript")
    #expect(whole.headers["Content-Length"] == "10")
    #expect(whole.headers["Accept-Ranges"] == "bytes")
    #expect(try await text(of: whole) == "0123456789")

    let part = folder.serve(path: "/js/main.js", range: "bytes=2-4")
    #expect(part.status == 206)
    #expect(part.headers["Content-Range"] == "bytes 2-4/10")
    #expect(try await text(of: part) == "234")

    let head = folder.serve(path: "/js/main.js", method: "HEAD")
    #expect(head.status == 200 && head.body == nil && head.headers["Content-Length"] == "10")

    #expect(folder.serve(path: "/js/main.js", range: "bytes=10-").status == 416)
    #expect(folder.serve(path: "/js/missing.js").status == 404)
    #expect(folder.serve(path: "/js").status == 404)
    #expect(folder.serve(path: "/../../../../etc/passwd").status == 404)
}

@Test func bodiesStreamInChunksAndStopWhenTheTaskStops() async throws {
    let root = try temporaryGame()
    defer { try? FileManager.default.removeItem(at: root) }
    let span = try #require(GameFolder(root: root, webRoot: "").serve(path: "/js/main.js", range: "bytes=1-").body)

    var chunks: [String] = []
    try await span.stream(chunkSize: 4) { chunks.append(String(decoding: $0, as: UTF8.self)); return true }
    #expect(chunks == ["1234", "5678", "9"])

    chunks = []
    try await span.stream(chunkSize: 4) { chunks.append(String(decoding: $0, as: UTF8.self)); return false }
    #expect(chunks == ["1234"])
}

@Test func symlinksThatLeaveTheGameFolderAreRefused() async throws {
    let fm = FileManager.default
    let root = try temporaryGame()
    let outside = root.deletingLastPathComponent().appendingPathComponent("outside-\(UUID().uuidString)")
    defer { try? fm.removeItem(at: root); try? fm.removeItem(at: outside) }
    try fm.createDirectory(at: outside, withIntermediateDirectories: true)
    try Data("secret".utf8).write(to: outside.appendingPathComponent("secret.txt"))
    try fm.createDirectory(at: root.appendingPathComponent("save"), withIntermediateDirectories: true)
    try fm.createSymbolicLink(at: root.appendingPathComponent("outside"), withDestinationURL: outside)
    try fm.createSymbolicLink(at: root.appendingPathComponent("save/file1.rmmzsave"), withDestinationURL: outside.appendingPathComponent("secret.txt"))
    try fm.createSymbolicLink(at: root.appendingPathComponent("save/file2.rmmzsave"), withDestinationURL: outside.appendingPathComponent("new.txt"))
    try fm.createSymbolicLink(at: root.appendingPathComponent("scripts"), withDestinationURL: root.appendingPathComponent("js"))
    let folder = GameFolder(root: root, webRoot: "")
    let bridge = NodeBridge(folder: folder)

    #expect(folder.resolve(virtualPath: "/game/outside") == nil)
    #expect(folder.resolve(virtualPath: "/game/outside/secret.txt") == nil)
    #expect(folder.resolve(virtualPath: "/game/outside/new/file.txt") == nil)
    #expect(folder.resolve(virtualPath: "/game/save/file1.rmmzsave") == nil)
    #expect(folder.resolve(virtualPath: "/game/save/file2.rmmzsave") == nil)
    #expect(folder.serve(path: "/outside/secret.txt").status == 404)
    #expect(folder.serve(path: "/save/file1.rmmzsave").status == 404)
    #expect(bridge.handle(.exists(path: "/game/outside/secret.txt")) == .value(.bool(false)))
    for request: BridgeRequest in [
        .readFile(path: "/game/outside/secret.txt", text: true),
        .readFile(path: "/game/save/file1.rmmzsave", text: true),
        .writeFile(path: "/game/outside/new.txt", data: "x", text: true),
        .writeFile(path: "/game/save/file2.rmmzsave", data: "x", text: true),
        .mkdir(path: "/game/outside/dir", recursive: true),
        .copyFile(from: "/game/index.html", to: "/game/outside/new.txt"),
        .rename(from: "/game/js/main.js", to: "/game/outside/new.txt"),
        .unlink(path: "/game/outside/secret.txt"),
        .rmdir(path: "/game/outside"),
    ] {
        guard case .failure(let error) = bridge.handle(request) else { Issue.record("\(request) reached outside"); continue }
        #expect(error.code == "EACCES")
    }
    #expect(try fm.contentsOfDirectory(atPath: outside.path) == ["secret.txt"])
    #expect(try String(contentsOf: outside.appendingPathComponent("secret.txt"), encoding: .utf8) == "secret")

    // A symlink that stays inside the game folder still works.
    #expect(try await text(of: folder.serve(path: "/scripts/main.js")) == "0123456789")
}

/// Exercises the lookup only on a case-sensitive volume, such as `VNDEGEN_TEST_DIR` on a case-sensitive APFS disk image.
/// On a case-insensitive disk the same expectations hold without it.
@Test func namesWithTheWrongCaseFindTheFileOnDisk() async throws {
    let fm = FileManager.default
    let root = try temporaryGame()
    defer { try? fm.removeItem(at: root) }
    try fm.createDirectory(at: root.appendingPathComponent("img/Pictures"), withIntermediateDirectories: true)
    try Data("png".utf8).write(to: root.appendingPathComponent("img/Pictures/Actor1.png"))
    try fm.createDirectory(at: root.appendingPathComponent("Save"), withIntermediateDirectories: true)
    let folder = GameFolder(root: root, webRoot: "")
    let bridge = NodeBridge(folder: folder)

    #expect(try await text(of: folder.serve(path: "/IMG/pictures/actor1.PNG")) == "png")
    #expect(bridge.handle(.exists(path: "/game/img/pictures/ACTOR1.png")) == .value(.bool(true)))
    #expect(bridge.handle(.readFile(path: "/game/Img/Pictures/actor1.png", text: true)) == .value(.string("png")))
    #expect(bridge.handle(.writeFile(path: "/game/save/file1.rmmzsave", data: "s", text: true)) == .value(.null))
    #expect(try fm.contentsOfDirectory(atPath: root.appendingPathComponent("Save").path) == ["file1.rmmzsave"])

    // A file the game wrote is found in any case, and writing or renaming onto it in another case replaces it.
    #expect(bridge.handle(.readFile(path: "/game/SAVE/File1.rmmzsave", text: true)) == .value(.string("s")))
    #expect(bridge.handle(.writeFile(path: "/game/save/FILE1.rmmzsave", data: "t", text: true)) == .value(.null))
    #expect(bridge.handle(.writeFile(path: "/game/save/file1.rmmzsave_", data: "u", text: true)) == .value(.null))
    #expect(bridge.handle(.rename(from: "/game/save/FILE1.RMMZSAVE_", to: "/game/save/File1.rmmzsave")) == .value(.null))
    #expect(try fm.contentsOfDirectory(atPath: root.appendingPathComponent("Save").path) == ["file1.rmmzsave"])
    #expect(bridge.handle(.readFile(path: "/game/save/file1.rmmzsave", text: true)) == .value(.string("u")))
    #expect(bridge.handle(.mkdir(path: "/game/IMG/pictures", recursive: false)) == .failure(BridgeError(code: "EEXIST", message: "file already exists")))
    #expect(bridge.handle(.readdir(path: "/game/IMG/PICTURES")) == .value(.strings(["Actor1.png"])))
}

@Test func mountedAppFoldersAndEmbeddedFilesAreServed() async throws {
    let root = try temporaryGame()
    let app = try temporaryGame()
    defer {
        try? FileManager.default.removeItem(at: root)
        try? FileManager.default.removeItem(at: app)
    }
    try Data("MZ....FWS-movie....".utf8).write(to: root.appendingPathComponent("Game.exe"))
    try Data("player".utf8).write(to: app.appendingPathComponent("js/player.js"))
    var folder = GameFolder(root: root, webRoot: "")
    folder.mounts = ["/__app/": app, "/__app/js/": app.appendingPathComponent("js")]
    folder.embedded = ["/Game.exe": FileSlice(offset: 6, length: 9)]

    // The longest mount wins, and a mount never reads from the game.
    #expect(try await text(of: folder.serve(path: "/__app/index.html")) == "<html></html>")
    #expect(try await text(of: folder.serve(path: "/__app/js/player.js")) == "player")
    #expect(folder.serve(path: "/__app/../js/main.js").status == 404)
    #expect(folder.serve(path: "/__app/js/main.js").status == 200)
    #expect(try await text(of: folder.serve(path: "/__app/js/main.js")) == "0123456789")
    #expect(try await text(of: folder.serve(path: "/js/main.js")) == "0123456789")

    let movie = folder.serve(path: "/Game.exe")
    #expect(movie.headers["Content-Length"] == "9")
    #expect(try await text(of: movie) == "FWS-movie")
    #expect(try await text(of: folder.serve(path: "/Game.exe", range: "bytes=4-")) == "movie")
    #expect(folder.read(servedPath: "/Game.exe", limit: 3) == Data("FWS".utf8))
    // A request in another case gets the movie too, never the whole executable.
    #expect(try await text(of: folder.serve(path: "/game.EXE")) == "FWS-movie")

    // A slice past the end of the file serves nothing.
    folder.embedded = ["/Game.exe": FileSlice(offset: 6, length: 100)]
    #expect(folder.serve(path: "/Game.exe").status == 404)
}
