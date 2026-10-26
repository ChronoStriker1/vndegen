import Foundation
import Testing
import VNCore
import GameInput
@testable import EngineRenPy

/// Writes `files` (relative path to contents) into a fresh folder.
private func folder(_ files: [String: String]) throws -> URL {
    let root = FileManager.default.temporaryDirectory.appendingPathComponent("vndegen-renpy-\(UUID().uuidString)", isDirectory: true)
    for (path, contents) in files {
        let url = root.appendingPathComponent(path)
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data(contents.utf8).write(to: url)
    }
    return root
}

private func layout(_ files: [String: String]) throws -> RenPyLayout? {
    RenPyLayout(FolderProbe(scanning: try folder(files)))
}

private let dualLineInit = "if PY2:\n    version_tuple = (7, 5, 1, vc_version)\nelse:\n    version_tuple = (8, 0, 1, vc_version)\n"

@Test func detectsRenPy8InsideAnExtraFolder() throws {
    let probe = FolderProbe(scanning: try folder([
        "TestGame-1.0-pc/renpy/__init__.py": "version_tuple = VersionTuple(*(int(i) for i in version.split(\".\")))\n",
        "TestGame-1.0-pc/renpy/vc_version.py": "branch = 'fix'\nversion = '8.5.3.26051504'\n",
        "TestGame-1.0-pc/lib/py3-linux-x86_64/librenpython.so": "",
        "TestGame-1.0-pc/game/script.rpyc": "",
    ]))
    #expect(RenPyPlugin().detect(probe) == EngineMatch(engine: .renPy, confidence: .certain, version: "8.5.3", evidence: "TestGame-1.0-pc/renpy/"))
    #expect(RenPyLayout(probe) == RenPyLayout(base: "TestGame-1.0-pc", version: "8.5.3", runtime: .renPy8, evidence: .version))
}

@Test func picksTheLineOfADualLineRenPyFromItsLibraries() throws {
    let layout = try layout([
        "renpy/__init__.py": dualLineInit,
        "renpy/vc_version.py": "vc_version = 22070801\n",
        "lib/py2-linux-x86_64/librenpython.so": "",
        "game/archive.rpa": "",
    ])
    #expect(layout == RenPyLayout(base: "", version: "7.5.1", runtime: .renPy7, evidence: .libraries))
}

@Test func picksTheLineOfADualLineRenPyFromItsCompiledFiles() throws {
    let layout = try layout([
        "renpy/__init__.py": dualLineInit,
        "renpy/__pycache__/__init__.cpython-39.pyc": "",
        "game/script.rpyc": "",
    ])
    #expect(layout == RenPyLayout(base: "", version: "8.0.1", runtime: .renPy8, evidence: .compiledFiles))
}

@Test func runsRenPy73AndOlderOnRenPy7() throws {
    let layout = try layout([
        "renpy/__init__.py": "version_tuple = (7, 4, 2, vc_version)\n",
        "lib/linux-x86_64/librenpython.so": "",
        "lib/python2.7/site.pyo": "",
        "game/scripts.rpa": "",
    ])
    #expect(layout == RenPyLayout(base: "", version: "7.4.2", runtime: .renPy7, evidence: .version))
    #expect(RenPyLayout.runtime(libraries: ["linux-x86_64", "python2.7"]) == .renPy7)
}

@Test func fallsBackToCompiledFilesThenRenPy8() throws {
    #expect(try layout(["renpy/main.pyo": "", "game/script.rpy": ""])?.runtime == .renPy7)
    #expect(try layout(["renpy/main.py": "", "game/script.rpy": ""]) == RenPyLayout(base: "", version: nil, runtime: .renPy8, evidence: .fallback))
}

@Test func findsScriptsInASubfolderOfAGameOneFolderDown() throws {
    let layout = try layout([
        "TestGame-1.0-pc/renpy/__init__.py": "version_tuple = (7, 4, 2, vc_version)\n",
        "TestGame-1.0-pc/game/scripts/story.rpyc": "",
    ])
    #expect(layout == RenPyLayout(base: "TestGame-1.0-pc", version: "7.4.2", runtime: .renPy7, evidence: .version))
}

