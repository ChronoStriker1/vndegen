import CANIM
import Foundation
import Testing
import TestSupport
import VNCore
@testable import EngineANIM

@Suite struct ANIMLayoutTests {
    private func probe(_ entries: [String]) -> FolderProbe { FolderProbe(root: URL(fileURLWithPath: "/game"), entries: Set(entries)) }

    @Test func findsTheScriptBesideItsDataAndTheEmoteArchive() {
        let layout = ANIMLayout(probe(["DDD.exe", "aisai_sce.dat", "aisai_define.dat", "aisai_system.dat", "cg/psbpack.dat", "cg/BG010.gax"]))
        #expect(layout?.root == "")
        #expect(layout?.base == "aisai")
        #expect(layout?.evidence == "aisai_sce.dat")
    }

    @Test func findsAGameOneFolderDownIgnoringCase() {
        let layout = ANIMLayout(probe(["MamaHaha/MGSW_SCE.DAT", "MamaHaha/mgsw_Define.dat", "MamaHaha/mgsw_system.DAT", "MamaHaha/CG/PSBPACK.dat"]))
        #expect(layout?.root == "MamaHaha")
        #expect(layout?.base == "MGSW")
    }

    @Test func wantsTheDefinesAndTheEmoteArchive() {
        #expect(ANIMLayout(probe(["aisai_sce.dat", "aisai_define.dat", "aisai_system.dat"])) == nil)
        #expect(ANIMLayout(probe(["aisai_sce.dat", "aisai_system.dat", "cg/psbpack.dat"])) == nil)
        #expect(ANIMLayout(probe(["_sce.dat", "_define.dat", "_system.dat", "cg/psbpack.dat"])) == nil)
    }
}

@Suite struct ANIMSaveTests {
    @Test func recognizesTheEngineSaveFiles() {
        #expect(ANIMPlugin.saveName("C:\\Games\\DDD\\aisai_save.dat", base: "aisai") == "aisai_save.dat")
        #expect(ANIMPlugin.saveName("/x/AISAI_SYSTEM.DAT", base: "aisai") == "AISAI_SYSTEM.DAT")
        #expect(ANIMPlugin.saveName("/x/qSavedata.dat", base: "aisai") == "qSavedata.dat")
        #expect(ANIMPlugin.saveName("/x/cg/pSaveThumb007.png", base: "aisai") == "cg/pSaveThumb007.png")
        #expect(ANIMPlugin.saveName("/x/pSaveThumb007.png", base: "aisai") == nil)
        #expect(ANIMPlugin.saveName("/x/cg/pSaveThumb07.png", base: "aisai") == nil)
        #expect(ANIMPlugin.saveName("/x/aisai_sce.dat", base: "aisai") == nil)
        #expect(ANIMPlugin.saveName("/x/mgsw_save.dat", base: "aisai") == nil)
    }
}

@Suite struct ANIMRuntimeTests {
    private func folder(_ name: String) throws -> URL {
        let root = TestFolders.root.appending(path: "\(name)-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root.appending(path: "cg"), withIntermediateDirectories: true)
        return root
    }

    @Test func probeNamesTheGameAfterItsScript() throws {
        let root = try folder("anim-probe")
        for name in ["mgsw_sce.dat", "mgsw_define.dat", "mgsw_system.dat", "cg/psbpack.dat"] {
            try Data([0, 0, 0, 1]).write(to: root.appending(path: name))
        }
        var title = [CChar](repeating: 0, count: 256)
        #expect(vnanim_probe(root.path, &title, title.count))
        #expect(String(decoding: title.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self) == "mgsw")
    }

    @Test func probeWantsTheEmoteArchive() throws {
        let root = try folder("anim-other")
        for name in ["mgsw_sce.dat", "mgsw_define.dat", "mgsw_system.dat"] {
            try Data([0, 0, 0, 1]).write(to: root.appending(path: name))
        }
        var title = [CChar](repeating: 0, count: 256)
        #expect(!vnanim_probe(root.path, &title, title.count))
    }
}
