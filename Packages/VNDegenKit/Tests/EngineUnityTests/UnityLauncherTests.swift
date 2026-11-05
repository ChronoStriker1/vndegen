#if os(macOS)
import Foundation
import RegistryFile
import Testing
import VNCore
@testable import EngineUnity

@Suite struct UnityAppInfoTests {
    @Test func readsCompanyAndProduct() {
        #expect(UnityAppInfo(text: "Studio\nGame") == UnityAppInfo(company: "Studio", product: "Game"))
        #expect(UnityAppInfo(text: "Studio\r\nGame\r\n")?.product == "Game")
        #expect(UnityAppInfo(text: "スタジオ\nゲーム")?.playerPrefsKey == #"HKEY_CURRENT_USER\Software\スタジオ\ゲーム"#)
    }

    /// Neither name may lead out of its folder or into another registry key, or break a `.reg` key line.
    @Test func refusesNamesThatCouldEscape() {
        for (company, product) in [("", "Game"), ("Studio", ""), ("..", "Game"), ("Studio", "a/b"), (#"a\b"#, "Game"),
                                   ("Studio", "G]ame"), ("St[udio", "Game"), ("Studio\u{0}", "Game"), ("Studio", "Ga\tme")] {
            #expect(UnityAppInfo(company: company, product: product) == nil)
        }
        #expect(UnityAppInfo(text: "Studio") == nil)
    }

    @Test func mapsSavePathsFromEveryPlatform() throws {
        let info = try #require(UnityAppInfo(company: "Studio", product: "My Game"))
        #expect(info.relativePath("C:/Users/pc/AppData/LocalLow/Studio/My Game/save1.dat") == "save1.dat")
        #expect(info.relativePath("/home/deck/.local/share/Steam/steamapps/compatdata/1/pfx/drive_c/users/steamuser/AppData/LocalLow/studio/my game/s/a.json") == "s/a.json")
        #expect(info.relativePath("/Users/me/Library/Application Support/Studio/My Game/save1.dat") == "save1.dat")
        #expect(info.relativePath("C:/Users/pc/AppData/LocalLow/Studio/Other/save1.dat") == nil)
        #expect(info.relativePath("C:/Users/pc/AppData/LocalLow/Studio/My Game/../x") == nil)
        #expect(info.relativePath("C:/Users/pc/AppData/LocalLow/Studio/My Game/") == nil)
    }

    @Test func readsAppInfoWhateverItsCase() throws {
        let folder = FileManager.default.temporaryDirectory.appending(path: "unity-\(UUID().uuidString)")
        defer { try? FileManager.default.removeItem(at: folder) }
        try FileManager.default.createDirectory(at: folder.appending(path: "Game_Data"), withIntermediateDirectories: true)
        try Data().write(to: folder.appending(path: "Game_Data/globalgamemanagers"))
        try Data().write(to: folder.appending(path: "Game.exe"))
        try Data("Studio\nGame".utf8).write(to: folder.appending(path: "Game_Data/App.Info"))
        let layout = try #require(UnityLayout(scanning: folder))
        #expect(UnityAppInfo(folder: folder, layout: layout) == UnityAppInfo(company: "Studio", product: "Game"))
        #expect(layout.exe(in: folder)?.lastPathComponent == "Game.exe")
    }
}

@Suite struct UnityBottleTests {
    private static func scratch() throws -> URL {
        let url = FileManager.default.temporaryDirectory.appending(path: "bottle-\(UUID().uuidString)").resolvingSymlinksInPath()
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        return url
    }

    private static let tools = URL(fileURLWithPath: "/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin")

