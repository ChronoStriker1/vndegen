import CSmileGame
import Foundation
import Testing
import TestSupport
import VNCore
@testable import EngineSmileGame

@Suite struct SmileGameLayoutTests {
    private func folder(_ files: [String: Data]) throws -> URL {
        let root = TestFolders.root.appending(path: "smilegame-\(UUID().uuidString)")
        for (path, data) in files {
            let url = root.appending(path: path)
            try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
            try data.write(to: url)
        }
        return root
    }

    private let package = Data("YUKARPKG".utf8) + Data([8, 0, 0, 0])

    @Test func findsThePackageBesideThePlayer() throws {
        let root = try folder(["data.sgbpack": package, "sgb_rpgplayer.exe": Data("MZ".utf8), "savedata/Save_00.sgs": Data()])
        let layout = SmileGameLayout(FolderProbe(scanning: root))
        #expect(layout?.root == "")
        #expect(layout?.evidence == "data.sgbpack")
        #expect(SmileGamePlugin().detect(FolderProbe(scanning: root))?.engine == .smileGame)
    }

    @Test func findsAGameTwoFoldersDownIgnoringCase() throws {
        let root = try folder([
            "RJ000000 170517/Game ver1.04/DATA.SGBPACK": package,
            "RJ000000 170517/Game ver1.04/SGB_RPGPlayer.exe": Data("MZ".utf8),
            "RJ000000 170517/banner.png": Data([0x89]),
        ])
        let layout = SmileGameLayout(FolderProbe(scanning: root))
        #expect(layout?.root == "RJ000000 170517/Game ver1.04")
        #expect(layout?.url(in: root).lastPathComponent == "Game ver1.04")
    }

    @Test func wantsThePlayerAndThePackageMagic() throws {
        #expect(SmileGameLayout(FolderProbe(scanning: try folder(["data.sgbpack": package]))) == nil)
        #expect(SmileGameLayout(FolderProbe(scanning: try folder(["data.sgbpack": package, "en/sgb_rpgplayer.exe": Data()]))) == nil)
        let zip = Data([0x50, 0x4b, 0x03, 0x04, 0, 0, 0, 0])
        #expect(SmileGameLayout(FolderProbe(scanning: try folder(["data.sgbpack": zip, "sgb_rpgplayer.exe": Data()]))) == nil)
    }

    @Test func savesAreInTheGamesSavedataFolder() throws {
        let root = try folder(["data.sgbpack": package, "sgb_rpgplayer.exe": Data(), "SaveData/Save_03.sgs": Data()])
        let locations = SmileGamePlugin().saveLocations(folder: root, dataDirectory: root)
        #expect(locations.count == 1)
        #expect(locations.first?.directory.lastPathComponent.lowercased() == "savedata")
        #expect(locations.first.map { FileManager.default.fileExists(atPath: $0.directory.appending(path: "Save_03.sgs").path) } == true)
    }
}

@Suite struct SmileGameSaveNameTests {
    @Test func recognizesThePlayersSaveFiles() {
        #expect(SmileGamePlugin.saveName("C:/Games/Sacred/savedata/Save_00.sgs") == "Save_00.sgs")
        #expect(SmileGamePlugin.saveName("C:\\Games\\Colette\\savedata\\Save_39.sgs") == "Save_39.sgs")
        #expect(SmileGamePlugin.saveName("/x/savedata/settings.dat") == "settings.dat")
        #expect(SmileGamePlugin.saveName("/x/savedata/Save_00.sgs.tmp") == nil)
        #expect(SmileGamePlugin.saveName("/x/savedata/Save_1.sgs") == nil)
        #expect(SmileGamePlugin.saveName("/x/Save_00.sgs") == nil)
        #expect(SmileGamePlugin.saveName("/x/data.sgbpack") == nil)
    }
}

