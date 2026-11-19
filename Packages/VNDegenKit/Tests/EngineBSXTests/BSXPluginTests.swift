import CBSX
import Foundation
import Testing
import TestSupport
import VNCore
@testable import EngineBSX

@Suite struct BSXLayoutTests {
    private func probe(_ entries: [String]) -> FolderProbe { FolderProbe(root: URL(fileURLWithPath: "/game"), entries: Set(entries)) }

    @Test func findsTheScriptBesideTheArchives() {
        let layout = BSXLayout(probe(["mkiedl.exe", "boot.exe", "bsxx.dat", "bsxx.cfg", "Graphics.bsa", "bgm.bsa"]))
        #expect(layout?.root == "")
        #expect(layout?.evidence == "bsxx.dat")
    }

    @Test func findsAGameOneFolderDownIgnoringCase() {
        let layout = BSXLayout(probe(["MK2IEDL/BSXX.DAT", "MK2IEDL/bsxx.CFG", "MK2IEDL/GraphicsBU.BSA"]))
        #expect(layout?.root == "MK2IEDL")
    }

    @Test func wantsTheConfigAndPictures() {
        #expect(BSXLayout(probe(["bsxx.dat", "Graphics.bsa"])) == nil)
        #expect(BSXLayout(probe(["bsxx.dat", "bsxx.cfg"])) == nil)
        #expect(BSXLayout(probe(["a/b/bsxx.dat", "a/b/bsxx.cfg", "a/b/Graphics.bsa"])) == nil)
    }
}

@Suite struct BSXSaveTests {
    @Test func recognizesTheEngineSaveFiles() {
        #expect(BSXPlugin.saveName("C:\\Games\\MKIEDL\\save\\no015") == "no015")
        #expect(BSXPlugin.saveName("/x/save/no000") == "no000")
        #expect(BSXPlugin.saveName("/x/save/no-01") == "no-01")
        #expect(BSXPlugin.saveName("/x/save/common.dat") == "common.dat")
        #expect(BSXPlugin.saveName("/x/save/no15") == nil)
        #expect(BSXPlugin.saveName("/x/save/no0151") == nil)
        #expect(BSXPlugin.saveName("/x/save/notes.txt") == nil)
        #expect(BSXPlugin.saveName("/x/bsxx.dat") == nil)
    }
}

@Suite struct BSXRuntimeTests {
    private func folder(_ name: String) throws -> URL {
        let root = TestFolders.root.appending(path: "\(name)-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        return root
    }

    @Test func probeReadsTheTitleFromBsxxIni() throws {
        let root = try folder("bsx-probe")
        var script = Data("BSXScript 3.0".utf8)
        script.append(Data(count: 0xa8))
        try script.write(to: root.appending(path: "bsxx.dat"))
        var ini = Data()
        for field in ["TEST@BISHOP", "Test Teacher", "Taro"] {
            ini.append(field.data(using: .utf16LittleEndian)!)
            ini.append(Data([0, 0]))
        }
        try ini.write(to: root.appending(path: "bsxx.ini"))
        var title = [CChar](repeating: 0, count: 256)
        #expect(vnbsx_probe(root.path, &title, title.count))
        #expect(String(decoding: title.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self) == "Test Teacher")
    }

    @Test func probeRefusesAnotherScript() throws {
        let root = try folder("bsx-other")
        try Data("BSXScript 2.0 and more".utf8).write(to: root.appending(path: "bsxx.dat"))
        var title = [CChar](repeating: 0, count: 256)
        #expect(!vnbsx_probe(root.path, &title, title.count))
    }
}
