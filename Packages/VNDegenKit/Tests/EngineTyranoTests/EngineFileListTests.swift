import EngineTyrano
import Foundation
import Testing
@testable import VNCore

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

    @MainActor func library() async -> LibraryStore {
        let library = LibraryStore(paths: paths, engines: [TyranoPlugin()])
        library.deletesToTrash = false
        await library.refresh()
        return library
    }
}

/// Laid out as Libido Estates and Welcome to Paradise Island are, with saves beside the executable.
@Test @MainActor func anElectronGameLosesElectronAndKeepsTheGame() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    let electron = ["icudtl.dat", "resources.pak", "chrome_100_percent.pak", "chrome_200_percent.pak", "LICENSES.chromium.html", "libEGL.dll",
                    "libGLESv2.dll", "ffmpeg.dll", "d3dcompiler_47.dll", "snapshot_blob.bin", "v8_context_snapshot.bin",
                    "swiftshader/libEGL.dll", "swiftshader/libGLESv2.dll", "locales/en-US.pak", "resources/electron.asar"]
    let kept = ["resources/app/tyrano/plugins/kag/kag.js", "resources/app/index.html", "resources/app/data/system/Config.tjs",
                "resources/app/package.json", "paradise_island_sf.sav", "SOVEREIGN64.dll", "version", "tools.exe"]
    for path in electron + kept { try scratch.write(path) }
    try scratch.write("paradise_island.exe", size: 104_700_000)

    let library = await scratch.library()
    #expect(library.game(scratch.id)?.detected?.engine == .tyrano)
    let plan = await library.engineFilePlan(for: scratch.id)
    #expect(plan.files.contains { $0.path == "paradise_island.exe" })
    let result = await library.removeEngineFiles(plan.files, of: scratch.id)
    #expect(result.failed.isEmpty)
    for path in kept { #expect(scratch.exists(path), "\(path)") }
    for path in electron + ["paradise_island.exe"] { #expect(!scratch.exists(path), "\(path)") }
    #expect(library.game(scratch.id)?.detected?.engine == .tyrano)
    #expect(await library.engineFilePlan(for: scratch.id).files.isEmpty)
}

/// Laid out as Butterfly Affection is: the page at the top of the folder, in old NW.js, under the game's name.
@Test @MainActor func anNWjsGameKeepsWhatItsPagesAndSavesNeed() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    let runtime = ["nw.pak", "icudtl.dat", "d3dcompiler_47.dll", "libGLESv2.dll", "libEGL.dll", "ffmpegsumo.dll"]
    let kept = ["tyrano/plugins/kag/kag.js", "index.html", "package.json", "data/system/Config.tjs", "node_modules/fs-extra/lib/index.js",
                "ButterflyAffection_sf.sav", "start.command", "tyrano_32bit/tyrano.js", "Update_History.txt"]
    for path in runtime + kept { try scratch.write(path) }
    try scratch.write("ButterflyAffection.exe", size: 46_300_000)

    let library = await scratch.library()
    let plan = await library.engineFilePlan(for: scratch.id)
    #expect(Set(plan.files.map(\.path)) == Set(runtime + ["ButterflyAffection.exe"]))
    _ = await library.removeEngineFiles(plan.files, of: scratch.id)
    for path in kept { #expect(scratch.exists(path), "\(path)") }
    #expect(library.game(scratch.id)?.detected?.engine == .tyrano)
}

/// A page with no desktop runtime has nothing to remove.
@Test @MainActor func aBrowserBuildOffersNothing() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    for path in ["tyrano/plugins/kag/kag.js", "index.html", "data/system/Config.tjs", "tool.exe"] { try scratch.write(path) }
    let library = await scratch.library()
    #expect(await library.engineFilePlan(for: scratch.id).files.isEmpty)
}

/// A `kag.js` left behind doesn't show the game is on disk. Its data does.
@Test @MainActor func aLeftoverEngineFileBesideAPackedExeOffersNothing() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    try scratch.write("Game.exe", size: 900_000_000)
    for path in ["tyrano/plugins/kag/kag.js", "nw.pak", "package.json"] { try scratch.write(path) }
    let library = await scratch.library()
    #expect(library.game(scratch.id)?.detected?.engine == .tyrano)
    #expect(await library.engineFilePlan(for: scratch.id).files.isEmpty)
}

/// The library's `TyranoScript (packed in exe)` games show no `kag.js` on disk, so detection finds nothing.
@Test @MainActor func aGameAppendedToItsExeOffersNothing() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    try scratch.write("Game.exe", size: 900_000_000)
    for path in ["nw.pak", "icudtl.dat", "package.json"] { try scratch.write(path) }
    let library = await scratch.library()
    #expect(!library.offersEngineFiles(scratch.id))
    #expect(await library.engineFilePlan(for: scratch.id).files.isEmpty)
    #expect(scratch.exists("Game.exe"))
}