    /// Every call names the bottle by its absolute path, so a CX_BOTTLE_PATH from the player's CrossOver.conf can't send
    /// it elsewhere, and nothing in the environment names another bottle.
    @Test func everyCommandNamesTheBottleByItsPath() throws {
        let folder = try Self.scratch().appending(path: "Bottles")
        let bottle = UnityBottle(folder: folder, tools: Self.tools)
        let inherited = ["CX_BOTTLE": "Steam", "WINEPREFIX": "/Users/me/Library/Application Support/CrossOver/Bottles/Steam",
                         "CX_BOTTLE_PATH": "/elsewhere", "CX_MANAGED_BOTTLE_PATH": "/managed", "HOME": "/Users/me"]
        let exe = URL(fileURLWithPath: "/games/G/Game.exe")
        let commands = try [
            bottle.create(environment: inherited), bottle.update(environment: inherited),
            bottle.run(exe, workdir: exe.deletingLastPathComponent(), environment: inherited),
            bottle.close("Game.exe", environment: inherited), bottle.killAll(environment: inherited),
            bottle.importRegistry(URL(fileURLWithPath: "/tmp/a.reg"), environment: inherited),
            bottle.exportRegistry(#"HKEY_CURRENT_USER\Software\S\G"#, to: URL(fileURLWithPath: "/tmp/b.reg"), environment: inherited),
        ]
        for command in commands {
            #expect(Array(command.arguments.prefix(4)) == ["--bottle", folder.appending(path: "VNDegen").path, "--scope", "private"])
            #expect(command.environment["CX_BOTTLE_PATH"] == folder.path)
            #expect(command.environment["WINEPREFIX"] == nil && command.environment["CX_BOTTLE"] == nil)
            #expect(command.environment["CX_MANAGED_BOTTLE_PATH"] == nil && command.environment["HOME"] == "/Users/me")
        }
        #expect(commands[0].executable.lastPathComponent == "cxbottle" && !commands[0].arguments.contains("--no-update"))
        #expect(commands[1].arguments[4] == "--update-only")
        for command in commands.dropFirst(2) { #expect(command.arguments[4] == "--no-update") }
        #expect(commands[4].arguments.suffix(3) == ["--ux-app", "wineserver", "-k"])
        #expect(commands[2].arguments.contains("winemenubuilder.exe=d"))
    }

    @Test func refusesABottleOutsideItsOwnFolder() throws {
        let base = try Self.scratch()
        let folder = base.appending(path: "Bottles")
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        let theirs = base.appending(path: "Steam")
        try FileManager.default.createDirectory(at: theirs, withIntermediateDirectories: true)
        try FileManager.default.createSymbolicLink(at: folder.appending(path: "VNDegen"), withDestinationURL: theirs)
        let bottle = UnityBottle(folder: folder, tools: Self.tools)
        #expect(throws: UnityBottle.Refusal.self) { try bottle.checkOwnBottle() }
        #expect(throws: UnityBottle.Refusal.self) { try bottle.killAll(environment: [:]) }
        #expect(throws: UnityBottle.Refusal.self) { try bottle.harden() }

        let crossOver = FileManager.default.homeDirectoryForCurrentUser.appending(path: "Library/Application Support/CrossOver/Bottles")
        #expect(throws: UnityBottle.Refusal.self) { try UnityBottle(folder: crossOver, tools: Self.tools).checkOwnBottle() }
        #expect(throws: Never.self) { try UnityBottle(folder: base.appending(path: "Fresh"), tools: Self.tools).checkOwnBottle() }
    }

    /// The user folders CrossOver links to the Mac's own become the bottle's, and only C: and Z: stay.
    @Test func hardeningKeepsTheGameInsideTheBottle() throws {
        let base = try Self.scratch()
        let bottle = UnityBottle(folder: base.appending(path: "Bottles"), tools: Self.tools)
        let fm = FileManager.default
        let home = base.appending(path: "home")
        for name in ["Desktop", "Documents", "Downloads"] { try fm.createDirectory(at: home.appending(path: name), withIntermediateDirectories: true) }
        try fm.createDirectory(at: bottle.userFolder.appending(path: "AppData"), withIntermediateDirectories: true)
        for name in ["Desktop", "Documents", "Downloads"] {
            try fm.createSymbolicLink(at: bottle.userFolder.appending(path: name), withDestinationURL: home.appending(path: name))
        }
        try fm.createSymbolicLink(atPath: bottle.userFolder.appending(path: "Inside").path, withDestinationPath: "AppData")
        let devices = bottle.path.appending(path: "dosdevices")
        try fm.createDirectory(at: devices, withIntermediateDirectories: true)
        try fm.createSymbolicLink(atPath: devices.appending(path: "c:").path, withDestinationPath: "../drive_c")
        try fm.createSymbolicLink(atPath: devices.appending(path: "z:").path, withDestinationPath: "/")
        try fm.createSymbolicLink(atPath: devices.appending(path: "y:").path, withDestinationPath: home.path)
        try fm.createSymbolicLink(atPath: devices.appending(path: "com1").path, withDestinationPath: "/dev/null")

        try bottle.harden()
        for name in ["Desktop", "Documents", "Downloads"] {
            let item = bottle.userFolder.appending(path: name)
            #expect((try? fm.destinationOfSymbolicLink(atPath: item.path)) == nil)
            #expect(try fm.contentsOfDirectory(atPath: item.path) == [UnityBottle.keepFile])
        }
        #expect((try? fm.destinationOfSymbolicLink(atPath: bottle.userFolder.appending(path: "Inside").path)) == "AppData")
        #expect(try fm.contentsOfDirectory(atPath: devices.path).sorted() == ["c:", "com1", "z:"])
        // The Mac's own folders are untouched.
        #expect(try fm.contentsOfDirectory(atPath: home.path).sorted() == ["Desktop", "Documents", "Downloads"])
    }

    @Test func copiesOnlyTheCoreFonts() throws {
        let base = try Self.scratch()
        let source = base.appending(path: "Supplemental")
        try FileManager.default.createDirectory(at: source, withIntermediateDirectories: true)
        for name in ["Arial.ttf", "Arial Bold Italic.ttf", "Arial Narrow.ttf", "Arial Unicode.ttf", "Impact.ttf", "Tahoma Bold.ttf",
                     "Times New Roman.ttf", "Baskerville.ttc"] {
            try Data(name.utf8).write(to: source.appending(path: name))
        }
        let bottle = UnityBottle(folder: base.appending(path: "Bottles"), tools: Self.tools)
        try bottle.installFonts(from: source)
        let fonts = bottle.driveC.appending(path: "windows/Fonts")
        #expect(try FileManager.default.contentsOfDirectory(atPath: fonts.path).sorted()
            == ["Arial Bold Italic.ttf", "Arial.ttf", "Impact.ttf", "Tahoma Bold.ttf", "Times New Roman.ttf"])
    }
}

@MainActor
@Suite struct UnityPlayerPrefsTests {
    /// The import deletes the game's key, then writes it, and touches nothing else.
    @Test func importReplacesOnlyTheGamesKey() throws {
        let key = #"HKEY_CURRENT_USER\Software\Studio\Game"#
        let text = UnityCrossOverSession.importText(key: key, values: ["b": .dword(2), "a": .binary([1, 0])])
        #expect(text == """
            Windows Registry Editor Version 5.00

            [-\(key)]

            [\(key)]
            "a"=hex:01,00
            "b"=dword:00000002

            """)
        let keyLines = text.split(separator: "\n").filter { $0.hasPrefix("[") }
        #expect(keyLines == ["[-\(key)]", "[\(key)]"])
    }
}

@Suite struct UnityRouteTests {
    /// A folder with a Windows build and a Mac build beside it, as SiNiSistar 2 ships.
    private static func game() throws -> URL {
        let folder = FileManager.default.temporaryDirectory.appending(path: "unity-\(UUID().uuidString)")
        let fm = FileManager.default
        try fm.createDirectory(at: folder.appending(path: "WIN/Game_Data"), withIntermediateDirectories: true)
        try Data().write(to: folder.appending(path: "WIN/Game_Data/globalgamemanagers"))
        try Data("Studio\nGame".utf8).write(to: folder.appending(path: "WIN/Game_Data/app.info"))
        try Data().write(to: folder.appending(path: "WIN/Game.exe"))
        try fm.createDirectory(at: folder.appending(path: "MAC/Game.app/Contents/Resources/Data"), withIntermediateDirectories: true)
        try Data().write(to: folder.appending(path: "MAC/Game.app/Contents/Resources/Data/globalgamemanagers"))
        return folder
    }

    @Test func aShippedMacVersionWinsUntilThePlayerPicksCrossOver() throws {
        let folder = try Self.game(), data = folder.appending(path: "data")
        defer { try? FileManager.default.removeItem(at: folder) }
        let bottles = folder.appending(path: "Bottles")
        let plugin = UnityPlugin(bottles: bottles)
        let game = Game(id: GameID("g"), title: "G")
        #expect(plugin.playLabel(for: game, folder: folder, dataDirectory: data) == "Open Mac version")
        #expect(plugin.registrySave(folder: folder, dataDirectory: data) == nil)
        let macSaves = try #require(plugin.saveLocations(folder: folder, dataDirectory: data).first)
        #expect(macSaves.directory.path.hasSuffix("/Library/Application Support/Studio/Game"))
        #expect(plugin.variants(for: game, folder: folder, dataDirectory: data)?.selected == nil)

        try plugin.selectVariant("crossover", for: game, dataDirectory: data)
        #expect(plugin.playLabel(for: game, folder: folder, dataDirectory: data) == "Play in CrossOver")
        #expect(plugin.registrySave(folder: folder, dataDirectory: data)
            == RegistrySave(key: #"HKEY_CURRENT_USER\Software\Studio\Game"#, file: data.appending(path: "PlayerPrefs.reg")))
        let bottleSaves = try #require(plugin.saveLocations(folder: folder, dataDirectory: data).first)
        #expect(bottleSaves.directory.path == bottles.appending(path: "VNDegen/drive_c/users/crossover/AppData/LocalLow/Studio/Game").path)
        // Local files there are recognized as saves by the same rule.
        #expect(bottleSaves.relativePath(bottleSaves.directory.appending(path: "slot1.sav").path) == "slot1.sav")

        try plugin.selectVariant(nil, for: game, dataDirectory: data)
        #expect(plugin.playLabel(for: game, folder: folder, dataDirectory: data) == "Open Mac version")
    }

    @Test func noAppInfoMeansNoSaveSync() throws {
        let folder = try Self.game()
        defer { try? FileManager.default.removeItem(at: folder) }
        try FileManager.default.removeItem(at: folder.appending(path: "WIN/Game_Data/app.info"))
        let plugin = UnityPlugin(bottles: folder.appending(path: "Bottles"))
        #expect(plugin.saveLocations(folder: folder, dataDirectory: folder).isEmpty)
        #expect(plugin.registrySave(folder: folder, dataDirectory: folder) == nil)
    }
}
#endif
