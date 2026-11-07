import CoreGraphics
import Foundation
import TestSupport
import Testing
import VNCore
@testable import EngineWolfRPG

@Suite struct WolfRPGLayoutTests {
    private func probe(_ entries: Set<String>) -> FolderProbe {
        FolderProbe(root: URL(filePath: "/games/x", directoryHint: .isDirectory), entries: entries)
    }

    @Test func findsLooseDataBesideGameExe() throws {
        let layout = try #require(WolfRPGLayout(probe(["Game.exe", "Data", "Data/BasicData", "Data/MapData"])))
        #expect(layout.root == "" && layout.evidence == "Data/BasicData" && layout.confidence == .certain)
    }

    @Test func findsAnArchivedGameAFolderDown() throws {
        let layout = try #require(WolfRPGLayout(probe(["Game", "Game/Game.exe", "Game/Data.wolf", "readme.txt"])))
        #expect(layout.root == "Game" && layout.confidence == .certain)
        let pro = try #require(WolfRPGLayout(probe(["Game", "Game/GamePro.exe", "Game/Data", "Game/Data/BasicData.wolf"])))
        #expect(pro.root == "Game" && pro.evidence == "Game/Data/BasicData.wolf")
    }

    @Test func findsADeepGameByItsMidiPlayer() throws {
        let entries: Set<String> = ["A", "A/B", "A/B/Game.exe", "A/B/Data", "A/B/GuruguruSMF4.dll"]
        let layout = try #require(WolfRPGLayout(probe(entries)))
        #expect(layout.root == "A/B" && layout.evidence == "A/B/GuruguruSMF4.dll")
        // The DLL alone, without Game.exe and Data beside it, isn't a game.
        #expect(WolfRPGLayout(probe(["A", "A/GuruguruSMF4.dll"])) == nil)
    }

    @Test func prefersTheGameWithItsExe() throws {
        let entries: Set<String> = ["Backup", "Backup/Data.wolf", "Game", "Game/Game.exe", "Game/Data.wolf"]
        let layout = try #require(WolfRPGLayout(probe(entries)))
        #expect(layout.root == "Game" && layout.confidence == .certain)
        #expect(WolfRPGLayout(probe(["Data.wolf"]))?.confidence == .likely)
    }

    /// Hash order would find the sorted first of these only 1 time in 26.
    @Test func aTieGoesToTheFirstInSortedOrder() throws {
        let roots = (UInt8(ascii: "a")...UInt8(ascii: "z")).map { String(UnicodeScalar($0)) }
        let layout = try #require(WolfRPGLayout(probe(Set(roots.flatMap { ["\($0)/Game.exe", "\($0)/Data.wolf"] }))))
        #expect(layout.root == "a" && layout.evidence == "a/Data.wolf")
        let evidence = try #require(WolfRPGLayout(probe(["Game.exe", "Data", "Data.wolf", "Data/BasicData", "Data/BasicData.wolf",
                                                         "GuruguruSMF4.dll"])))
        #expect(evidence.evidence == "Data.wolf")
    }

    /// A real folder, because only a scan can see past the listing's three levels.
    private func folder(_ files: [String]) throws -> URL {
        let root = TestFolders.root.appendingPathComponent(UUID().uuidString, isDirectory: true)
        for file in files {
            let url = root.appendingPathComponent(file)
            try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
            try Data().write(to: url)
        }
        return root
    }

    @Test func findsAGameThreeFoldersDown() throws {
        let root = try folder(["Outer/Inner/hspext.dll", "Outer/Inner/sys/Game.exe", "Outer/Inner/sys/Data/BasicData.wolf",
                               "Outer/Inner/sys/GuruguruSMF4.dll",
                               // A bonus game, deeper still, doesn't win over the game itself.
                               "Outer/Inner/Bonus/Extra/Game.exe", "Outer/Inner/Bonus/Extra/Data.wolf"])
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(WolfRPGLayout(FolderProbe(scanning: root)) == nil)
        let layout = try #require(WolfRPGLayout(scanning: root))
        #expect(layout.root == "Outer/Inner/sys" && layout.evidence == "Outer/Inner/sys/GuruguruSMF4.dll" && layout.confidence == .certain)
        #expect(WolfRPGLayout(deepening: FolderProbe(scanning: root)) == layout)
    }

    @Test func aLongListingIsNotScannedDeeper() throws {
        let filler = (0..<201).map { "Outer/Inner/file\($0).png" }
        let root = try folder(filler + ["Outer/Inner/sys/Game.exe", "Outer/Inner/sys/Data.wolf"])
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(WolfRPGLayout(scanning: root) == nil)
    }

    /// Data.wolf three folders down, with no Game.exe beside it, could be anything.
    @Test func aDeepFolderNeedsItsExe() throws {
        let root = try folder(["Outer/Inner/sys/Data.wolf", "Outer/Inner/sys/readme.txt"])
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(WolfRPGLayout(scanning: root) == nil)
    }

    @Test func ignoresOtherGames() {
        #expect(WolfRPGLayout(probe(["Game.exe", "data", "data/Map001.rvdata2", "Game.ini"])) == nil)
    }
}

