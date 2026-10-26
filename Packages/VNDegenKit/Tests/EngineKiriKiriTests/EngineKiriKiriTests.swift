import Foundation
import Testing
import GameInput
import VNCore
@testable import EngineKiriKiri

/// An in-memory listing of `files` plus the folders above them, as `FolderProbe(scanning:)` would list them.
private func probe(_ files: [String]) -> FolderProbe {
    var entries = Set<String>()
    for file in files {
        var parts = file.split(separator: "/").map(String.init)
        while !parts.isEmpty {
            entries.insert(parts.joined(separator: "/"))
            parts.removeLast()
        }
    }
    return FolderProbe(root: URL(fileURLWithPath: "/nonexistent"), entries: entries)
}

/// Writes `files` (relative path to contents) into a fresh folder.
private func folder(_ files: [String: Data]) throws -> URL {
    let root = FileManager.default.temporaryDirectory.appendingPathComponent("vndegen-kirikiri-\(UUID().uuidString)", isDirectory: true)
    for (path, contents) in files {
        let url = root.appendingPathComponent(path)
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try contents.write(to: url)
    }
    return root
}

private let xp3 = Data([0x58, 0x50, 0x33, 0x0D, 0x0A, 0x20, 0x0A, 0x1A, 0x8B, 0x67, 0x01, 0x17, 0, 0, 0])

@Test func detectsADataArchiveInTheRoot() {
    let listing = probe(["Test Game 08.exe", "data.xp3", "patch.xp3", "plugin/menu.dll"])
    #expect(KiriKiriLayout(listing) == KiriKiriLayout(root: "", data: .archive("data.xp3")))
    #expect(KiriKiriPlugin().detect(listing) == EngineMatch(engine: .kiriKiri, confidence: .certain, evidence: "data.xp3"))
}

@Test func detectsADataArchiveOneAndTwoFoldersDown() {
    #expect(KiriKiriLayout(probe(["Test Game 08/Game.eXe", "Test Game 08/Data.XP3"]))
        == KiriKiriLayout(root: "Test Game 08", data: .archive("Test Game 08/Data.XP3")))
    #expect(KiriKiriLayout(probe(["Test Game 08/Test Game 08 v1.1/game.exe", "Test Game 08/Test Game 08 v1.1/data.xp3",
                                  "Test Game 08/readme.txt"]))
        == KiriKiriLayout(root: "Test Game 08/Test Game 08 v1.1", data: .archive("Test Game 08/Test Game 08 v1.1/data.xp3")))
}

@Test func prefersTheShallowestRoot() {
    let listing = probe(["Test Game 08/game.exe", "Test Game 08/data.xp3", "Test Game 08/old/game.exe", "Test Game 08/old/data.xp3"])
    #expect(KiriKiriLayout(listing)?.root == "Test Game 08")
}

@Test func detectsAnUnpackedDataFolder() {
    let listing = probe(["Test Game 01/game.exe", "Test Game 01/data/startup.tjs", "Test Game 01/data/system/startup.tjs",
                         "Test Game 01/bgm.xp3"])
    #expect(KiriKiriLayout(listing) == KiriKiriLayout(root: "Test Game 01", data: .folder("Test Game 01/data")))
    #expect(KiriKiriPlugin().detect(listing)?.evidence == "Test Game 01/data/startup.tjs")
}

@Test func detectsAnUnpackedDataFolderTwoFoldersDown() throws {
    let copied = try folder(["outer/inner/game.exe": Data(), "outer/inner/data/startup.tjs": Data(),
                             "outer/inner/data/system/Config.tjs": Data()])
    // The library lists three levels, which ends at `outer/inner/data`.
    #expect(KiriKiriPlugin().detect(FolderProbe(scanning: copied))?.evidence == "outer/inner/data/startup.tjs")
    #expect(KiriKiriLayout(scanning: copied) == KiriKiriLayout(root: "outer/inner", data: .folder("outer/inner/data")))
    let saves = KiriKiriPlugin().saveLocations(folder: copied, dataDirectory: URL(fileURLWithPath: "/nonexistent-data"))
    #expect(saves.first?.directory.standardizedFileURL.path == copied.appendingPathComponent("outer/inner/savedata").standardizedFileURL.path)
}

