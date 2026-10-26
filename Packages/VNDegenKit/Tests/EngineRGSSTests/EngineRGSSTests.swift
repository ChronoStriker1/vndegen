import Foundation
import Testing
import VNCore
@testable import EngineRGSS

/// Builds a game folder from relative paths and file contents, and deletes it afterwards.
private final class GameFolder {
    let root: URL

    init(_ files: [String: Data]) throws {
        root = FileManager.default.temporaryDirectory.appendingPathComponent("rgss-\(UUID().uuidString)", isDirectory: true)
        for (path, data) in files {
            let file = root.appendingPathComponent(path)
            try FileManager.default.createDirectory(at: file.deletingLastPathComponent(), withIntermediateDirectories: true)
            try data.write(to: file)
        }
    }

    deinit { try? FileManager.default.removeItem(at: root) }

    var probe: FolderProbe { FolderProbe(scanning: root) }
}

private func ini(_ text: String, encoding: String.Encoding = .utf8) -> Data { text.data(using: encoding)! }

private let none: @Sendable (RuntimePackage) -> URL? = { _ in nil }

@Suite struct LayoutTests {
    @Test func packedVXAceAtTheTop() throws {
        let game = try GameFolder(["Game.ini": ini("[Game]\nRTP=RPGVXAce\nLibrary=System\\RGSS301.dll\n"), "Game.rgss3a": Data()])
        let layout = try #require(RGSSLayout(game.probe))
        #expect(layout.version == .vxAce)
        #expect(layout.root == "")
        #expect(layout.evidence == "Game.rgss3a")
        #expect(layout.rtps == ["RPGVXAce"])
    }

    @Test func packedXPOneFolderDownWithThreeRTPKeys() throws {
        let game = try GameFolder(["Game v1.4/Game.ini": ini("[Game]\nRTP1=Standard\nRTP2=\nRTP3=\n"), "Game v1.4/Game.rgssad": Data()])
        let layout = try #require(RGSSLayout(game.probe))
        #expect(layout.version == .xp)
        #expect(layout.gameIni == "Game v1.4/Game.ini")
        #expect(layout.rtps == ["Standard"])
    }

    /// The library reads three levels, which stops above Data/ in a game two folders down.
    @Test func unpackedVXTwoFoldersDown() throws {
        let game = try GameFolder(["A/B/Game.ini": ini("[Game]\nRTP=RPGVX\n"), "A/B/Data/Scripts.rvdata": Data()])
        let layout = try #require(RGSSLayout(game.probe))
        #expect(layout.version == .vx)
        #expect(layout.root == "A/B")
        #expect(layout.evidence == "A/B/Data/Scripts.rvdata")
    }

    @Test func shiftJISGameIniAndLibraryFallback() throws {
        // A Japanese title, and no archive under the usual name, so the version comes from Library=.
        let text = "[Game]\r\nRTP=RPGVXAce\r\nLibrary=System\\RGSS301.dll\r\nTitle=魔王城\r\n"
        let game = try GameFolder(["Game.ini": ini(text, encoding: .shiftJIS), "Other.rgss3a": Data()])
        let layout = try #require(RGSSLayout(game.probe))
        #expect(layout.version == .vxAce)
        #expect(layout.rtps == ["RPGVXAce"])
    }

    @Test func notRGSS() throws {
        let game = try GameFolder(["Game.ini": ini("[Game]\nTitle=Other\n"), "www/js/rpg_core.js": Data()])
        #expect(RGSSLayout(game.probe) == nil)
    }

    @Test func detectionPicksTheVersionsPlugin() throws {
        let game = try GameFolder(["Game.ini": ini("[Game]\nRTP=RPGVX\n"), "Game.rgss2a": Data()])
        #expect(RGSSPlugin(.vx, runtimeFolder: none).detect(game.probe)?.engine == .rpgMakerVX)
        #expect(RGSSPlugin(.vxAce, runtimeFolder: none).detect(game.probe) == nil)
        #expect(RGSSPlugin(.xp, runtimeFolder: none).detect(game.probe) == nil)
    }
}

