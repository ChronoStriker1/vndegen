import EngineRenPy
import Foundation
import Testing
@testable import VNCore

private struct Scratch {
    let base = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
    var paths: LibraryPaths { LibraryPaths(gamesRoot: base.appendingPathComponent("Games"), support: base.appendingPathComponent("Support")) }
    var game: URL { paths.gamesRoot.appendingPathComponent("Game", isDirectory: true) }
    let id = GameID("Game")

    func write(_ path: String, _ text: String = "x") throws {
        let url = game.appendingPathComponent(path)
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data(text.utf8).write(to: url)
    }

    func exists(_ path: String) -> Bool { FileManager.default.fileExists(atPath: game.appendingPathComponent(path).path) }

    @MainActor func library() async -> LibraryStore {
        let library = LibraryStore(paths: paths, engines: [RenPyPlugin()])
        library.deletesToTrash = false
        await library.refresh()
        return library
    }
}

private let version8 = "version_tuple = (8, 3, 4, vc_version)\n"
/// Ren'Py 7.5 to 8.1 ship one `renpy/` for both lines, so only `lib/` or the compiled files say which one a game uses.
private let bothLines = "version_tuple = (7, 6, 3, vc_version)\nversion_tuple = (8, 1, 3, vc_version)\n"

@Test @MainActor func aGameLosesItsLibrariesAndLaunchersAndKeepsItsSaves() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    try scratch.write("Outer/renpy/__init__.py", version8)
    let kept = ["Outer/game/script.rpyc", "Outer/game/saves/1-1-LT1.save", "Outer/game/saves/persistent", "Outer/game/python-packages/a.py",
                "Outer/Game.py", "Outer/Game.sh", "Outer/winmm.dll", "Outer/sovereign64.dll", "Outer/lib/custom/mod.py", "Outer/log.txt", "Outer/tool.exe"]
    let gone = ["Outer/Game.exe", "Outer/Game-32.exe", "Outer/Game.app/Contents/MacOS/Game", "Outer/lib/py3-windows-x86_64/python.exe",
                "Outer/lib/py3-linux-x86_64/python", "Outer/lib/python3.9/os.py"]
    for path in kept + gone { try scratch.write(path) }

    let library = await scratch.library()
    let before = library.game(scratch.id)?.detected
    #expect(before?.engine == .renPy)
    let plan = await library.engineFilePlan(for: scratch.id)
    #expect(Set(plan.files.map(\.path)) == ["Outer/Game.exe", "Outer/Game-32.exe", "Outer/Game.app", "Outer/lib/py3-windows-x86_64",
                                            "Outer/lib/py3-linux-x86_64", "Outer/lib/python3.9"])
    let result = await library.removeEngineFiles(plan.files, of: scratch.id)
    #expect(result.failed.isEmpty)
    for path in kept + ["Outer/renpy/__init__.py"] { #expect(scratch.exists(path), "\(path)") }
    for path in gone { #expect(!scratch.exists(path), "\(path)") }
    // Still the same game on the same version, on the removal's rescan and on a fresh launch.
    #expect(library.game(scratch.id)?.detected == before)
    let relaunched = await scratch.library()
    #expect(relaunched.game(scratch.id)?.detected == before)
    let folder = library.folder(for: scratch.id)
    #expect(RenPyLayout(FolderProbe(scanning: folder))?.runtime == .renPy8)
}

/// All of `lib/` goes as one folder when nothing else is in it.
@Test @MainActor func aLibFolderOfOnlyPlatformsGoesWhole() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    try scratch.write("renpy/__init__.py", version8)
    for path in ["game/script.rpy", "lib/py3-windows-x86_64/a", "lib/python3.9/b"] { try scratch.write(path) }
    let library = await scratch.library()
    #expect(await library.engineFilePlan(for: scratch.id).files.map(\.path) == ["lib"])
}

@Test @MainActor func libStaysWhenItsNamesAloneTellTheLineApart() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    try scratch.write("renpy/__init__.py", bothLines)
    for path in ["game/script.rpy", "lib/py2-windows-x86_64/a", "Game.py", "Game.exe"] { try scratch.write(path) }
    let library = await scratch.library()
    #expect(RenPyLayout(FolderProbe(scanning: library.folder(for: scratch.id)))?.evidence == .libraries)
    #expect(await library.engineFilePlan(for: scratch.id).files.map(\.path) == ["Game.exe"])
    #expect(scratch.exists("lib/py2-windows-x86_64/a"))
}

@Test @MainActor func libGoesWhenTheCompiledFilesTellTheSameLine() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    try scratch.write("renpy/__init__.py", bothLines)
    for path in ["renpy/__pycache__/a.pyc", "game/script.rpy", "lib/py3-windows-x86_64/a"] { try scratch.write(path) }
    let library = await scratch.library()
    let layout = RenPyLayout(FolderProbe(scanning: library.folder(for: scratch.id)))
    #expect(layout?.evidence == .libraries && layout?.runtime == .renPy8)
    let plan = await library.engineFilePlan(for: scratch.id)
    #expect(plan.files.map(\.path) == ["lib"])
    _ = await library.removeEngineFiles(plan.files, of: scratch.id)
    let after = RenPyLayout(FolderProbe(scanning: library.folder(for: scratch.id)))
    #expect(after?.runtime == .renPy8 && after?.evidence == .compiledFiles)
}

/// A Mac build can hold the game, and then it isn't a launcher.
@Test @MainActor func anAppHoldingTheGameStays() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    try scratch.write("renpy/__init__.py", version8)
    for path in ["game/script.rpy", "Game.py", "Game.app/Contents/Resources/autorun/game/script.rpyc"] { try scratch.write(path) }
    let library = await scratch.library()
    #expect(await library.engineFilePlan(for: scratch.id).files.isEmpty)
}

/// A PC build has one `.py`. A second may be a mod's, so its `.exe` could be a mod's tool too.
@Test @MainActor func twoScriptsLeaveEveryLauncher() async throws {
    let scratch = Scratch()
    defer { try? FileManager.default.removeItem(at: scratch.base) }
    try scratch.write("renpy/__init__.py", version8)
    for path in ["game/script.rpy", "Game.py", "Game.exe", "Patch.py", "Patch.exe"] { try scratch.write(path) }
    let library = await scratch.library()
    #expect(await library.engineFilePlan(for: scratch.id).files.isEmpty)
}