@Test func prefersTheDataArchiveOverTheDataFolder() {
    let listing = probe(["game.exe", "data.xp3", "data/startup.tjs"])
    #expect(KiriKiriLayout(listing)?.data == .archive("data.xp3"))
}

@Test func ignoresAppleDoubleFiles() {
    #expect(KiriKiriLayout(probe(["game.exe", "._data.xp3", "._game.exe"])) == nil)
    let listing = probe(["._data.xp3", "Test Game 08/._data.xp3", "Test Game 08/game.exe", "Test Game 08/data.xp3"])
    #expect(KiriKiriLayout(listing) == KiriKiriLayout(root: "Test Game 08", data: .archive("Test Game 08/data.xp3")))
}

@Test func doesNotMatchOtherEngines() {
    #expect(KiriKiriLayout(probe(["Test Game.exe", "readme.txt"])) == nil)
    #expect(KiriKiriLayout(probe(["Test Game.exe", "renpy/__init__.py", "game/script.rpyc", "lib/py3-windows-x86_64/python.exe"])) == nil)
    #expect(KiriKiriLayout(probe(["Game.exe", "www/js/rpg_core.js", "www/index.html", "package.json"])) == nil)
}

@Test func detectsAnArchiveEmbeddedInTheExecutable() throws {
    let root = try folder([
        "Test Game 73/Test Game 73.exe": Data(count: 4096),
        "Test Game 73/config.exe": Data(count: 512),
        "Test Game 73/scenario.xp3": xp3,
        "Test Game 73/._scenario.xp3": Data(count: 64),
    ])
    let listing = FolderProbe(scanning: root)
    #expect(KiriKiriLayout(listing) == KiriKiriLayout(root: "Test Game 73", data: .embedded("Test Game 73/Test Game 73.exe")))
    #expect(KiriKiriPlugin().detect(listing)?.evidence == "Test Game 73/Test Game 73.exe")
}

@Test func doesNotTakeAnyXP3NamedFileForAnEmbeddedArchive() throws {
    let root = try folder(["game.exe": Data(count: 64), "bgm.xp3": Data("not an archive".utf8)])
    #expect(KiriKiriLayout(FolderProbe(scanning: root)) == nil)
    #expect(KiriKiriLayout(probe(["game.exe", "bgm.xp3"])) == nil)
}

@Test func mapsSavesFromOtherMachines() {
    let rule = { KiriKiriPlugin.saveRelativePath($0, folder: ["savedata"]) }
    #expect(rule("/home/deck/Games/vn/Test Game 08/savedata/datasc.ksd") == "datasc.ksd")
    #expect(rule("C:/Games/Test Game 08/savedata/data0.kdt") == "data0.kdt")
    #expect(rule("C:/Games/Test Game 08/SaveData/data.cfu") == "data.cfu")
    #expect(rule("/home/deck/Games/vn/Test Game 08/savedata/slot/data0.bmp") == "slot/data0.bmp")
}

@Test func rejectsPathsThatNameNoSaveOrLeaveTheSaveFolder() {
    let rule = { KiriKiriPlugin.saveRelativePath($0, folder: ["savedata"]) }
    #expect(rule("C:/Games/Test Game 08/data.xp3") == nil)
    #expect(rule("C:/Games/Test Game 08/savedata") == nil)
    #expect(rule("C:/Games/Test Game 08/savedata/") == nil)
    #expect(rule("C:/Games/Test Game 08/savedata//datasc.ksd") == nil)
    #expect(rule("C:/Games/Test Game 08/savedata/./datasc.ksd") == nil)
    #expect(rule("C:/Games/Test Game 08/savedata/../data.xp3") == nil)
    #expect(rule("C:/Games/Test Game 08/savedata/slot/../../library.json") == nil)
}