@Suite struct RuntimeTests {
    @Test func gameIniNamesMapToTheOfficialRTPs() throws {
        #expect(RGSSRuntimes.package(named: "RPGVXAce") == RGSSRuntimes.vxAce)
        #expect(RGSSRuntimes.package(named: "rpgvx") == RGSSRuntimes.vx)
        #expect(RGSSRuntimes.package(named: "Standard") == RGSSRuntimes.xp)
        #expect(RGSSRuntimes.package(named: "MyCustomRTP") == nil)
        for package in [RGSSRuntimes.xp, RGSSRuntimes.vx, RGSSRuntimes.vxAce] {
            #expect(package.download.scheme == "https")
            #expect(package.sha256.count == 64)
        }
    }

    @Test func pluginListsTheRTPsTheGameNames() throws {
        let game = try GameFolder(["Game.ini": ini("[Game]\nRTP1=Standard\nRTP2=MyCustomRTP\n"), "Game.rgssad": Data()])
        let plugin = RGSSPlugin(.xp, runtimeFolder: none)
        let game0 = Game(id: GameID("x"), title: "x")
        #expect(plugin.runtimePackages(for: game0, folder: game.root) == [RGSSRuntimes.xp])
    }

    @Test func launchLinksInstalledRTPsAndThePreloadScript() throws {
        let data = FileManager.default.temporaryDirectory.appendingPathComponent("rgss-data-\(UUID().uuidString)")
        let installed = try GameFolder(["Graphics/System/Window.png": Data()])
        defer { try? FileManager.default.removeItem(at: data) }
        let game = try GameFolder(["Game.ini": ini("[Game]\nRTP=RPGVXAce\n"), "Game.rgss3a": Data()])
        let layout = try #require(RGSSLayout(game.probe))
        let script = try GameFolder(["zz_vndegen.rb": Data("# preload".utf8)])
        let paths = RGSSPaths(folder: game.root, dataDirectory: data, layout: layout, runtimes: ["RPGVXAce": installed.root],
                              preload: script.root.appendingPathComponent("zz_vndegen.rb"))
        try paths.prepare()
        try paths.prepare()
        let link = paths.rtpFolder.appendingPathComponent("RPGVXAce/Graphics/System/Window.png")
        #expect(FileManager.default.fileExists(atPath: link.path))
        #expect(FileManager.default.contents(atPath: paths.preloadFolder.appendingPathComponent("zz_vndegen.rb").path) == Data("# preload".utf8))
    }
}

@Suite struct SaveTests {
    @Test func pcSavePathsMapToTheirFileName() {
        #expect(RGSSPlugin.saveRelativePath("C:/Games/Example/Save01.rvdata2") == "Save01.rvdata2")
        #expect(RGSSPlugin.saveRelativePath("/home/deck/Games/Example/Save1.rxdata") == "Save1.rxdata")
        #expect(RGSSPlugin.saveRelativePath("/home/deck/Games/Example/save3.rvdata") == "save3.rvdata")
        #expect(RGSSPlugin.saveRelativePath("C:/Games/Example/Data/Scripts.rvdata2") == nil)
        #expect(RGSSPlugin.saveRelativePath("C:/Games/Example/Game.ini") == nil)
    }
}

@Suite struct InputTests {
    @Test func buttonsPressRPGMakersKeys() {
        var router = RGSSInputRouter()
        #expect(router.translate(.button(.confirm, pressed: true)) == [.key(RetroKey.return, down: true)])
        #expect(router.translate(.button(.confirm, pressed: false)) == [.key(RetroKey.return, down: false)])
        #expect(router.translate(.button(.dash, pressed: true)) == [.key(RetroKey.leftShift, down: true)])
        #expect(router.translate(.button(.menu, pressed: true)) == [.key(RetroKey.ascii("x"), down: true)])
        #expect(router.translate(.button(.hub, pressed: true)).isEmpty)
    }

