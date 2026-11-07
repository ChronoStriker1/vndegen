#if os(macOS)
import Foundation
import TestSupport
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
        // The player's own logs and caches aren't saves.
        for file in ["Player.log", "Player-prev.log", "output_log.txt", "Unity/Analytics/config"] {
            #expect(info.relativePath("C:/Users/pc/AppData/LocalLow/Studio/My Game/\(file)") == nil)
        }
        #expect(info.relativePath("C:/Users/pc/AppData/LocalLow/Studio/My Game/saves/Player.log") == "saves/Player.log")
    }

    @Test func readsAppInfoWhateverItsCase() throws {
        let folder = TestFolders.root.appending(path: "unity-\(UUID().uuidString)")
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
        let url = TestFolders.root.appending(path: "bottle-\(UUID().uuidString)").resolvingSymlinksInPath()
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
            bottle.close("Game.exe", environment: inherited), bottle.close("Game.exe", force: true, environment: inherited),
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
        #expect(Array(commands[4].arguments.suffix(5)) == ["--no-convert", "taskkill", "/f", "/im", "Game.exe"])
        #expect(commands[2].arguments.contains("winemenubuilder.exe=d"))
        #expect(commands[2].arguments.last == "/games/G/Game.exe" && !commands[2].arguments.contains("--cx-app"))
        #expect(Array(commands[3].arguments.suffix(4)) == ["--no-convert", "taskkill", "/im", "Game.exe"])
        #expect(Array(commands[5].arguments.suffix(4)) == ["--no-convert", "regedit", "/S", #"Z:\tmp\a.reg"#])
        #expect(Array(commands[6].arguments.suffix(5)) == ["--no-convert", "regedit", "/E", #"Z:\tmp\b.reg"#, #"HKEY_CURRENT_USER\Software\S\G"#])
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
        #expect(throws: UnityBottle.Refusal.self) { try bottle.close("Game.exe", environment: [:]) }
        #expect(throws: UnityBottle.Refusal.self) { try bottle.harden() }

        let crossOver = FileManager.default.homeDirectoryForCurrentUser.appending(path: "Library/Application Support/CrossOver/Bottles")
        #expect(throws: UnityBottle.Refusal.self) { try UnityBottle(folder: crossOver, tools: Self.tools).checkOwnBottle() }
        #expect(throws: Never.self) { try UnityBottle(folder: base.appending(path: "Fresh"), tools: Self.tools).checkOwnBottle() }
    }

    /// A bottle of VNDegen's own whose C: drive or registry file links into another bottle is refused too, and so is
    /// save sync's folder there.
    @Test func refusesLinksOutOfTheBottle() throws {
        let fm = FileManager.default
        let base = try Self.scratch()
        let theirs = base.appending(path: "Steam")
        try fm.createDirectory(at: theirs.appending(path: "drive_c/users/crossover/AppData/LocalLow/Studio/Game"), withIntermediateDirectories: true)
        try Data().write(to: theirs.appending(path: "user.reg"))
        for part in ["drive_c", "user.reg", "dosdevices/c:"] {
            let folder = base.appending(path: "Bottles-\(part.replacingOccurrences(of: "/", with: "-"))")
            let bottle = UnityBottle(folder: folder, tools: Self.tools)
            try fm.createDirectory(at: bottle.path.appending(path: "dosdevices"), withIntermediateDirectories: true)
            try fm.createSymbolicLink(at: bottle.path.appending(path: part), withDestinationURL: theirs.appending(path: "drive_c"))
            #expect(throws: UnityBottle.Refusal.self) { try bottle.checkOwnBottle() }
            #expect(throws: UnityBottle.Refusal.self) { try bottle.installFonts(from: base) }
            if part == "drive_c" {
                let game = try UnityRouteTests.game()
                defer { try? fm.removeItem(at: game) }
                let data = game.appending(path: "data")
                try UnityPlugin(bottles: folder).selectVariant("crossover", for: Game(id: GameID("g"), title: "G"), dataDirectory: data)
                #expect(UnityPlugin(bottles: folder).saveLocations(folder: game, dataDirectory: data).isEmpty)
            }
        }
        // A link inside that stays in the bottle is fine.
        let folder = base.appending(path: "Bottles-ok")
        let bottle = UnityBottle(folder: folder, tools: Self.tools)
        try fm.createDirectory(at: bottle.path.appending(path: "real_c"), withIntermediateDirectories: true)
        try fm.createSymbolicLink(atPath: bottle.path.appending(path: "drive_c").path, withDestinationPath: "real_c")
        #expect(throws: Never.self) { try bottle.checkOwnBottle() }
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
@Suite struct UnitySetupTests {
    /// Saves set aside while the bottle is made come back whole, even after a setup that stopped halfway left some in
    /// each place.
    @Test func mergeKeepsEverySave() throws {
        let fm = FileManager.default
        let base = TestFolders.root.appending(path: "merge-\(UUID().uuidString)")
        defer { try? fm.removeItem(at: base) }
        let aside = base.appending(path: "aside"), saves = base.appending(path: "saves")
        try fm.createDirectory(at: aside.appending(path: "Studio/Game"), withIntermediateDirectories: true)
        try fm.createDirectory(at: saves.appending(path: "Studio/Game"), withIntermediateDirectories: true)
        try fm.createDirectory(at: saves.appending(path: "Other/Game"), withIntermediateDirectories: true)
        try Data("a".utf8).write(to: aside.appending(path: "Studio/Game/1.sav"))
        try Data("b".utf8).write(to: saves.appending(path: "Studio/Game/2.sav"))
        try Data("c".utf8).write(to: saves.appending(path: "Other/Game/3.sav"))
        try UnityCrossOverSession.merge(saves, into: aside)
        #expect(!fm.fileExists(atPath: saves.path))
        try UnityCrossOverSession.merge(aside, into: saves)
        #expect(!fm.fileExists(atPath: aside.path))
        for (path, text) in [("Studio/Game/1.sav", "a"), ("Studio/Game/2.sav", "b"), ("Other/Game/3.sav", "c")] {
            #expect(try String(contentsOf: saves.appending(path: path), encoding: .utf8) == text)
        }
    }
}

@MainActor
@Suite struct UnityPlayerPrefsTests {
    /// The import writes the game's key alone: the file's values, and deletions of the values the file lacks. It never
    /// deletes the key, which would take its subkeys along.
    @Test func importChangesOnlyTheGamesValues() throws {
        let key = #"HKEY_CURRENT_USER\Software\Studio\Game"#
        let text = UnityCrossOverSession.importText(key: key, values: ["b": .dword(2), "a": .binary([1, 0])],
                                                    removing: ["A", "old", "z\"q", "x\n\n[HKEY_CURRENT_USER]\n;"])
        #expect(text == """
            Windows Registry Editor Version 5.00

            [\(key)]
            "a"=hex:01,00
            "b"=dword:00000002
            "old"=-
            "z\\"q"=-

            """)
        #expect(text.split(separator: "\n").filter { $0.hasPrefix("[") } == ["[\(key)]"])
        let parsed = try RegistryData(reg: text)
        #expect(parsed.keys.keys.sorted() == [key] && parsed.values(of: key) == ["a": .binary([1, 0]), "b": .dword(2)])
    }
}

@MainActor
@Suite struct UnityMacAppTests {
    /// An app from a zip made on Windows has no execute bits. Only its main executable gets them.
    @Test func restoresTheExecuteBitOfTheMainExecutable() throws {
        let fm = FileManager.default
        let app = TestFolders.root.appending(path: "mac-\(UUID().uuidString)/Game.app")
        defer { try? fm.removeItem(at: app.deletingLastPathComponent()) }
        try fm.createDirectory(at: app.appending(path: "Contents/MacOS"), withIntermediateDirectories: true)
        try fm.createDirectory(at: app.appending(path: "Contents/Frameworks"), withIntermediateDirectories: true)
        let plist: NSDictionary = ["CFBundleExecutable": "Game"]
        plist.write(to: app.appending(path: "Contents/Info.plist"), atomically: true)
        for path in ["Contents/MacOS/Game", "Contents/Frameworks/UnityPlayer.dylib"] {
            try Data().write(to: app.appending(path: path))
            try fm.setAttributes([.posixPermissions: 0o644], ofItemAtPath: app.appending(path: path).path)
        }
        try UnityMacAppSession.makeRunnable(app)
        #expect(fm.isExecutableFile(atPath: app.appending(path: "Contents/MacOS/Game").path))
        #expect(!fm.isExecutableFile(atPath: app.appending(path: "Contents/Frameworks/UnityPlayer.dylib").path))
    }
}

@Suite struct UnityRouteTests {
    /// A folder with a Windows build and a Mac build beside it, as SiNiSistar 2 ships.
    static func game() throws -> URL {
        let folder = TestFolders.root.appending(path: "unity-\(UUID().uuidString)")
        let fm = FileManager.default
        try fm.createDirectory(at: folder.appending(path: "WIN/Game_Data"), withIntermediateDirectories: true)
        try Data().write(to: folder.appending(path: "WIN/Game_Data/globalgamemanagers"))
        try Data("Studio\nGame".utf8).write(to: folder.appending(path: "WIN/Game_Data/app.info"))
        try Data().write(to: folder.appending(path: "WIN/Game.exe"))
        try fm.createDirectory(at: folder.appending(path: "MAC/Game.app/Contents/Resources/Data"), withIntermediateDirectories: true)
        try Data().write(to: folder.appending(path: "MAC/Game.app/Contents/Resources/Data/globalgamemanagers"))
        let plist: NSDictionary = ["CFBundleIdentifier": "com.Studio.Game", "CFBundleExecutable": "Game"]
        plist.write(to: folder.appending(path: "MAC/Game.app/Contents/Info.plist"), atomically: true)
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
        #expect(macSaves.directory.path.hasSuffix("/Library/Application Support/com.Studio.Game"))
        // A PC backup's saves and a Mac's land in the same place.
        #expect(macSaves.relativePath("C:/Users/pc/AppData/LocalLow/Studio/Game/SaveData/s1.dat") == "SaveData/s1.dat")
        #expect(macSaves.relativePath("/Users/me/Library/Application Support/com.studio.game/SaveData/s1.dat") == "SaveData/s1.dat")
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