@Test func keepsSavesInTheRootOfANestedGame() throws {
    let copied = try folder([
        "Test Game 08/game.exe": Data(),
        "Test Game 08/data.xp3": xp3,
        "Test Game 08/savedata/datasc.ksd": Data(),
    ])
    let data = URL(fileURLWithPath: "/nonexistent-data")
    let location = try #require(KiriKiriPlugin().saveLocations(folder: copied, dataDirectory: data).first)
    #expect(location.directory.standardizedFileURL == copied.appendingPathComponent("Test Game 08/savedata").standardizedFileURL)
    let local = location.directory.standardizedFileURL.path
    #expect(location.relativePath("\(local)/datasc.ksd") == "datasc.ksd")
    #expect(location.relativePath("\(local)/slot/data0.kdt") == "slot/data0.kdt")
    #expect(KiriKiriPlugin().saveLocations(folder: try folder(["readme.txt": Data()]), dataDirectory: data).isEmpty)
}

@MainActor @Test func makingASessionReportsTheMissingEngine() {
    let launch = GameLaunch(game: Game(id: GameID("Test Game 08")), folder: URL(fileURLWithPath: "/nonexistent"),
                            dataDirectory: URL(fileURLWithPath: "/nonexistent-data"), onExit: {})
    #expect(throws: KiriKiriError.self) { try KiriKiriPlugin().makeSession(launch) }
}

@Test func mapsAppDataSavesFromWindowsAndProton() {
    let rule = KiriKiriPlugin.appDataRelativePath
    #expect(rule("C:/Users/player/AppData/Roaming/Studio/Test Game 08/datasc.ksd") == "Studio/Test Game 08/datasc.ksd")
    #expect(rule("/home/deck/.local/share/Steam/steamapps/compatdata/4012345/pfx/drive_c/users/steamuser/AppData/Roaming/Studio/data0.kdt")
        == "Studio/data0.kdt")
    #expect(rule("C:/users/player/appdata/roaming/Studio/datasu.ksd") == "Studio/datasu.ksd")
    #expect(rule("C:/Users/player/AppData/Local/Studio/datasc.ksd") == nil)
    #expect(rule("C:/Users/player/AppData/Roaming/") == nil)
    #expect(rule("C:/Users/player/AppData/Roaming/Studio/../../x.ksd") == nil)
    #expect(rule("C:/Games/Test Game 08/savedata/datasc.ksd") == nil)
}

@Test func mapsDocumentsSavesFromWindowsAndProton() {
    let rule = KiriKiriPlugin.documentsRelativePath
    #expect(rule("C:/Users/player/Documents/Studio/Test Game 08/datasc.ksd") == "Studio/Test Game 08/datasc.ksd")
    #expect(rule("/home/deck/.local/share/Steam/steamapps/compatdata/4012345/pfx/drive_c/users/steamuser/My Documents/Studio/data0.kdt")
        == "Studio/data0.kdt")
    #expect(rule("/home/deck/.local/share/Steam/steamapps/compatdata/4012345/pfx/drive_c/users/steamuser/Documents/Studio/data0.kdt")
        == "Studio/data0.kdt")
    #expect(rule("C:/Users/player/Desktop/Studio/datasc.ksd") == nil)
    #expect(rule("C:/Users/player/Documents/./datasc.ksd") == nil)
    #expect(rule("C:/Users/player/Saved Games/Studio/datasc.ksd") == "Studio/datasc.ksd")
    #expect(rule("/home/deck/.local/share/Steam/steamapps/compatdata/4012345/pfx/drive_c/users/steamuser/Saved Games/Studio/data0.kdt")
        == "Studio/data0.kdt")
}

/// A `.cf` as a Kirikiri Z game ships it: a comment header, CRLF line endings, and each value spelled out in `\x`
/// escapes. `datapath` here is `$(exepath)/erogeVN`.
private let movedSavesConfig = Data("""
    ; ============================================================================
    ; *DO NOT EDIT* this file unless you are understanding what you are doing.
    ;  Each line consists of NAME="VALUE" pair, VALUE is a series of
    ;  \\xNN, where NN is hexadecimal representation of UNICODE codepoint.
    ; ============================================================================
    datapath="\\x24\\x28\\x65\\x78\\x65\\x70\\x61\\x74\\x68\\x29\\x2f\\x65\\x72\\x6f\\x67\\x65\\x56\\x4e"
    usedb="\\x79\\x65\\x73"
    hkupdaterect=""

    """.replacingOccurrences(of: "\n", with: "\r\n").utf8)