@Test func findsScriptsAnyDepthUnderGame() throws {
    let layout = try layout([
        "TestGame-1.0-pc/renpy/__init__.py": "version_tuple = (7, 4, 2, vc_version)\n",
        "TestGame-1.0-pc/game/scripts/act1/scenes/story.rpyc": "",
    ])
    #expect(layout?.base == "TestGame-1.0-pc")
}

@Test func theGameFolderScanStopsAfterItsEntryLimit() throws {
    let game = try folder(["images/a.png": "", "images/b.png": "", "images/c.png": "", "scripts/act1/story.rpy": ""])
    #expect(RenPyLayout.containsScript(game))
    #expect(!RenPyLayout.containsScript(game, limit: 2))
}

@Test func ignoresFoldersWithoutGameScripts() throws {
    #expect(try layout(["renpy/__init__.py": "", "game/images/bg.png": ""]) == nil)
    #expect(try layout(["js/rmmz_core.js": "", "game/script.rpy": ""]) == nil)
}

@Test func mapsUserSavesFromEveryPlatformIntoOneFolder() {
    let map = RenPyPlugin.userSaveRelativePath
    #expect(map("/home/deck/.renpy/Game-1505403882/1-1-LT1.save") == "1-1-LT1.save")
    #expect(map("C:/Users/player/AppData/Roaming/RenPy/Game-1505403882/persistent") == "persistent")
    #expect(map("/Users/player/Library/RenPy/Game-1505403882/sync/persistent") == "sync/persistent")
    #expect(map("C:/Users/player/AppData/Roaming/RenPy/tokens/security_keys.txt") == nil)
    #expect(map("/home/deck/.renpy/persistent") == nil)
    #expect(map("/home/deck/Games/NoSocks/save/file1.rmmzsave") == nil)
}

@Test func mapsGameFolderSavesIntoTheSameFolder() throws {
    let data = URL(fileURLWithPath: "/tmp/vndegen/GameData/Test Game")
    let locations = RenPyPlugin().saveLocations(folder: URL(fileURLWithPath: "/nonexistent"), dataDirectory: data)
    let location = try #require(locations.first)
    #expect(locations.count == 1)
    #expect(location.directory.path == "/tmp/vndegen/GameData/Test Game/saves")
    #expect(location.relativePath("C:/Games/Test Game/game/saves/1-1-LT1.save") == "1-1-LT1.save")
    #expect(location.relativePath("C:/Games/Test Game/game/saves") == nil)
    #expect(location.relativePath("/tmp/vndegen/GameData/Test Game/saves/auto-1-LT1.save") == "auto-1-LT1.save")
}

@Test func rejectsSavePathsThatLeaveTheSaveFolder() throws {
    let data = URL(fileURLWithPath: "/tmp/vndegen/GameData/Test Game")
    let location = try #require(RenPyPlugin().saveLocations(folder: URL(fileURLWithPath: "/nonexistent"), dataDirectory: data).first)
    #expect(location.relativePath("/home/deck/.renpy/Game-1505403882/../../../library.json") == nil)
    #expect(location.relativePath("C:/Games/Test Game/game/saves/../../game/script.rpy") == nil)
    #expect(location.relativePath("/tmp/vndegen/GameData/Test Game/saves/../renpy.json") == nil)
    #expect(location.relativePath("/tmp/vndegen/GameData/Test Game/saves/./1-1-LT1.save") == nil)
    #expect(location.relativePath("/VNDegen/Test Game/saves/../../x") == nil)
    #expect(location.relativePath("/home/deck/.renpy/Game-1505403882/sync/persistent") == "sync/persistent")
}

@Test func mapsTheSamePortablePathOnEveryInstall() throws {
    let here = try #require(RenPyPlugin().saveLocations(folder: URL(fileURLWithPath: "/nonexistent"),
                                                        dataDirectory: URL(fileURLWithPath: "/tmp/a/GameData/Test Game")).first)
    #expect(here.relativePath("/VNDegen/Test Game/saves/1-1-LT1.save") == "1-1-LT1.save")
    #expect(here.relativePath("/VNDegen/Test Game/saves/sync/persistent") == "sync/persistent")
    // A backup made before VNDegen recorded the portable path names another install's data directory.
    #expect(here.relativePath("/Users/player/Library/Application Support/VNDegen/GameData/Test Game/saves/auto-1-LT1.save") == "auto-1-LT1.save")
    #expect(here.relativePath("/VNDegen/Test Game/1-1-LT1.save") == nil)
}

