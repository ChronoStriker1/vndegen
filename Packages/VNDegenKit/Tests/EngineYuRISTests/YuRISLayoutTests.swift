import Foundation
import Testing
import VNCore
@testable import EngineYuRIS

@Suite struct YuRISLayoutTests {
    private func probe(_ entries: [String]) -> FolderProbe { FolderProbe(root: URL(fileURLWithPath: "/game"), entries: Set(entries)) }

    @Test func findsArchivesInPac() {
        let layout = YuRISLayout(probe(["yscfg.dat", "pac", "pac/bn.ypf", "pac/cg.ypf", "game.exe"]))
        #expect(layout?.root == "")
        #expect(layout?.evidence == "pac/bn.ypf")
    }

    @Test func findsArchivesBesideTheConfigOneFolderDown() {
        let layout = YuRISLayout(probe(["Game", "Game/YSCFG.DAT", "Game/ysbin.ypf", "Game/bgm.ypf"]))
        #expect(layout?.root == "Game")
        #expect(layout?.evidence == "Game/bgm.ypf")
    }

    @Test func findsTheScriptArchiveWithoutAConfig() {
        let layout = YuRISLayout(probe(["09 Game", "09 Game/ysbin.ypf", "09 Game/cg.ypf", "09 Game/game.exe"]))
        #expect(layout?.root == "09 Game")
        #expect(layout?.evidence == "09 Game/ysbin.ypf")
        #expect(YuRISLayout(probe(["pac/YSBIN.YPF", "pac/cg.ypf"]))?.root == "")
        #expect(YuRISLayout(probe(["a/b/c/ysbin.ypf"])) == nil)
    }

    @Test func wantsTheConfigAndAnArchive() {
        #expect(YuRISLayout(probe(["yscfg.dat", "data.pak"])) == nil)
        #expect(YuRISLayout(probe(["pac/bn.ypf"])) == nil)
        #expect(YuRISLayout(probe(["yscfg.dat", "pac/deep/bn.ypf"])) == nil)
    }

    @Test func findsArchivesInPacBelowTheListing() throws {
        let folder = FileManager.default.temporaryDirectory.appending(path: "yuris-layout-\(UUID().uuidString)")
        let game = folder.appending(path: "v1/Game")
        try FileManager.default.createDirectory(at: game.appending(path: "pac"), withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: folder) }
        try Data().write(to: game.appending(path: "yscfg.dat"))
        try Data().write(to: game.appending(path: "pac/cg1.ypf"))
        let layout = YuRISLayout(FolderProbe(scanning: folder))
        #expect(layout?.root == "v1/Game")
        #expect(layout?.evidence == "v1/Game/pac/cg1.ypf")
        try FileManager.default.removeItem(at: game.appending(path: "pac/cg1.ypf"))
        #expect(YuRISLayout(FolderProbe(scanning: folder)) == nil)
    }

    @Test func savesAreSdFilesInTheSaveFolder() throws {
        let folder = FileManager.default.temporaryDirectory.appending(path: "yuris-layout-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: folder.appending(path: "pac"), withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: folder) }
        try Data().write(to: folder.appending(path: "yscfg.dat"))
        try Data().write(to: folder.appending(path: "pac/bn.ypf"))
        let locations = YuRISPlugin().saveLocations(folder: folder, dataDirectory: folder)
        #expect(locations.count == 1)
        #expect(locations.first?.directory.lastPathComponent == "save")
    }
}