@Test func mapsTheSaveFolderAGamesConfigNames() throws {
    let copied = try folder([
        "Test Game 71/inner/game.exe": Data(),
        "Test Game 71/inner/game.cf": movedSavesConfig,
        "Test Game 71/inner/config.exe": Data(),
        "Test Game 71/inner/data/startup.tjs": Data(),
    ])
    let data = URL(fileURLWithPath: "/tmp/vndegen-kirikiri-data/GameData/Test Game 71")
    let locations = KiriKiriPlugin().saveLocations(folder: copied, dataDirectory: data)
    #expect(locations.map { $0.directory.lastPathComponent } == ["erogeVN", "AppData", "Documents"])
    let saves = locations[0]
    #expect(saves.directory.standardizedFileURL.path == copied.appendingPathComponent("Test Game 71/inner/erogeVN").standardizedFileURL.path)
    #expect(saves.relativePath("\(saves.directory.standardizedFileURL.path)/datasc.ksd") == "datasc.ksd")
    #expect(saves.relativePath("C:/Games/Test Game 71/erogeVN/data0.kdt") == "data0.kdt")
    #expect(saves.relativePath("/home/deck/Games/vn/Test Game 71/EROGEVN/slot/data0.bmp") == "slot/data0.bmp")
    #expect(saves.relativePath("/VNDegen/Test Game 71/erogeVN/datasu.ksd") == "datasu.ksd")
    // The game no longer writes savedata/, and an erogeVN folder under AppData belongs to that location.
    #expect(saves.relativePath("C:/Games/Test Game 71/savedata/datasc.ksd") == nil)
    #expect(saves.relativePath("C:/Users/player/AppData/Roaming/erogeVN/datasc.ksd") == nil)
    #expect(saves.relativePath("C:/Games/Test Game 71/erogeVN/../data.xp3") == nil)
}