    @Test func aKeyHeldByTwoButtonsGoesUpWithTheLastRelease() {
        var router = RGSSInputRouter()
        let enter = KeyboardKey(key: "Enter", code: "Enter", keyCode: 13)
        #expect(router.translate(.button(.confirm, pressed: true)) == [.key(RetroKey.return, down: true)])
        #expect(router.translate(.key(enter, pressed: true)).isEmpty)
        #expect(router.translate(.button(.confirm, pressed: false)).isEmpty)
        #expect(router.translate(.key(enter, pressed: false)) == [.key(RetroKey.return, down: false)])
    }

    @Test func xboxButtonsPressRPGMakersKeys() {
        var router = RGSSInputRouter()
        #expect(router.translate(.gamepad(.a, pressed: true)) == [.key(RetroKey.return, down: true)])
        #expect(router.translate(.gamepad(.y, pressed: true)) == [.key(RetroKey.ascii("x"), down: true)])
        // The D-pad and the left stick share the arrow keys.
        #expect(router.translate(.gamepad(.up, pressed: true)) == [.key(RetroKey.up, down: true)])
        #expect(router.translate(.gamepad(.leftStickUp, pressed: true)).isEmpty)
        #expect(router.translate(.gamepad(.up, pressed: false)).isEmpty)
        #expect(router.translate(.gamepad(.leftStickUp, pressed: false)) == [.key(RetroKey.up, down: false)])
        #expect(router.translate(.gamepad(.view, pressed: true)).isEmpty)
    }

    @Test func remappedKeys() {
        #expect(KeyboardKey(key: "z", code: "KeyZ", keyCode: 90).retroKey == RetroKey.ascii("z"))
        #expect(KeyboardKey(key: "5", code: "Digit5", keyCode: 53).retroKey == RetroKey.ascii("5"))
        #expect(KeyboardKey(key: "F5", code: "F5", keyCode: 116).retroKey == RetroKey.f1 + 4)
        #expect(KeyboardKey(key: "ArrowUp", code: "ArrowUp", keyCode: 38).retroKey == RetroKey.up)
        #expect(KeyboardKey(key: "?", code: "Unknown", keyCode: 0).retroKey == nil)
    }

    @Test func hardwareKeyboardUsages() {
        #expect(RetroKey.hidUsage(29) == RetroKey.ascii("z"))
        #expect(RetroKey.hidUsage(39) == RetroKey.ascii("0"))
        #expect(RetroKey.hidUsage(40) == RetroKey.return)
        #expect(RetroKey.hidUsage(82) == RetroKey.up)
        #expect(RetroKey.hidUsage(225) == RetroKey.leftShift)
        #expect(RetroKey.hidUsage(98) == RetroKey.keypad0)
        #expect(RetroKey.hidUsage(89) == RetroKey.keypad0 + 1)
    }

    @Test func releaseAllAfterPause() {
        var router = RGSSInputRouter()
        _ = router.translate(.button(.left, pressed: true))
        _ = router.hostKey(RetroKey.ascii("z"), pressed: true, isRepeat: false)
        #expect(router.hostKey(RetroKey.ascii("z"), pressed: true, isRepeat: true).isEmpty)
        #expect(router.releaseAll() == [.key(RetroKey.ascii("z"), down: false), .key(RetroKey.left, down: false)])
    }

    @Test func clicksLandWhereThePointerWas() {
        var router = RGSSInputRouter()
        let point = CGPoint(x: 0.25, y: 0.75)
        _ = router.translate(.pointer(PointerEvent(phase: .moved, location: point)))
        #expect(router.translate(.click(.primary, pressed: true)) == [.mouseMove(to: point), .mouseButton(0, down: true)])
    }
}
