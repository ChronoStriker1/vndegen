import Foundation
import Testing
import VNCore
import WebRuntime
import FontPack
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

@Test func startPagesCarryTheMoviesURLPath() {
    // The URL path, escaped once more for the query, so flash.js reads the URL path back.
    #expect(FlashPlugin.urlPath("sub/My Game 01.swf") == "/sub/My%20Game%2001.swf")
    #expect(FlashPlugin.urlPath("Hash#Game/100% Done?.swf") == "/Hash%23Game/100%25%20Done%3F.swf")
    #expect(FlashPlugin.urlPath("打倒!/Game.exe") == "/%E6%89%93%E5%80%92!/Game.exe")
    #expect(FlashPlugin.startPage(for: .movie(path: "sub/My Game 01.swf")) == "/__vndegen/flash.html?movie=/sub/My%2520Game%252001.swf")
    #expect(FlashPlugin.startPage(for: .projector(path: "打倒!/Game.exe", movie: FileSlice(offset: 0, length: 1)))
        == "/__vndegen/flash.html?movie=/%25E6%2589%2593%25E5%2580%2592%21/Game.exe")
    #expect(FlashPlugin.startPage(for: .page(path: "My Menu.html")) == "/My%20Menu.html")
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
    // Ruffle percent-encodes the movie's path, as its URL has it, and not the shared object's own name.
    try store.set("game/My%20Game/%E6%89%93%E5%80%92.swf/セーブ", sol.base64EncodedString())

    let saves = root.appendingPathComponent("SharedObjects")
    #expect(try Data(contentsOf: saves.appendingPathComponent("sub/scene.swf/save_data.sol")) == sol)
    #expect(try Data(contentsOf: saves.appendingPathComponent("root_save.sol")) == sol)
    #expect(try Data(contentsOf: saves.appendingPathComponent("game.swf/#folder/slot.sol")) == sol)
    #expect(try Data(contentsOf: saves.appendingPathComponent("My Game/打倒.swf/セーブ.sol")) == sol)
    #expect(!FileManager.default.fileExists(atPath: saves.appendingPathComponent("volume").path))

    let loaded = try store.load()
    #expect(loaded == [
        "game/sub/scene.swf/save_data": sol.base64EncodedString(), "game//root_save": sol.base64EncodedString(),
        "game/game.swf/#folder/slot": sol.base64EncodedString(), "volume": "0.5",
        "game/My%20Game/%E6%89%93%E5%80%92.swf/セーブ": sol.base64EncodedString(),
    ])
    try store.remove("game//root_save")
    #expect(try store.load()["game//root_save"] == nil)
    try store.clear()
    #expect(try store.load().isEmpty)

    // Each key comes back as Ruffle asks for it: a literal %, a folder that starts with #, which starts with %23 on disk
    // so it never reads as a name, names with slashes for a movie, a folder, and the local path /, and a name with a
    // movie's extension in it.
    for (key, file) in [
        ("game/My%25Game/movie.swf/save", "My%Game/movie.swf/save"),
        ("game/%23bonus/movie.swf/save", "%23bonus/movie.swf/save"),
        ("game/%23bonus/save", "%23bonus/save"),
        ("game//#bonus/save", "#bonus/save"),
        ("game//#saves/slot", "#saves/slot"),
        ("game/Sub/#saves/chapter/slot", "Sub/#saves/chapter/slot"),
        ("game/game.swf/#chapter.swf/slot", "game.swf/#chapter.swf/slot"),
        ("game/Folder!/game.swf/save", "Folder!/game.swf/save"),
        // A real folder named %23bonus, or %2523bonus, gets one more 25, so it never reads back as #bonus.
        ("game/%2523bonus/movie.swf/save", "%2523bonus/movie.swf/save"),
        ("game/%252523bonus/movie.swf/save", "%252523bonus/movie.swf/save"),
        ("game/100%25/movie.swf/save", "100%/movie.swf/save"),
    ] {
        #expect(SharedObjectStore.file(forKey: key) == file)
        #expect(SharedObjectStore.key(forFile: file) == key)
    }

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