@Test func resolvesAConfigsSaveFolderAsTheEngineDoes() throws {
    let cases: [(String, [String]?)] = [
        (#"datapath="\x24\x28\x65\x78\x65\x70\x61\x74\x68\x29\x5c\x73\x61\x76\x65\x5c""#, ["save"]),
        (#"datapath="save/" "slot""#, ["save", "slot"]),
        ("datapath=./Save", ["Save"]),
        ("datapath='$(appdatapath)/Studio/Test Game'", nil),
        ("datapath=$(savedgamespath)/Studio", nil),
        // The engine keeps savedata/ for a path that leaves the game's folders, and for the root itself.
        ("datapath=$(exepath)/../saves", ["savedata"]),
        // KiriKiri reads a folder named only with dots as climbing one folder fewer than its dots.
        ("datapath=$(exepath)/.../victim/savedata", ["savedata"]),
        ("datapath=$(exepath)/a/b/..../savedata", ["savedata"]),
        ("datapath=$(exepath)/a/b/.../save", ["save"]),
        ("datapath=$(exepath)/a/./save", ["a", "save"]),
        ("datapath=$(appdatapath)/.../.../victim", ["savedata"]),
        (#"datapath="C:\\saves""#, ["savedata"]),
        ("datapath=/Users/someone/saves", ["savedata"]),
        ("datapath=$(exepath)", nil),
        ("datapath=", ["savedata"]),
        // Only the first datapath counts, and a comment or another option is no datapath.
        ("; datapath=commented\nDataPath=wrongcase\ndatapath=first\ndatapath=second", ["first"]),
    ]
    for (config, expected) in cases {
        let copied = try folder(["game.exe": Data(), "game.cf": Data(config.utf8), "data.xp3": xp3])
        let layout = try #require(KiriKiriLayout(scanning: copied))
        let paths = KiriKiriPaths(folder: copied, dataDirectory: URL(fileURLWithPath: "/tmp/vndegen-kirikiri-data/GameData/Test Game"),
                                  layout: layout)
        #expect(KiriKiriPlugin.saveFolder(paths) == expected, "\(config)")
    }
}

/// `path` with its symbolic links resolved, as the engine sees it.
private func realPath(_ path: String) throws -> String {
    let real = try #require(realpath(path, nil))
    defer { free(real) }
    return String(cString: real)
}

@Test func mapsTheSaveFolderOfEachDataPathCaseTheEngineSavesIn() throws {
    let table = URL(fileURLWithPath: #filePath).deletingLastPathComponent().appendingPathComponent("Fixtures/datapath-cases.tsv")
    let rows = try String(contentsOf: table, encoding: .utf8).split(separator: "\n").filter { !$0.hasPrefix("#") }
    #expect(rows.count > 20)
    for row in rows {
        let columns = row.split(separator: "\t", omittingEmptySubsequences: false).map(String.init)
        try #require(columns.count == 4, "\(row)")
        let (name, datapath, setup, saves) = (columns[0], columns[1], columns[2], columns[3])
        let caseFolder = try folder(["a/game/game.exe": Data(), "a/game/game.cf": Data("datapath=\(datapath)\n".utf8), "a/game/data.xp3": xp3])
        let game = caseFolder.appendingPathComponent("a/game", isDirectory: true)
        for made in ["data/AppData", "data/Documents", "victim"] {
            try FileManager.default.createDirectory(at: caseFolder.appendingPathComponent(made), withIntermediateDirectories: true)
        }
        for item in setup.split(separator: " ") where item != "-" {
            if let arrow = item.range(of: "->") {
                try FileManager.default.createSymbolicLink(atPath: game.appendingPathComponent(String(item[..<arrow.lowerBound])).path,
                                                           withDestinationPath: String(item[arrow.upperBound...]))
            } else {
                try FileManager.default.createDirectory(at: game.appendingPathComponent(String(item)), withIntermediateDirectories: true)
            }
        }
        let locations = KiriKiriPlugin().saveLocations(folder: game, dataDirectory: caseFolder.appendingPathComponent("data"))
        let expected = try realPath(caseFolder.path) + "/" + saves, realGame = try realPath(game.path)
        if expected == realGame || saves.hasPrefix("data/") {
            // The root holds the game's files too, and the AppData and Documents locations cover their own folders.
            #expect(locations.map { $0.directory.lastPathComponent } == ["AppData", "Documents"], "\(name): \(datapath)")
        } else {
            let directory = locations[0].directory.standardizedFileURL.path
            #expect(locations.count == 3 && realGame + directory.dropFirst(game.standardizedFileURL.path.count) == expected,
                    "\(name): \(datapath) maps \(directory)")
        }
        try FileManager.default.removeItem(at: caseFolder)
    }
}

@Test func readsTheConfigOfTheExecutableTheEngineRuns() throws {
    // With several executables and no .cf of their own, the engine reads no .cf.
    let ambiguous = try folder(["game.exe": Data(), "config.exe": Data(), "other.cf": Data("datapath=save".utf8)])
    #expect(KiriKiriPlugin.configuredDataPath(gameRoot: ambiguous) == nil)
    let lone = try folder(["Game.EXE": Data(), "Game.cf": Data("datapath=save".utf8)])
    #expect(KiriKiriPlugin.configuredDataPath(gameRoot: lone) == "save")
}

@Test func readsTJSStringLiterals() {
    #expect(KiriKiriPlugin.tjsString(#""\x61\x3042b""#) == "a\u{3042}b")
    #expect(KiriKiriPlugin.tjsString(#""\x304260""#) == "\u{3042}60")
    #expect(KiriKiriPlugin.tjsString(#"'a\'b\\c\n'"#) == "a'b\\c\n")
    #expect(KiriKiriPlugin.tjsString(#""\0101""#) == "A")
    #expect(KiriKiriPlugin.tjsString(#""ab"  "cd" ignored"#) == "abcd")
    #expect(KiriKiriPlugin.tjsString(#""unclosed"#) == nil)
    #expect(KiriKiriPlugin.tjsString(#""\x61"#) == nil)
}

@Test func keepsAppDataAndDocumentsSavesInTheDataDirectory() throws {
    let copied = try folder(["Test Game 08/game.exe": Data(), "Test Game 08/data.xp3": xp3])
    let data = URL(fileURLWithPath: "/tmp/vndegen-kirikiri-data/GameData/Test Game 08")
    let locations = KiriKiriPlugin().saveLocations(folder: copied, dataDirectory: data)
    #expect(locations.map { $0.directory.lastPathComponent } == ["savedata", "AppData", "Documents"])
    let appData = locations[1], documents = locations[2]
    #expect(appData.directory.standardizedFileURL.path == data.appendingPathComponent("AppData").path)
    #expect(appData.relativePath("\(data.path)/AppData/Studio/datasc.ksd") == "Studio/datasc.ksd")
    #expect(appData.relativePath("/VNDegen/Test Game 08/AppData/Studio/datasc.ksd") == "Studio/datasc.ksd")
    #expect(appData.relativePath("/Users/someone/Library/Application Support/VNDegen/GameData/Test Game 08/AppData/Studio/datasc.ksd")
        == "Studio/datasc.ksd")
    #expect(appData.relativePath("C:/Users/player/AppData/Roaming/Studio/datasc.ksd") == "Studio/datasc.ksd")
    #expect(appData.relativePath("/VNDegen/Test Game 08/Documents/Studio/datasc.ksd") == nil)
    #expect(documents.relativePath("/VNDegen/Test Game 08/Documents/Studio/datasc.ksd") == "Studio/datasc.ksd")
    #expect(documents.relativePath("C:/Users/player/My Documents/Studio/datasc.ksd") == "Studio/datasc.ksd")
}

@Test func leavesAppDataSaveFoldersToTheAppDataLocation() throws {
    let copied = try folder(["game.exe": Data(), "data.xp3": xp3])
    let locations = KiriKiriPlugin().saveLocations(folder: copied, dataDirectory: URL(fileURLWithPath: "/nonexistent-data"))
    let path = "C:/Users/player/AppData/Roaming/Studio/savedata/datasc.ksd"
    #expect(locations[0].relativePath(path) == nil)
    #expect(locations[1].relativePath(path) == "Studio/savedata/datasc.ksd")
    // A game installed in Documents still saves in its own savedata folder, and that location comes first.
    #expect(locations[0].relativePath("C:/Users/player/Documents/Games/Test Game 08/savedata/datasc.ksd") == "datasc.ksd")
}

@Test func leavesSaveFoldersInVNDegensOwnUserFoldersToThoseLocations() throws {
    let copied = try folder(["game.exe": Data(), "data.xp3": xp3])
    let data = URL(fileURLWithPath: "/tmp/vndegen-kirikiri-data/GameData/Test Game 08")
    let locations = KiriKiriPlugin().saveLocations(folder: copied, dataDirectory: data)
    // Save sync restores a file to the first location that recognizes it.
    let claim = { (path: String) in locations.firstIndex { $0.relativePath(path) != nil } }
    for (path, location) in [
        ("/VNDegen/Test Game 08/AppData/Studio/savedata/datasc.ksd", 1),
        ("/Users/someone/Library/Application Support/VNDegen/GameData/Test Game 08/AppData/Studio/savedata/datasc.ksd", 1),
        ("\(data.path)/AppData/Studio/savedata/datasc.ksd", 1),
        ("/VNDegen/Test Game 08/Documents/Studio/savedata/datasc.ksd", 2),
    ] {
        #expect(claim(path) == location, "\(path)")
        #expect(locations[location].relativePath(path) == "Studio/savedata/datasc.ksd")
    }
    #expect(claim("C:/Users/player/Documents/Games/Test Game 08/savedata/datasc.ksd") == 0)
    #expect(claim("/VNDegen/Test Game 08/savedata/datasc.ksd") == 0)
}

@Test func mapsButtonsToKAGControls() {
    #expect(VirtualButton.confirm.kiriKiriControl == .hold(.key(SDLScancode.return)))
    #expect(VirtualButton.cancel.kiriKiriControl == .hold(.mouse(SDLMouseButton.right)))
    #expect(VirtualButton.menu.kiriKiriControl == .hold(.key(SDLScancode.escape)))
    #expect(VirtualButton.dash.kiriKiriControl == .hold(.key(SDLScancode.leftControl)))
    #expect(VirtualButton.up.kiriKiriControl == .hold(.key(SDLScancode.up)))
    #expect(VirtualButton.left.kiriKiriControl == .hold(.key(SDLScancode.left)))
    #expect(VirtualButton.pageUp.kiriKiriControl == .wheel(1))
    #expect(VirtualButton.pageDown.kiriKiriControl == .wheel(-1))
    #expect(VirtualButton.hub.kiriKiriControl == nil)
}

@Test func sendsButtonsAsKeysClicksAndWheelNotches() {
    var router = KiriKiriInputRouter()
    #expect(router.translate(.button(.confirm, pressed: true)) == [.key(scancode: 40, down: true)])
    #expect(router.translate(.button(.confirm, pressed: false)) == [.key(scancode: 40, down: false)])
    #expect(router.translate(.button(.cancel, pressed: true)) == [.click(3, down: true)])
    #expect(router.translate(.button(.cancel, pressed: false)) == [.click(3, down: false)])
    #expect(router.translate(.button(.pageUp, pressed: true)) == [.wheel(notches: 1)])
    #expect(router.translate(.button(.pageUp, pressed: false)) == [])
    #expect(router.translate(.button(.hub, pressed: true)) == [])
}

@Test func holdsAKeyUntilItsLastPressLetsGo() {
    var router = KiriKiriInputRouter()
    let enter = KeyboardKey(key: "Enter", code: "Enter", keyCode: 13)
    #expect(router.translate(.button(.confirm, pressed: true)) == [.key(scancode: 40, down: true)])
    #expect(router.translate(.key(enter, pressed: true)) == [])
    #expect(router.translate(.button(.confirm, pressed: false)) == [])
    #expect(router.translate(.key(enter, pressed: false)) == [.key(scancode: 40, down: false)])
    #expect(router.translate(.key(enter, pressed: false)) == [])
    #expect(router.translate(.click(.primary, pressed: true)) == [.click(1, down: true)])
    #expect(router.releaseAll() == [.click(1, down: false)])
    #expect(router.translate(.click(.primary, pressed: false)) == [])
}

@Test func sendsTouchMousePointerEvents() {
    var router = KiriKiriInputRouter()
    let point = CGPoint(x: 0.25, y: 0.75)
    #expect(router.translate(.pointer(PointerEvent(phase: .began, location: point)))
        == [.mouseMove(to: point), .mouseButton(1, down: true, at: point)])
    #expect(router.translate(.pointer(PointerEvent(phase: .moved, location: point))) == [.mouseMove(to: point)])
    #expect(router.translate(.pointer(PointerEvent(phase: .ended, location: point, button: .secondary)))
        == [.mouseButton(3, down: false, at: point)])
}

