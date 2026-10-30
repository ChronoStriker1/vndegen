import Foundation
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

    @Test func savesBesideGameExeInTheFolderAsSpelled() throws {
        let root = FileManager.default.temporaryDirectory.appending(path: "wolfrpg-\(UUID().uuidString)")
        defer { try? FileManager.default.removeItem(at: root) }
        for path in ["Game/Game.exe", "Game/Data.wolf", "Game/save/SaveData01.sav"] {
            let url = root.appending(path: path)
            try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
            try Data().write(to: url)
        }
        let locations = WolfRPGPlugin().saveLocations(folder: root, dataDirectory: root.appending(path: "data"))
        let location = try #require(locations.first)
        #expect(locations.count == 1)
        #expect(location.directory.standardizedFileURL.path == root.appending(path: "Game/save").standardizedFileURL.path)
    }
}