@Suite struct WolfRPGSaveLocationTests {
    @Test func mapsFilesDirectlyInTheSaveFolder() {
        #expect(WolfRPGPlugin.saveRelativePath("C:/Games/Game/Save/SaveData01.sav") == "SaveData01.sav")
        #expect(WolfRPGPlugin.saveRelativePath("/home/x/Game/save/System.sav") == "System.sav")
        #expect(WolfRPGPlugin.saveRelativePath("C:/Games/Game/Save/") == nil)
        #expect(WolfRPGPlugin.saveRelativePath("C:/Games/Game/Data/Save.dat") == nil)
        #expect(WolfRPGPlugin.saveRelativePath("Save/..") == nil)
    }

    @Test func savesInTheSaveFolderBesideGameExe() throws {
        let root = TestFolders.root.appending(path: "wolfrpg-\(UUID().uuidString)")
        defer { try? FileManager.default.removeItem(at: root) }
        for path in ["Game/Game.exe", "Game/Data.wolf", "Game/Save/SaveData01.sav"] {
            let url = root.appending(path: path)
            try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
            try Data().write(to: url)
        }
        let locations = WolfRPGPlugin().saveLocations(folder: root, dataDirectory: root.appending(path: "data"))
        let location = try #require(locations.first)
        #expect(locations.count == 1)
        #expect(location.directory.resolvingSymlinksInPath().path
            == root.appending(path: "Game/Save").resolvingSymlinksInPath().path)
    }
}

@Suite struct WolfRPGInputTests {
    @Test func pointerDrivesTheMouse() {
        var router = WolfRPGInputRouter()
        let at = CGPoint(x: 0.5, y: 0.25)
        #expect(router.pointer(PointerEvent(phase: .began, location: at), at: at) == [.mouseMove(at), .mouseButton(0, down: true)])
        #expect(router.pointer(PointerEvent(phase: .moved, location: at), at: at) == [.mouseMove(at)])
        #expect(router.pointer(PointerEvent(phase: .ended, location: at), at: at) == [.mouseMove(at), .mouseButton(0, down: false)])
    }

    @Test func aBoundRightClickHoldsUntilLetGo() {
        var router = WolfRPGInputRouter()
        #expect(router.translate(.click(.secondary, pressed: true)) { $0 } == [.mouseButton(1, down: true)])
        #expect(router.releaseAll() == [.mouseButton(1, down: false)])
    }

    @Test func scrollingUpTurnsTheWheelAwayFromThePlayer() {
        var router = WolfRPGInputRouter()
        #expect(router.translate(.scroll(dy: -WolfRPGInputRouter.pointsPerNotch)) { $0 } == [.wheel(1)])
    }
}