@Test func turnsScrollingIntoWheelNotches() {
    var router = KiriKiriInputRouter()
    #expect(KiriKiriInputRouter.pointsPerNotch == 40)
    #expect(router.translate(.scroll(dy: 24)) == [])
    // Content scrolling down is the wheel turned toward the player.
    #expect(router.translate(.scroll(dy: 24)) == [.wheel(notches: -1)])
    #expect(router.translate(.scroll(dy: -92)) == [.wheel(notches: 2)])
    #expect(router.translate(.scroll(dy: -38)) == [.wheel(notches: 1)])
}

@Test func mapsEveryBindableKeyToAScancode() {
    for key in KeyboardKey.choices {
        #expect(key.sdlScancode != nil, "\(key.code)")
    }
    #expect(KeyboardKey(key: "a", code: "KeyA", keyCode: 65).sdlScancode == 4)
    #expect(KeyboardKey(key: "z", code: "KeyZ", keyCode: 90).sdlScancode == 29)
    #expect(KeyboardKey(key: "1", code: "Digit1", keyCode: 49).sdlScancode == 30)
    #expect(KeyboardKey(key: "0", code: "Digit0", keyCode: 48).sdlScancode == 39)
    #expect(KeyboardKey(key: "F12", code: "F12", keyCode: 123).sdlScancode == 69)
    #expect(KeyboardKey(key: " ", code: "Space", keyCode: 32).sdlScancode == 44)
    #expect(KeyboardKey(key: "?", code: "Unknown", keyCode: 0).sdlScancode == nil)
}

