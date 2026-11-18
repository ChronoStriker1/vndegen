import CEAGLS
import Foundation
import Testing
import TestSupport
import VNCore
@testable import EngineEAGLS

@Suite struct EAGLSLayoutTests {
    private func probe(_ entries: [String]) -> FolderProbe { FolderProbe(root: URL(fileURLWithPath: "/game"), entries: Set(entries)) }

    @Test func findsTheArchivesBesideTheExe() {
        let layout = EAGLSLayout(probe(["MILK48.exe", "EAGLS.dll", "CG/CGPACK.idx", "CG/CGPACK.pak", "Script/SCPACK.idx", "Script/SCPACK.pak"]))
        #expect(layout?.root == "")
        #expect(layout?.evidence == "Script/SCPACK.idx")
    }

    @Test func findsAGameOneFolderDownIgnoringCase() {
        let layout = EAGLSLayout(probe(["[ENG] Game/cg/cgpack.IDX", "[ENG] Game/SCRIPT/scpack.idx", "[ENG] Game/SCRIPT/SCPACK.PAK"]))
        #expect(layout?.root == "[ENG] Game")
    }

    @Test func wantsThePicturesBesideTheScripts() {
        #expect(EAGLSLayout(probe(["Script/SCPACK.idx", "Script/SCPACK.pak"])) == nil)
        #expect(EAGLSLayout(probe(["Script/SCPACK.idx", "CG/CGPACK.idx"])) == nil)
        #expect(EAGLSLayout(probe(["a/b/Script/SCPACK.idx", "a/b/Script/SCPACK.pak", "a/b/CG/CGPACK.idx"])) == nil)
    }
}

@Suite struct EAGLSSaveTests {
    @Test func recognizesTheEngineSaveFiles() {
        #expect(EAGLSPlugin.saveName("C:\\Games\\MILK48\\Save\\Save0.sav") == "Save0.sav")
        #expect(EAGLSPlugin.saveName("/x/Save/Save79.sav") == "Save79.sav")
        #expect(EAGLSPlugin.saveName("/x/Save/GlobalSave.sav") == "GlobalSave.sav")
        #expect(EAGLSPlugin.saveName("/x/Save/VNDegen.cfg") == "VNDegen.cfg")
        #expect(EAGLSPlugin.saveName("/x/Save/Save.sav") == nil)
        #expect(EAGLSPlugin.saveName("/x/Save/Save100.sav") == nil)
        #expect(EAGLSPlugin.saveName("/x/CG/CGPACK.pak") == nil)
    }
}

@Suite struct EAGLSRuntimeTests {
    @Test func probeReadsTheWindowTitle() throws {
        let root = TestFolders.root.appending(path: "eagls-probe-\(UUID().uuidString)")
        for folder in ["CG", "Script"] {
            try FileManager.default.createDirectory(at: root.appending(path: folder), withIntermediateDirectories: true)
        }
        for file in ["CG/CGPACK.idx", "CG/CGPACK.pak", "Script/SCPACK.idx", "Script/SCPACK.pak"] {
            try Data([0]).write(to: root.appending(path: file))
        }
        try Data("[WindowState]\r\nWindowTitle=Test Kingdom\r\nScreenXsize=800\r\n".utf8).write(to: root.appending(path: "EAGLS.ini"))
        var title = [CChar](repeating: 0, count: 256)
        #expect(vneagls_probe(root.path, &title, title.count))
        #expect(String(decoding: title.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self) == "Test Kingdom")
    }

    @Test func probeRefusesAFolderWithoutArchives() throws {
        let root = TestFolders.root.appending(path: "eagls-empty-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        var title = [CChar](repeating: 0, count: 256)
        #expect(!vneagls_probe(root.path, &title, title.count))
    }
}
