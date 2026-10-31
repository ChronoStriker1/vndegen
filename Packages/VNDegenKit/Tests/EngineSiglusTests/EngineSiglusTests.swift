import Foundation
import Testing
import VNCore
@testable import EngineSiglus

private func folder(_ files: [String: Data]) throws -> URL {
    let root = FileManager.default.temporaryDirectory.appending(path: "siglus-\(UUID().uuidString)")
    for (path, data) in files {
        let url = root.appending(path: path)
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try data.write(to: url)
    }
    return root
}

/// The first bytes of a RealLive `SEEN.TXT`, a table of scenario offsets and sizes.
private let seen = Data([0x00, 0x00, 0x00, 0x00, 0x40, 0x9c, 0x00, 0x00])

@Suite struct SiglusLayoutTests {
    @Test func findsSiglusEngineGamesAtTheTopOrAFolderDown() throws {
        let top = try folder(["Scene.pck": Data(), "Gameexe.dat": Data(), "SiglusEngine.exe": Data()])
        defer { try? FileManager.default.removeItem(at: top) }
        #expect(SiglusLayout(scanning: top) == SiglusLayout(engine: .siglus, root: "", evidence: "Gameexe.dat"))
        let down = try folder(["Summer Pockets/SCENE.PCK": Data(), "Summer Pockets/._Scene.pck": Data()])
        defer { try? FileManager.default.removeItem(at: down) }
        #expect(SiglusLayout(scanning: down) == SiglusLayout(engine: .siglus, root: "Summer Pockets", evidence: "Summer Pockets/SCENE.PCK"))
        #expect(SiglusPlugin().detect(FolderProbe(scanning: down))?.confidence == .certain)
        #expect(RealLivePlugin().detect(FolderProbe(scanning: down)) == nil)
    }

    @Test func siglusEngineWinsOverItsOwnGameexeIni() throws {
        let root = try folder(["Gameexe.ini": Data(), "Seen.txt": seen, "Scene.pck": Data()])
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(SiglusLayout(scanning: root)?.engine == .siglus)
    }

    @Test func findsRealLiveGamesBesideOrAboveTheirScenarios() throws {
        let beside = try folder(["CLANNAD/Gameexe.ini": Data(), "CLANNAD/Seen.txt": seen, "CLANNAD/g00/BG001.g00": Data()])
        defer { try? FileManager.default.removeItem(at: beside) }
        #expect(SiglusLayout(scanning: beside) == SiglusLayout(engine: .realLive, root: "CLANNAD", evidence: "CLANNAD/Seen.txt"))
        #expect(RealLivePlugin().detect(FolderProbe(scanning: beside))?.evidence == "CLANNAD/Seen.txt")
        #expect(SiglusPlugin().detect(FolderProbe(scanning: beside)) == nil)
        let below = try folder(["GAMEEXE.INI": Data(), "DAT/SEEN.TXT": seen])
        defer { try? FileManager.default.removeItem(at: below) }
        #expect(SiglusLayout(scanning: below) == SiglusLayout(engine: .realLive, root: "", evidence: "DAT/SEEN.TXT"))
    }

    @Test func leavesOutAVG32AndFoldersWithoutScenarios() throws {
        let avg32 = try folder(["Gameexe.ini": Data(), "SEEN.TXT": Data("PACL....".utf8)])
        defer { try? FileManager.default.removeItem(at: avg32) }
        #expect(SiglusLayout(scanning: avg32) == nil)
        let bare = try folder(["Gameexe.ini": Data(), "readme.txt": Data()])
        defer { try? FileManager.default.removeItem(at: bare) }
        #expect(SiglusLayout(scanning: bare) == nil)
        let deep = try folder(["a/b/Scene.pck": Data()])
        defer { try? FileManager.default.removeItem(at: deep) }
        #expect(SiglusLayout(scanning: deep) == nil)
    }
}

@Suite struct SiglusSaveTests {
    @Test func mapsSavesInTheSaveFolderWhateverItsCase() {
        #expect(SiglusPlugin.saveRelativePath("C:/Games/Rewrite/savedata/0001.sav", folder: "savedata") == "0001.sav")
        #expect(SiglusPlugin.saveRelativePath("C:\\Games\\Rewrite\\SaveData\\global.sav", folder: "savedata") == "global.sav")
        #expect(SiglusPlugin.saveRelativePath("/x/savedata/sub/read.sav", folder: "savedata") == "sub/read.sav")
        #expect(SiglusPlugin.saveRelativePath("C:/Games/Rewrite/Scene.pck", folder: "savedata") == nil)
        #expect(SiglusPlugin.saveRelativePath("C:/Games/Rewrite/savedata", folder: "savedata") == nil)
        #expect(SiglusPlugin.saveRelativePath("/data/game/savedata_rs/save001.sav", folder: "savedata_rs") == "save001.sav")
    }