@Test func passesTheKeyboardThroughExceptTheVNDegenKey() {
    var router = KiriKiriInputRouter()
    #expect(router.hostKey(SDLScancode.graveAccent, pressed: true, isRepeat: false) == [])
    #expect(router.hostKey(SDLScancode.return, pressed: true, isRepeat: false) == [.key(scancode: 40, down: true)])
    #expect(router.hostKey(SDLScancode.return, pressed: true, isRepeat: true) == [.key(scancode: 40, down: true)])
    #expect(router.hostKey(SDLScancode.return, pressed: false, isRepeat: false) == [.key(scancode: 40, down: false)])
}

#if os(macOS)
@MainActor @Test func mapsMacKeyCodesToScancodes() {
    let table = KiriKiriHostView.scancodes
    #expect(table[0x00] == 4) // A
    #expect(table[0x24] == SDLScancode.return)
    #expect(table[0x35] == SDLScancode.escape)
    #expect(table[0x3B] == SDLScancode.leftControl)
    #expect(table[0x7E] == SDLScancode.up)
    #expect(Set(table.values).count == table.count)
}
#endif

@Test func handsTheEngineADataFolderWithItsClosingSlash() {
    let folder = URL(fileURLWithPath: "/Games/Test Game 01")
    let data = URL(fileURLWithPath: "/Data/Test Game 01")
    let unpacked = KiriKiriPaths(folder: folder, dataDirectory: data,
                                 layout: KiriKiriLayout(root: "Test Game 01", data: .folder("Test Game 01/data")))
    #expect(unpacked.projectData == "/Games/Test Game 01/Test Game 01/data/")
    #expect(unpacked.gameRoot.path == "/Games/Test Game 01/Test Game 01")
    let archive = KiriKiriPaths(folder: folder, dataDirectory: data, layout: KiriKiriLayout(root: "", data: .archive("data.xp3")))
    #expect(archive.projectData == "/Games/Test Game 01/data.xp3")
    #expect(archive.appData.path == "/Data/Test Game 01/AppData")
    #expect(archive.documents.path == "/Data/Test Game 01/Documents")
    #expect(archive.log.path == "/Data/Test Game 01/logs/kirikiri.txt")
}

