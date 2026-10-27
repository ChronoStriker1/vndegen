import Foundation
import Testing
import VNCore
import WebRuntime
@testable import EngineFlash

/// `VNDEGEN_TEST_DIR` puts the games on another volume, such as a case-sensitive disk image.
private let scratch = ProcessInfo.processInfo.environment["VNDEGEN_TEST_DIR"].map { URL(fileURLWithPath: $0, isDirectory: true) }
    ?? FileManager.default.temporaryDirectory

private func game(_ files: [String: Data]) throws -> URL {
    let root = scratch.appendingPathComponent("vndegen-flash-\(UUID().uuidString)", isDirectory: true)
    for (path, data) in files {
        let url = root.appendingPathComponent(path)
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try data.write(to: url)
    }
    return root
}

/// A movie header and `size` bytes in all.
private func movie(_ size: Int = 64, signature: String = "CWS") -> Data {
    Data(signature.utf8) + Data(count: max(0, size - 3))
}

/// A Windows projector: the player, the movie, then the movie's length and the marker, little-endian.
private func projector(_ movie: Data, player: Int = 100) -> Data {
    func uint32(_ value: UInt32) -> Data { withUnsafeBytes(of: value.littleEndian) { Data($0) } }
    return Data("MZ".utf8) + Data(count: player - 2) + movie + uint32(0xFA12_3456) + uint32(UInt32(movie.count))
}

private func html(_ text: String) -> Data { Data(text.utf8) }

@Test func projectorsCarryTheirMovieAtTheEnd() throws {
    let root = try game(["Game.exe": projector(movie(50)), "Other.exe": Data("MZ not a projector".utf8) + Data(count: 64)])
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(FlashGame.projectorMovie(root.appendingPathComponent("Game.exe")) == FileSlice(offset: 100, length: 50))
    #expect(FlashGame.projectorMovie(root.appendingPathComponent("Other.exe")) == nil)
    // A marker whose length runs past the start of the file is no projector.
    var broken = projector(movie(50))
    broken.replaceSubrange(broken.count - 4 ..< broken.count, with: withUnsafeBytes(of: UInt32(5000).littleEndian) { Data($0) })
    try broken.write(to: root.appendingPathComponent("Broken.exe"))
    #expect(FlashGame.projectorMovie(root.appendingPathComponent("Broken.exe")) == nil)
}

@Test func theGameStartsWithItsProjectorOrItsMainMovie() throws {
    let root = try game([
        "Game/Game.exe": projector(movie()),
        "Game/Game.app/Contents/Resources/movie.swf": movie(),
        "Game/data/title.swf": movie(),
        "unins000.exe": Data("MZ".utf8) + Data(count: 200),
    ])
    defer { try? FileManager.default.removeItem(at: root) }
    let flash = try #require(FlashGame(folder: root))
    #expect(flash.main == .projector(path: "Game/Game.exe", movie: FileSlice(offset: 100, length: 64)))
    #expect(flash.movies["game/data/title.swf"] == "Game/data/title.swf")
    // Movies deeper than the projector are the scenes it loads, not other ways in.
    #expect(flash.choices == [flash.main])

    let movies = try game([
        "Loader.swf": movie(10), "Game.swf": movie(5000), "Gallery.swf": movie(4000), "sub/scene.swf": movie(9000),
        "Game.html": html("<embed src=\"Game.swf\">"), "notes.txt": html("not flash"),
    ])
    defer { try? FileManager.default.removeItem(at: movies) }
    let collection = try #require(FlashGame(folder: movies))
    // The largest movie at the top, and no page that only wraps one movie.
    #expect(collection.main == .movie(path: "Game.swf"))
    #expect(collection.choices.map(\.path) == ["Game.swf", "Gallery.swf", "Loader.swf"])
}

@Test func aCollectionsMenuPageComesFirst() throws {
    let root = try game([
        "ECM5.exe": projector(movie()),
        "MAIN.html": html("<a href=\"ecm1.html\">1</a><a href=\"flash/4/four.swf\">4</a>"),
        "ecm1.html": html("<embed src=\"flash/1/01.swf\"><embed src=\"flash/1/bar.swf\">"),
        "flash/1/01.swf": movie(), "flash/1/bar.swf": movie(), "flash/4/four.swf": movie(),
    ])
    defer { try? FileManager.default.removeItem(at: root) }
    let flash = try #require(FlashGame(folder: root))
    #expect(flash.main == .page(path: "MAIN.html"))
    #expect(flash.choices.map(\.path) == ["MAIN.html", "ECM5.exe", "ecm1.html"])
    #expect(FlashPlugin.startPage(for: flash.main) == "/MAIN.html")
}

@Test func detectionIsCertainForProjectorsAndFindsThemFolderDeep() throws {
    let deep = try game(["Wrapper/Circle/RJ000000/game.exe": projector(movie()), "Wrapper/Circle/RJ000000/dat/a.swf": movie()])
    let movieOnly = try game(["game.swf": movie(), "readme.txt": html("")])
    let other = try game(["Game.exe": Data("MZ".utf8) + Data(count: 64), "www/index.html": html("<html>")])
    defer { [deep, movieOnly, other].forEach { try? FileManager.default.removeItem(at: $0) } }
    let plugin = FlashPlugin()
    let match = try #require(plugin.detect(FolderProbe(scanning: deep)))
    #expect(match.confidence == .certain && match.evidence == "Wrapper/Circle/RJ000000/game.exe")
    #expect(plugin.detect(FolderProbe(scanning: movieOnly))?.confidence == .likely)
    #expect(plugin.detect(FolderProbe(scanning: other)) == nil)
}