@Suite struct SmileGameSlotTests {
    @Test func menuIDsNameASlot() {
        #expect(SmileGameSlots.choice(SmileGameSlots.saveID(0)) == .save(0))
        #expect(SmileGameSlots.choice("load.39") == .load(39))
        #expect(SmileGameSlots.choice("load.40") == nil)
        #expect(SmileGameSlots.choice("save") == nil)
        #expect(SmileGameSlots.fileName(7) == "Save_07.sgs")
    }

    @Test func listsWrittenSlotsFromTheFolder() throws {
        let root = TestFolders.root.appending(path: "smilegame-slots-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        try Data([1]).write(to: root.appending(path: "save_02.SGS"))
        let slots = SmileGameSlots.list(in: root)
        #expect(slots.count == 40)
        #expect(slots.filter { $0.written != nil }.map(\.index) == [2])
        #expect(slots[0].title == "Slot 1: empty")
    }
}

@Suite struct SmileGameInputTests {
    static let enter = KeyboardKey(key: "Enter", code: "Enter", keyCode: 13)

    @Test func buttonsPressThePlayersKeys() {
        var router = SmileGameInputRouter()
        #expect(router.translate(.button(.confirm, pressed: true)) == [SmileGameKey(code: 13, down: true)])
        #expect(router.translate(.button(.cancel, pressed: true)) == [SmileGameKey(code: 27, down: true)])
        #expect(router.translate(.button(.menu, pressed: true)) == [SmileGameKey(code: 88, down: true)])
        #expect(router.translate(.button(.dash, pressed: true)) == [SmileGameKey(code: 16, down: true)])
        #expect(router.translate(.button(.left, pressed: true)) == [SmileGameKey(code: 37, down: true)])
        #expect(router.translate(.button(.pageUp, pressed: true)) == [])
        #expect(router.translate(.pointer(PointerEvent(phase: .began, location: .zero))) == [])
    }

    @Test func aKeyHeldTwiceGoesUpWithTheLastRelease() {
        var router = SmileGameInputRouter()
        #expect(router.translate(.key(Self.enter, pressed: true)) == [SmileGameKey(code: 13, down: true)])
        #expect(router.translate(.button(.confirm, pressed: true)) == [])
        #expect(router.translate(.key(Self.enter, pressed: false)) == [])
        #expect(router.translate(.button(.confirm, pressed: false)) == [SmileGameKey(code: 13, down: false)])
    }

    @Test func hardwareKeysIgnoreRepeatsAndReleaseOnPause() {
        var router = SmileGameInputRouter()
        #expect(router.hostKey(hidUsage: 82, pressed: true) == [SmileGameKey(code: 38, down: true)])
        #expect(router.hostKey(hidUsage: 82, pressed: true) == [])
        #expect(router.hostKey(hidUsage: 27, pressed: true) == [SmileGameKey(code: 88, down: true)])
        #expect(router.releaseAll() == [SmileGameKey(code: 38, down: false), SmileGameKey(code: 88, down: false)])
        #expect(router.hostKey(hidUsage: 82, pressed: false) == [])
    }

    @Test func extraButtonsStartUnbound() {
        let changes = SmileGamePlugin().defaultControlChanges
        #expect(changes[.r] == .nothing)
        #expect(changes[.view] == .nothing)
        #expect(changes[.a] == nil)
    }
}

@Suite struct SmileGameRuntimeTests {
    @Test func probeRefusesAFolderWithoutAPackage() throws {
        let root = TestFolders.root.appending(path: "smilegame-empty-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        #expect(!vnsmilegame_probe(root.path, nil, 0))
    }

    @Test func openSaysWhyAFolderDoesntStart() throws {
        let root = TestFolders.root.appending(path: "smilegame-bad-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        try Data("PK\u{3}\u{4}notapackage".utf8).write(to: root.appending(path: "data.sgbpack"))
        var problem = [CChar](repeating: 0, count: 256)
        let started = root.path.withCString { path in
            var config = VNSmileGameConfig()
            config.gameDir = path
            config.audioRate = 44100
            return vnsmilegame_open(&config, &problem, problem.count)
        }
        #expect(started == nil)
        #expect(String(cString: problem).contains("not a Smile Game Builder package"))
    }
}
