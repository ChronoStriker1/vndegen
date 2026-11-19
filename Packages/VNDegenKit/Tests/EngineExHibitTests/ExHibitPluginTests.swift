import CEXHIBIT
import Foundation
import Testing
import TestSupport
import VNCore
@testable import EngineExHibit

@Suite struct ExHibitLayoutTests {
    private func probe(_ entries: [String]) -> FolderProbe { FolderProbe(root: URL(fileURLWithPath: "/game"), entries: Set(entries)) }

    @Test func findsTheIniBesideTheScripts() {
        let layout = ExHibitLayout(probe(["ExHIBIT.exe", "ExHIBIT.ini", "resident.dll", "rld/start.rld", "rld/title.rld"]))
        #expect(layout?.root == "")
        #expect(layout?.evidence == "ExHIBIT.ini")
    }

    @Test func findsAGameOneFolderDownIgnoringCase() {
        let layout = ExHibitLayout(probe(["Succubus Sessions/exhibit.INI", "Succubus Sessions/RESIDENT.DLL", "Succubus Sessions/RLD/Start.rld"]))
        #expect(layout?.root == "Succubus Sessions")
    }

    @Test func wantsTheEngineAndTheFirstScript() {
        #expect(ExHibitLayout(probe(["ExHIBIT.ini", "resident.dll"])) == nil)
        #expect(ExHibitLayout(probe(["ExHIBIT.ini", "rld/start.rld"])) == nil)
        #expect(ExHibitLayout(probe(["a/b/ExHIBIT.ini", "a/b/resident.dll", "a/b/rld/start.rld"])) == nil)
    }
}

@Suite struct ExHibitSaveTests {
    @Test func recognizesTheUserFolderFiles() {
        #expect(ExHibitPlugin.saveName("C:\\Games\\Mami\\userdata\\u0") == "u0")
        #expect(ExHibitPlugin.saveName("/x/userdata/u89") == "u89")
        #expect(ExHibitPlugin.saveName("/x/userdata/udb") == "udb")
        #expect(ExHibitPlugin.saveName("/x/userdata/Succubus Sessions 1.system.dat") == "Succubus Sessions 1.system.dat")
        #expect(ExHibitPlugin.saveName("/x/userdata/thumbnail/U00000.gyu") == "thumbnail/U00000.gyu")
        #expect(ExHibitPlugin.saveName("/x/userdata/vndegen/slot003.sav") == "vndegen/slot003.sav")
        #expect(ExHibitPlugin.saveName("settings.dat") == "settings.dat")
        #expect(ExHibitPlugin.saveName("/x/userdata/u") == nil)
        #expect(ExHibitPlugin.saveName("/x/userdata/notes.txt") == nil)
        #expect(ExHibitPlugin.saveName("/x/rld/start.rld") == nil)
        #expect(ExHibitPlugin.saveName("/x/userdata/thumbnail/a/b.gyu") == nil)
    }
}

@Suite struct ExHibitProbeTests {
    @Test func probeReadsTheTitleFromTheIni() throws {
        let root = TestFolders.root.appending(path: "exhibit-probe-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root.appending(path: "rld"), withIntermediateDirectories: true)
        try Data([0]).write(to: root.appending(path: "rld/start.rld"))
        try Data([0]).write(to: root.appending(path: "resident.dll"))
        try Data("[release]\r\nentry=start\r\n[setting]\r\nTITLE=Test Sessions\r\nN_REG=2048\r\n".utf8).write(to: root.appending(path: "ExHIBIT.ini"))
        var title = [CChar](repeating: 0, count: 256)
        #expect(vnexhibit_probe(root.path, &title, title.count))
        #expect(String(decoding: title.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self) == "Test Sessions")
    }

    @Test func probeRefusesAFolderWithoutScripts() throws {
        let root = TestFolders.root.appending(path: "exhibit-empty-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        try Data("TITLE=x\r\n".utf8).write(to: root.appending(path: "ExHIBIT.ini"))
        var title = [CChar](repeating: 0, count: 256)
        #expect(!vnexhibit_probe(root.path, &title, title.count))
    }

    @Test func openRefusesAFolderWithoutAGame() {
        var problem = [CChar](repeating: 0, count: 256)
        let engine = "/nonexistent".withCString { dir -> OpaquePointer? in
            var config = VNExHibitConfig()
            config.gameDir = dir
            return vnexhibit_open(&config, &problem, problem.count)
        }
        #expect(engine == nil)
        #expect(String(decoding: problem.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self) == "No ExHibit game was found in this folder.")
    }
}