/// An engine that runs no game. It records the calls a session makes while one of its frames is still running.
private enum FakeEngine {
    nonisolated(unsafe) static var inFrame = false
    nonisolated(unsafe) static var callsInFrame: [String] = []
    /// Runs inside the next frame, as a native modal loop would.
    nonisolated(unsafe) static var duringFrame: (() -> Void)?

    static func record(_ call: String) {
        if inFrame { callsInFrame.append(call) }
    }

    @MainActor static var engine: KiriKiriEngine {
        KiriKiriEngine(
            start: { _ in 0 },
            iterate: {
                FakeEngine.record("iterate")
                guard !FakeEngine.inFrame else { return 0 }
                FakeEngine.inFrame = true
                defer { FakeEngine.inFrame = false }
                let work = FakeEngine.duringFrame
                FakeEngine.duringFrame = nil
                work?()
                return 0
            },
            key: { _, _ in }, mouseButton: { _, _, _, _ in }, mouseMove: { _, _ in }, click: { _, _ in }, wheel: { _ in },
            setPaused: { _ in }, requestQuit: {}, stop: { FakeEngine.record("stop") }, view: { nil })
    }
}

@MainActor @Test func runsNoEngineCallUnderAFrameThatIsStillRunning() throws {
    let data = try folder([:])
    let launch = GameLaunch(game: Game(id: GameID("Test Game 08")), folder: URL(fileURLWithPath: "/nonexistent"),
                            dataDirectory: data, onExit: {})
    let session = try KiriKiriSession(launch, layout: KiriKiriLayout(root: "", data: .archive("data.xp3")),
                                      engine: FakeEngine.engine)
    try #require(session.view as? KiriKiriHostView).onReady?()
    FakeEngine.callsInFrame = []
    FakeEngine.duringFrame = {
        MainActor.assumeIsolated {
            session.step()
            session.terminate()
            session.step()
        }
    }
    session.step()
    #expect(FakeEngine.callsInFrame.isEmpty)
}
