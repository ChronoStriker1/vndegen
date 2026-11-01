import EngineRPGMaker
import Foundation
import Testing
@testable import VNCore

/// A game folder laid out as a library game's is, in a library of its own.
private struct Scratch {
    let base = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
    var paths: LibraryPaths { LibraryPaths(gamesRoot: base.appendingPathComponent("Games"), support: base.appendingPathComponent("Support")) }
    var game: URL { paths.gamesRoot.appendingPathComponent("Game", isDirectory: true) }
    let id = GameID("Game")

    /// Written sparse, so a file the size of a runtime costs nothing.
    func write(_ path: String, size: UInt64 = 1) throws {
        let url = game.appendingPathComponent(path)
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        FileManager.default.createFile(atPath: url.path, contents: nil)
        let handle = try FileHandle(forWritingTo: url)
        try handle.truncate(atOffset: size)
        try handle.close()
    }

    func exists(_ path: String) -> Bool { FileManager.default.fileExists(atPath: game.appendingPathComponent(path).path) }

    @MainActor func library(_ flavor: RPGMakerPlugin.Flavor) async -> LibraryStore {
        let library = LibraryStore(paths: paths, engines: [RPGMakerPlugin(flavor)])
        library.deletesToTrash = false
        await library.refresh()
        return library
    }
}

private let runtime = ["Game.exe", "nw.dll", "node.dll", "nw.pak", "resources.pak", "icudtl.dat", "libEGL.dll", "libGLESv2.dll",
                       "ffmpeg.dll", "d3dcompiler_47.dll", "notification_helper.exe", "v8_context_snapshot.bin",
                       "nw_100_percent.pak", "nw_200_percent.pak", "swiftshader/libEGL.dll", "swiftshader/libGLESv2.dll",
                       "locales/en-US.pak", "locales/en-US.pak.info", "pnacl/a_nexe"]

/// What a mod, a translation, or the game brought, and its saves. Nothing here may ever be offered.
private let mine = ["package.json", "Game_en.exe", "ym.exe", "winmm.dll", "sovereign.dll", "lib/greenworks-win64.node", "lib/steam_api64.dll",
                    "Readme.txt", "save/file1.rpgsave", "translation-table.json"]

@Test @MainActor func anMVGameLosesItsRuntimeAndKeepsItsSavesAndData() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    let game = ["www/js/rpg_core.js", "www/index.html", "www/data/System.json", "www/img/a.png", "www/save/file1.rpgsave", "www/save/global.rpgsave"]
    for path in runtime + mine + game { try scratch.write(path) }
    try scratch.write("credits.html", size: 900_000)

    let library = await scratch.library(.mv)
    #expect(library.game(scratch.id)?.detected?.engine == .rpgMakerMV)
    let plan = await library.engineFilePlan(for: scratch.id)
    #expect(plan.problem == nil)
    #expect(Set(plan.files.map(\.path)) == Set(runtime.filter { !$0.contains("/") } + ["credits.html", "locales", "pnacl", "swiftshader"]))

    let result = await library.removeEngineFiles(plan.files, of: scratch.id)
    #expect(result.failed.isEmpty)
    for path in mine + game { #expect(scratch.exists(path), "\(path)") }
    for path in runtime { #expect(!scratch.exists(path), "\(path)") }
    // Still the same game, on the rescan the removal ran and on a fresh launch.
    #expect(library.game(scratch.id)?.detected?.engine == .rpgMakerMV)
    let relaunched = await scratch.library(.mv)
    #expect(relaunched.game(scratch.id)?.detected?.engine == .rpgMakerMV)
    #expect(await relaunched.engineFilePlan(for: scratch.id).files.isEmpty)
}

@Test @MainActor func anMZGameKeepsItsOwnFilesInTheRuntimesFolders() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    // The library has an MZ game with translations in locales, and MZ's web root is the top of the game.
    let game = ["js/rmmz_core.js", "index.html", "data/System.json", "save/file1.rmmzsave", "locales/en.json", "locales/ja.json"]
    for path in runtime + mine + game { try scratch.write(path) }
    let library = await scratch.library(.mz)
    let plan = await library.engineFilePlan(for: scratch.id)
    #expect(plan.files.contains { $0.path == "locales/en-US.pak" })
    #expect(!plan.files.contains { $0.path == "locales" })
    _ = await library.removeEngineFiles(plan.files, of: scratch.id)
    for path in mine + game { #expect(scratch.exists(path), "\(path)") }
    #expect(!scratch.exists("locales/en-US.pak"))
}

/// A game's own credits page is far smaller than NW.js's, the smallest of which in the library is 870 KB.
@Test @MainActor func aSmallCreditsPageIsTheGames() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    for path in ["js/rmmz_core.js", "nw.dll"] { try scratch.write(path) }
    try scratch.write("credits.html", size: 401)
    let library = await scratch.library(.mz)
    #expect(await library.engineFilePlan(for: scratch.id).files.map(\.path) == ["nw.dll"])
}

/// A game packed inside its `.exe` shows no engine core on disk, so detection finds nothing and nothing is offered,
/// whatever NW.js files sit beside it.
@Test @MainActor func aGameAppendedToItsExeOffersNothing() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    try scratch.write("Game.exe", size: 400_000_000)
    for path in ["nw.dll", "nw.pak", "icudtl.dat", "package.json"] { try scratch.write(path) }
    let library = await scratch.library(.mv)
    #expect(library.game(scratch.id)?.detected == nil)
    #expect(!library.offersEngineFiles(scratch.id))
    #expect(await library.engineFilePlan(for: scratch.id).files.isEmpty)
    #expect(scratch.exists("Game.exe"))
}

/// Without NW.js's own files, `Game.exe` could be anything.
@Test @MainActor func namesAloneDontProveItsNWjs() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    for path in ["js/rmmz_core.js", "Game.exe", "icudtl.dat", "d3dcompiler_47.dll"] { try scratch.write(path) }
    let library = await scratch.library(.mz)
    #expect(await library.engineFilePlan(for: scratch.id).files.isEmpty)
}

@Test @MainActor func theOneLargeExeIsTheRuntimeWhateverItsName() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    for path in ["js/rmmz_core.js", "nw.pak", "game_en.exe"] { try scratch.write(path) }
    try scratch.write("ButterflyAffection.exe", size: 46_000_000)
    let library = await scratch.library(.mz)
    #expect(Set(await library.engineFilePlan(for: scratch.id).files.map(\.path)) == ["ButterflyAffection.exe", "nw.pak"])
}

@Test @MainActor func twoLargeExesBothStayEvenUnderAListedName() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    for path in ["js/rmmz_core.js", "nw.pak"] { try scratch.write(path) }
    try scratch.write("Game.exe", size: 46_000_000)
    try scratch.write("Game_JP.exe", size: 46_000_000)
    let library = await scratch.library(.mz)
    #expect(await library.engineFilePlan(for: scratch.id).files.map(\.path) == ["nw.pak"])
}