@Test func startPagesEscapeTheMoviePath() {
    #expect(FlashPlugin.startPage(for: .movie(path: "sub/My Game 01.swf"))
        == "/__vndegen/flash.html?movie=/sub/My%20Game%2001.swf")
    #expect(FlashPlugin.startPage(for: .projector(path: "打倒!/Game.exe", movie: FileSlice(offset: 0, length: 1)))
        == "/__vndegen/flash.html?movie=/%E6%89%93%E5%80%92%21/Game.exe")
}

@Test func sharedObjectsAreSolFilesNamedByTheirMovie() throws {
    let root = try game([:])
    defer { try? FileManager.default.removeItem(at: root) }
    let store = FlashPlugin.storage(root)
    let sol = Data([0x00, 0xBF, 0x00, 0x00, 0x00, 0x10, 0x54, 0x43, 0x53, 0x4F])
    try store.set("game/sub/scene.swf/save_data", sol.base64EncodedString())
    try store.set("game//root_save", sol.base64EncodedString())
    try store.set("game/game.swf/#folder/slot", sol.base64EncodedString())
    try store.set("volume", "0.5")

    let saves = root.appendingPathComponent("SharedObjects")
    #expect(try Data(contentsOf: saves.appendingPathComponent("sub/scene.swf/save_data.sol")) == sol)
    #expect(try Data(contentsOf: saves.appendingPathComponent("root_save.sol")) == sol)
    #expect(try Data(contentsOf: saves.appendingPathComponent("game.swf/#folder/slot.sol")) == sol)
    #expect(!FileManager.default.fileExists(atPath: saves.appendingPathComponent("volume").path))

    let loaded = try store.load()
    #expect(loaded == [
        "game/sub/scene.swf/save_data": sol.base64EncodedString(), "game//root_save": sol.base64EncodedString(),
        "game/game.swf/#folder/slot": sol.base64EncodedString(), "volume": "0.5",
    ])
    try store.remove("game//root_save")
    #expect(try store.load()["game//root_save"] == nil)
    try store.clear()
    #expect(try store.load().isEmpty)

    // Keys of other hosts and keys that would leave the folder go to the other store.
    #expect(SharedObjectStore.file(forKey: "other.example/game.swf/save") == nil)
    #expect(SharedObjectStore.file(forKey: "game/../escape") == nil)
    #expect(SharedObjectStore.file(forKey: "game/.hidden/save") == nil)
    #expect(throws: (any Error).self) { try store.set("game/game.swf/save", "not base64!") }
}

@Test func savesFromThePCMapToTheMovieThatMadeThem() {
    let movies = ["game.swf": "Game.swf", "sub/scene.swf": "sub/Scene.swf", "game.exe": "Game.exe"]
    let flashPlayer = "C:/Users/Player/AppData/Roaming/Macromedia/Flash Player/#SharedObjects/AB12CD34/localhost"
    #expect(FlashPlugin.savePath("\(flashPlayer)/Games/Test Game 01/Game.swf/save.sol", movies: movies) == "Game.swf/save.sol")
    #expect(FlashPlugin.savePath("\(flashPlayer)/Games/Test Game 01/sub/scene.swf/save.sol", movies: movies) == "sub/Scene.swf/save.sol")
    #expect(FlashPlugin.savePath("\(flashPlayer)/Games/Test Game 01/game.exe/#data/slot1.sol", movies: movies) == "Game.exe/#data/slot1.sol")
    #expect(FlashPlugin.savePath("\(flashPlayer)/save.sol", movies: movies) == "save.sol")
    #expect(FlashPlugin.savePath("C:/x/Flash Player/#SharedObjects/AB12CD34/#localWithNet/Games/game.swf/s.sol", movies: movies) == "Game.swf/s.sol")
    #expect(FlashPlugin.savePath("\(flashPlayer)/Games/Other Game/other.swf/save.sol", movies: movies) == nil)
    #expect(FlashPlugin.savePath("\(flashPlayer)/Games/Test Game 01/Game.swf/settings.txt", movies: movies) == nil)
    // Ruffle on the PC, and VNDegen on another device.
    #expect(FlashPlugin.savePath("C:/Users/Player/AppData/Roaming/ruffle/SharedObjects/localhost/Games/Game.swf/save.sol", movies: movies) == "Game.swf/save.sol")
    #expect(FlashPlugin.savePath("/var/mobile/Data/GameData/test/SharedObjects/sub/Scene.swf/save.sol", movies: movies) == "sub/Scene.swf/save.sol")
    #expect(FlashPlugin.savePath("/var/mobile/Data/GameData/test/SharedObjects/../save.sol", movies: movies) == nil)
}

@Test func thePlayerPicksWhichEntryStartsTheGame() throws {
    let root = try game(["Loader.swf": movie(10), "Game.swf": movie(5000)])
    let data = try game([:])
    defer { [root, data].forEach { try? FileManager.default.removeItem(at: $0) } }
    let plugin = FlashPlugin(), record = Game(id: GameID("test"))
    let flash = try #require(FlashGame(folder: root))
    let variants = try #require(plugin.variants(for: record, folder: root, dataDirectory: data))
    #expect(variants.automatic == "Game.swf" && variants.selected == nil)
    #expect(variants.options.map(\.id) == ["Game.swf", "Loader.swf"])

    try plugin.selectVariant("Loader.swf", for: record, dataDirectory: data)
    #expect(FlashPlugin.entry(for: flash, dataDirectory: data) == .movie(path: "Loader.swf"))
    try plugin.selectVariant(nil, for: record, dataDirectory: data)
    #expect(FlashPlugin.entry(for: flash, dataDirectory: data) == .movie(path: "Game.swf"))
}
