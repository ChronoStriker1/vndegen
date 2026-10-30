import Foundation
import Testing
import VNCore
@testable import EngineAliceSoft

@Suite struct AliceSoftDetectionTests {
    private func layout(_ entries: [String]) -> AliceSoftLayout? {
        AliceSoftLayout(FolderProbe(root: URL(fileURLWithPath: "/game"), entries: Set(entries)))
    }

    @Test func system39AtTheTop() throws {
        let l = try #require(layout(["System39.ain", "System39.exe", "RanceSA.ALD", "RanceGA.ALD"]))
        #expect(l.system == .system39)
        #expect(l.root == "")
        #expect(l.confidence == .certain)
    }

    @Test func system39WithoutItsExecutable() throws {
        // Some releases ship only the data; the scenario archive is enough.
        let l = try #require(layout(["System39.ain", "tt2SA.ALD", "tt2GA.ald", "bgm/tt2_02.mp3"]))
        #expect(l.system == .system39)
    }

    @Test func system39AinAloneIsNotAGame() {
        #expect(layout(["System39.ain"]) == nil)
    }

    @Test func system30OneFolderDown() throws {
        let l = try #require(layout(["Rance 4.1 32bit/SYSTEM3.EXE", "Rance 4.1 32bit/ADISK.DAT", "Rance 4.1 32bit/ACG.dat",
                                     "Rance 4.1 64bit/readme.txt"]))
        #expect(l.system == .system30)
        #expect(l.root == "Rance 4.1 32bit")
        #expect(l.confidence == .certain)
    }

    @Test func system4WithAliceStart() throws {
        let l = try #require(layout(["Rance02.ain", "AliceStart.ini", "DLL/x.dll"]))
        #expect(l.system == .system4)
    }

    @Test func system4WithSystem40() throws {
        let l = try #require(layout(["Rance7.ain", "System40.ini", "System40.exe"]))
        #expect(l.system == .system4)
    }

    @Test func plainAinIsNotSystem4() {
        #expect(layout(["Something.ain", "Game.exe"]) == nil)
    }
}