@Test func genericFontsAreWindowsLatinFontsWithJapaneseFallbacks() throws {
    // FontPack's state is process-wide, so this configures the folders FontPackTests and WebRuntime's FontTests do,
    // which run at the same time: the bundled fonts, then empty files named as the Korean download's.
    let fonts = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
        .appending(path: "../../../../Engines/Shared/fonts").standardizedFileURL
    let downloaded = FileManager.default.temporaryDirectory.appending(path: "vndegen-font-tests")
    try FileManager.default.createDirectory(at: downloaded, withIntermediateDirectories: true)
    for name in ["SourceHanSansKR-Regular.otf", "SourceHanSansKR-Bold.otf"] {
        try Data().write(to: downloaded.appending(path: name), options: .atomic)
    }
    #expect(FontPack.configure(folders: [fonts, downloaded]))
    let families = FlashFonts.families()
    let sans = try #require(families["sans"])
    #expect(sans.name == "VNDegen Flash sans")
    #expect(sans.faces == [
        FlashFonts.Face(local: "Arial", file: "LiberationSans-Regular.ttf", bold: false, latin: true),
        FlashFonts.Face(local: "Arial", file: "LiberationSans-Bold.ttf", bold: true, latin: true),
        FlashFonts.Face(local: "MS UI Gothic", file: "ume-ugo4.ttf", bold: false, latin: false),
    ])
    #expect(families["serif"]?.faces.map(\.file) == ["LiberationSerif-Regular.ttf", "LiberationSerif-Bold.ttf", "ume-pmo3.ttf"])
    #expect(families["typewriter"]?.faces.map(\.file) == ["LiberationMono-Regular.ttf", "LiberationMono-Bold.ttf", "ume-tgo4.ttf"])

    let script = try FlashFonts.pageScript("const fonts = __VNDEGEN_FLASH_FONTS__;")
    #expect(script.contains("vndegen:\\/\\/game\\/.vndegen\\/fonts\\/") || script.contains("vndegen://game/.vndegen/fonts/"))
    #expect(!script.contains("__VNDEGEN_FLASH_FONTS__"))
}

@Test func launcherPagesAreClassifiedByTheMoviesTheyReach() throws {
    let root = try game([
        // A menu that links only to episode pages, which embed the movies.
        "index.html": html("<a href='episodes/one.html'>1</a> <a href=\"episodes/two.html?x=1\">2</a> <a href='readme.html'>?</a>"),
        "episodes/one.html": html("<object data='../flash/one.swf'><embed src='../flash/one.swf'></object><a href='../index.html'>Back</a>"),
        "episodes/two.html": html("<embed src=\"../flash/two.swf\"><a href='../index.html'>Back</a>"),
        "readme.html": html("<p>no movies</p>"),
        // The usual <object> with an <embed> fallback names one movie twice: a wrapper, not a player.
        "play.html": html("<object><param name='movie' value='flash/one.swf'><embed src='flash/one.swf'></object>"),
        "flash/one.swf": movie(), "flash/two.swf": movie(),
    ])
    defer { try? FileManager.default.removeItem(at: root) }
    let flash = try #require(FlashGame(folder: root))
    #expect(flash.main == .page(path: "index.html"))
    #expect(!flash.entries.contains(.page(path: "play.html")))
    #expect(!flash.entries.contains(.page(path: "readme.html")))
    #expect(flash.folders == ["flash": "flash"])
}

@Test func projectorsWrappedDeepAreFoundAmongOtherFiles() throws {
    var files: [String: Data] = ["Circle/Wrapper/RJ000000/game.exe": projector(movie())]
    for index in 0 ..< 40 { files["Circle/notes \(index).txt"] = html("") }
    let root = try game(files)
    defer { try? FileManager.default.removeItem(at: root) }
    let match = try #require(FlashPlugin().detect(FolderProbe(scanning: root)))
    #expect(match.confidence == .certain && match.evidence == "Circle/Wrapper/RJ000000/game.exe")
}