@Test func copiesGameFolderSavesOnlyIntoAnEmptySaveFolder() throws {
    let base = try folder(["game/saves/1-1-LT1.save": "pc save", "game/saves/persistent": "pc persistent"])
    let data = try folder([:])
    try RenPyPlugin.importGameSaves(base: base, dataDirectory: data)
    #expect(try String(contentsOf: data.appendingPathComponent("saves/1-1-LT1.save"), encoding: .utf8) == "pc save")

    try Data("vndegen".utf8).write(to: data.appendingPathComponent("saves/persistent"))
    try Data("newer pc save".utf8).write(to: base.appendingPathComponent("game/saves/1-1-LT1.save"))
    try RenPyPlugin.importGameSaves(base: base, dataDirectory: data)
    #expect(try String(contentsOf: data.appendingPathComponent("saves/persistent"), encoding: .utf8) == "vndegen")
    #expect(try String(contentsOf: data.appendingPathComponent("saves/1-1-LT1.save"), encoding: .utf8) == "pc save")
}

@Test func aFailedSaveImportLeavesTheSaveFolderEmptyForTheNextLaunch() throws {
    let base = try folder(["game/saves/1-1-LT1.save": "pc save", "game/saves/2-1-LT1.save": "unreadable"])
    let unreadable = base.appendingPathComponent("game/saves/2-1-LT1.save")
    let data = try folder([:])
    try FileManager.default.setAttributes([.posixPermissions: 0], ofItemAtPath: unreadable.path)
    #expect(throws: RenPyError.self) { try RenPyPlugin.importGameSaves(base: base, dataDirectory: data) }
    #expect((try? FileManager.default.contentsOfDirectory(atPath: data.appendingPathComponent("saves").path))?.isEmpty ?? true)
    #expect(try FileManager.default.contentsOfDirectory(atPath: data.path).isEmpty)

    try FileManager.default.setAttributes([.posixPermissions: 0o644], ofItemAtPath: unreadable.path)
    try RenPyPlugin.importGameSaves(base: base, dataDirectory: data)
    #expect(try FileManager.default.contentsOfDirectory(atPath: data.appendingPathComponent("saves").path).sorted() == ["1-1-LT1.save", "2-1-LT1.save"])
}

@Test func anUnreadableGameSaveFolderFailsTheLaunchUntilItCanBeRead() throws {
    let base = try folder(["game/saves/1-1-LT1.save": "pc save"])
    let gameSaves = base.appendingPathComponent("game/saves")
    let data = try folder([:])
    try FileManager.default.setAttributes([.posixPermissions: 0], ofItemAtPath: gameSaves.path)
    #expect(throws: RenPyError.self) { try RenPyPlugin.importGameSaves(base: base, dataDirectory: data) }

    try FileManager.default.setAttributes([.posixPermissions: 0o755], ofItemAtPath: gameSaves.path)
    try RenPyPlugin.importGameSaves(base: base, dataDirectory: data)
    #expect(try String(contentsOf: data.appendingPathComponent("saves/1-1-LT1.save"), encoding: .utf8) == "pc save")
}

#if os(macOS)
private final class TestBundle {}

/// Runs the RenPyCompat tool that `swift test` built beside the test bundle.
private func runCompat(_ arguments: [String]) throws -> Int32 {
    let products = Bundle(for: TestBundle.self).bundleURL.deletingLastPathComponent()
    let process = Process()
    process.executableURL = products.appendingPathComponent("RenPyCompat")
    process.arguments = arguments
    process.standardOutput = FileHandle.nullDevice
    process.standardError = FileHandle.nullDevice
    try process.run()
    process.waitUntilExit()
    return process.terminationStatus
}

@Test func compatToolRefusesAnOutputFolderThatOverlapsAGame() throws {
    let games = try folder(["Test Game/8/renpy/__init__.py": "", "Test Game/8/game/script.rpy": "label start:\n    return\n"])
    let script = games.appendingPathComponent("Test Game/8/game/script.rpy")
    let link = games.deletingLastPathComponent().appendingPathComponent("\(games.lastPathComponent)-link")
    try FileManager.default.createSymbolicLink(at: link, withDestinationURL: games)
    for out in [games, games.appendingPathComponent("Test Game"), games.appendingPathComponent("Test Game/8/game/out"), link] {
        #expect(try runCompat([games.path, "--runtimes", "/nonexistent", "--out", out.path]) == 2, "\(out.path)")
        #expect(FileManager.default.fileExists(atPath: script.path))
    }
}