    @Test func siglusEngineSavesBesideTheGameAndRealLiveInTheDataFolder() throws {
        let root = try folder(["Game/Scene.pck": Data(), "Game/SaveData/0001.sav": Data()])
        defer { try? FileManager.default.removeItem(at: root) }
        let data = root.appending(path: "data")
        let siglus = try #require(SiglusPlugin().saveLocations(folder: root, dataDirectory: data).first)
        #expect(siglus.directory.lastPathComponent == "SaveData")
        let realLive = try #require(RealLivePlugin().saveLocations(folder: root, dataDirectory: data).first)
        #expect(realLive.directory == data.appending(path: "savedata_rs", directoryHint: .isDirectory))
        #expect(RealLivePlugin().backupWarning(for: Game(id: GameID("g")), dataDirectory: data) != nil)
    }
}

@Suite struct SiglusInputTests {
    @Test func buttonsPressTheGamesKeys() {
        var router = SiglusInputRouter()
        #expect(router.translate(.button(.confirm, pressed: true)) == [.key(VirtualKey.return, down: true)])
        #expect(router.translate(.button(.confirm, pressed: false)) == [.key(VirtualKey.return, down: false)])
        #expect(router.translate(.button(.menu, pressed: true)) == [.button(1, down: true)])
        #expect(router.translate(.button(.pageUp, pressed: true)) == [.wheel(1)])
        #expect(router.translate(.button(.pageUp, pressed: false)) == [])
        #expect(router.translate(.button(.dash, pressed: true)) == [.key(VirtualKey.control, down: true)])
        #expect(router.releaseAll() == [.button(1, down: false), .key(VirtualKey.control, down: false)])
    }

    @Test func aKeyHeldTwiceGoesUpWithTheLastRelease() {
        var router = SiglusInputRouter()
        #expect(router.translate(.button(.confirm, pressed: true)) == [.key(13, down: true)])
        #expect(router.hostKey(13, pressed: true, isRepeat: false) == [])
        #expect(router.hostKey(13, pressed: true, isRepeat: true) == [.key(13, down: true, repeat: true)])
        #expect(router.translate(.button(.confirm, pressed: false)) == [])
        #expect(router.hostKey(13, pressed: false, isRepeat: false) == [.key(13, down: false)])
    }

    @Test func scrollingTurnsTheWheel() {
        var router = SiglusInputRouter()
        #expect(router.scroll(30) == [])
        #expect(router.scroll(20) == [.wheel(-1)])
        #expect(router.scroll(-85) == [.wheel(2)])
    }

    @Test func pointsLandOnTheGamesScreen() {
        let screen = CGSize(width: 1280, height: 720)
        // A 4:3 view has bars above and below a 16:9 game.
        let view = CGSize(width: 1024, height: 768)
        #expect(SiglusSession.screenPoint(CGPoint(x: 0.5, y: 0.5), view: view, screen: screen) == CGPoint(x: 640, y: 360))
        #expect(SiglusSession.screenPoint(CGPoint(x: 0, y: 0.125), view: view, screen: screen) == CGPoint(x: 0, y: 0))
    }

    @Test func keysMapToVirtualKeys() {
        #expect(VirtualKey.forUsage(40) == VirtualKey.return)
        #expect(VirtualKey.forUsage(4) == 65)
        #expect(VirtualKey.forUsage(58) == 112)
        #expect(KeyboardKey(key: "a", code: "KeyA", keyCode: 65).virtualKey == 65)
        #expect(KeyboardKey(key: "ArrowUp", code: "ArrowUp", keyCode: 38).virtualKey == VirtualKey.up)
    }

    @Test func messageBoxesListTheirButtonsInTheEnginesOrder() {
        #expect(SiglusPrompt.buttons(0) == ["OK"])
        #expect(SiglusPrompt.buttons(1) == ["OK", "Cancel"])
        #expect(SiglusPrompt.buttons(3) == ["Yes", "No", "Cancel"])
    }
}