@Test func savesScopedToAFolderOrFromRuffleMapToo() {
    let movies = ["sub/game.swf": "Sub/Game.swf", "my game/movie.swf": "My Game/movie.swf"]
    let folders = ["sub": "Sub", "my game": "My Game"]
    let flashPlayer = "C:/Users/Player/AppData/Roaming/Macromedia/Flash Player/#SharedObjects/AB12CD34/localhost"
    // A movie in sub/ that asked for its folder as the local path.
    #expect(FlashPlugin.savePath("\(flashPlayer)/Games/Test Game 01/sub/save.sol", movies: movies, folders: folders) == "Sub/save.sol")
    #expect(FlashPlugin.savePath("\(flashPlayer)/Games/Test Game 01/sub/#slots/one.sol", movies: movies, folders: folders) == "Sub/#slots/one.sol")
    // A folder is no match for a movie this game doesn't have.
    #expect(FlashPlugin.savePath("\(flashPlayer)/Games/Test Game 01/sub/other.swf/save.sol", movies: movies, folders: folders) == nil)
    // Names with slashes in them, saved for a folder or for /.
    #expect(FlashPlugin.savePath("\(flashPlayer)/Games/Test Game 01/sub/#saves/chapter/slot.sol", movies: movies, folders: folders) == "Sub/#saves/chapter/slot.sol")
    #expect(FlashPlugin.savePath("\(flashPlayer)/#saves/slot.sol", movies: movies, folders: folders) == "#saves/slot.sol")
    // A name with slashes in it, saved for /, stays one even when a movie's name follows it.
    #expect(FlashPlugin.savePath("\(flashPlayer)/#slots/game.swf/slot.sol", movies: ["game.swf": "Game.swf"], folders: [:]) == "#slots/game.swf/slot.sol")
    // A real folder named %23bonus imports as itself.
    #expect(FlashPlugin.savePath("\(flashPlayer)/Games/Test Game 01/%23bonus/extra.swf/save.sol",
                                 movies: ["%23bonus/extra.swf": "%23bonus/extra.swf"], folders: [:]) == "%2523bonus/extra.swf/save.sol")
    // A PC folder that starts with # is a folder when a movie follows it, and a game folder that does is %23 on disk.
    #expect(FlashPlugin.savePath("\(flashPlayer)/#Games/Test Game 01/sub/game.swf/save.sol", movies: movies, folders: folders) == "Sub/Game.swf/save.sol")
    #expect(FlashPlugin.savePath("\(flashPlayer)/Games/Test Game 01/#bonus/extra.swf/save.sol",
                                 movies: ["#bonus/extra.swf": "#bonus/extra.swf"], folders: ["#bonus": "#bonus"]) == "%23bonus/extra.swf/save.sol")
    // Ruffle on the PC percent-encodes the movie's path.
    #expect(FlashPlugin.savePath("C:/Users/Player/AppData/Roaming/ruffle/SharedObjects/localhost/Games/My%20Game/movie.swf/s.sol",
                                 movies: movies, folders: folders) == "My Game/movie.swf/s.sol")
}

@Test func aMenuOfProjectorsIsAnEntry() throws {
    let root = try game([
        "Menu.html": html("<a href='Episode1.exe'>1</a> <a href=\"Episode2.exe\">2</a> <a href='setup.exe'>setup</a>"),
        "Episode1.exe": projector(movie()), "Episode2.exe": projector(movie()), "setup.exe": Data("MZ".utf8) + Data(count: 64),
    ])
    defer { try? FileManager.default.removeItem(at: root) }
    let flash = try #require(FlashGame(folder: root))
    #expect(flash.main == .page(path: "Menu.html"))
}

@Test func theDeepScanStopsAtItsBudget() throws {
    var files: [String: Data] = ["a/b/c/z/game.exe": projector(movie())]
    for index in 0 ..< 50 { files["a/b/c/file\(index).txt"] = html("") }
    let root = try game(files)
    defer { try? FileManager.default.removeItem(at: root) }
    let probe = FolderProbe(scanning: root)
    #expect(FlashPlugin.wrapsFlash(probe))
    #expect(!FlashPlugin.wrapsFlash(probe, budget: 10))
}