@Test func compatToolRefusesARunFolderThatALinkPutsInsideAGame() throws {
    let games = try folder(["Test Game/renpy/__init__.py": "", "Test Game/game/script.rpy": "", "Test Game/8/keep.txt": ""])
    let reports = try folder([:])
    try FileManager.default.createDirectory(at: reports, withIntermediateDirectories: true)
    try FileManager.default.createSymbolicLink(at: reports.appendingPathComponent("Test Game"),
                                               withDestinationURL: games.appendingPathComponent("Test Game"))
    #expect(try runCompat([games.path, "--runtimes", "/nonexistent", "--runtime", "8", "--out", reports.path]) == 2)
    #expect(FileManager.default.fileExists(atPath: games.appendingPathComponent("Test Game/8/keep.txt").path))
}
#endif

@Test func storesTheRuntimeOverridePerGame() throws {
    let data = try folder([:])
    #expect(RenPyPlugin.runtimeOverride(dataDirectory: data) == nil)
    try RenPyPlugin.setRuntimeOverride(.renPy7, dataDirectory: data)
    #expect(RenPyPlugin.runtimeOverride(dataDirectory: data) == .renPy7)
    try RenPyPlugin.setRuntimeOverride(nil, dataDirectory: data)
    #expect(RenPyPlugin.runtimeOverride(dataDirectory: data) == nil)
}

@MainActor @Test func commandRunsTheLauncherOnTheChosenRuntime() throws {
    let support = try #require(RenPyCommand.support)
    #expect(FileManager.default.fileExists(atPath: support.appendingPathComponent("launcher.py").path))
    #expect(FileManager.default.fileExists(atPath: support.appendingPathComponent("scripts/zz_vndegen.rpy").path))

    let command = RenPyCommand(base: URL(fileURLWithPath: "/games/Test Game"), runtime: .renPy7, runtimes: URL(fileURLWithPath: "/app/RenPy"),
                               support: support, dataDirectory: URL(fileURLWithPath: "/data/Test Game"),
                               placement: .frame(CGRect(x: 10, y: 38.4, width: 1280, height: 772)))
    #expect(command.executable.path == "/app/RenPy/7/lib/py2-mac-universal/python")
    #expect(command.arguments == [support.appendingPathComponent("launcher.py").path, "/games/Test Game"])
    #expect(command.environment["VNDEGEN_RENPY_RUNTIME"] == "/app/RenPy/7")
    #expect(command.environment["VNDEGEN_DATA"] == "/data/Test Game")
    #expect(command.environment["VNDEGEN_WINDOW"] == "10,38,1280,772")
    #expect(command.environment["VNDEGEN_FULLSCREEN"] == nil)
}

@Test func passesRemappedButtonsToTheHook() throws {
    let z = try #require(KeyboardKey.choices.first { $0.code == "KeyZ" })
    let up = try #require(KeyboardKey.choices.first { $0.code == "ArrowUp" })
    let buttons: [ControlInput: ControlAction] = [.y: .nothing, .a: .key(z), .l: .key(up), .b: .click(.secondary), .x: .button(.confirm)]
    #expect(RenPyCommand.buttonsVariable(buttons) == "a=key:K_z,b=click:3,x=button:confirm,y=nothing,l=key:K_UP")
    #expect(RenPyCommand.buttonsVariable([:]) == nil)
    #expect(KeyboardKey.choices.allSatisfy { RenPyCommand.pygameKey($0) != nil })
    #expect(KeyboardKey.choices.compactMap(RenPyCommand.pygameKey).filter { $0.hasPrefix("K_F") }.count == 12)

    let command = RenPyCommand(base: URL(fileURLWithPath: "/games/Test Game"), runtime: .renPy8, runtimes: URL(fileURLWithPath: "/app/RenPy"),
                               support: URL(fileURLWithPath: "/support"), dataDirectory: URL(fileURLWithPath: "/data/Test Game"),
                               placement: nil, buttons: [.r2: .click(.primary)])
    #expect(command.environment["VNDEGEN_BUTTONS"] == "r2=click:1")
}
