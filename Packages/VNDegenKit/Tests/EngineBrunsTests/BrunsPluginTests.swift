import CBRUNS
import Foundation
import Testing
import TestSupport
import VNCore
@testable import EngineBruns

@Suite struct BrunsLayoutTests {
    private func probe(_ entries: [String]) -> FolderProbe { FolderProbe(root: URL(fileURLWithPath: "/game"), entries: Set(entries)) }

    @Test func findsTheConfigBesideTheEngine() {
        let layout = BrunsLayout(probe(["ams.cfg", "binary/bruns.exe", "scene/brs009000.bso", "scene/brs000000.bso"]))
        #expect(layout?.root == "")
        #expect(layout?.evidence == "ams.cfg")
    }

    @Test func findsAGameOneFolderDownIgnoringCase() {
        let layout = BrunsLayout(probe(["RJ124179/AMS.CFG", "RJ124179/Binary/Bruns.exe", "RJ124179/Scene/BRS009000.bso"]))
        #expect(layout?.root == "RJ124179")
    }

    @Test func wantsTheEngineAndTheSystemScript() {
        #expect(BrunsLayout(probe(["ams.cfg", "binary/bruns.exe"])) == nil)
        #expect(BrunsLayout(probe(["ams.cfg", "scene/brs009000.bso"])) == nil)
        #expect(BrunsLayout(probe(["a/b/ams.cfg", "a/b/binary/bruns.exe", "a/b/scene/brs009000.bso"])) == nil)
    }
}

@Suite struct BrunsSaveTests {
    private let product = BrunsPlugin.Product(author: "モニスタラッシュ", name: "オーク・キングダム")

    @Test func keepsTheSystemFileInTheProductFolder() {
        #expect(BrunsPlugin.saveName("C:\\Users\\me\\AppData\\Roaming\\モニスタラッシュ\\オーク・キングダム\\save\\bruns-system.dat", product: product) == "bruns-system.dat")
        #expect(BrunsPlugin.saveName("/x/AppData/Roaming/モニスタラッシュ/オーク・キングダム/save/BRUNS-SYSTEM.DAT", product: product) == "BRUNS-SYSTEM.DAT")
        #expect(BrunsPlugin.saveName("/x/モニスタラッシュ/別のゲーム/save/bruns-system.dat", product: product) == nil)
        #expect(BrunsPlugin.saveName("/x/モニスタラッシュ/オーク・キングダム/save/notes.txt", product: product) == nil)
        #expect(BrunsPlugin.saveName("/game/save/bruns-system0.dat", product: product) == nil)
    }

    @Test func savesGoUnderAppDataInTheDataFolder() {
        let dir = BrunsPlugin.saveDirectory(URL(fileURLWithPath: "/data"), product: product)
        #expect(dir.path == "/data/AppData/Roaming/モニスタラッシュ/オーク・キングダム/save")
    }
}

@Suite struct BrunsOpenTests {
    @Test func openRefusesAFolderWithoutAGame() {
        var problem = [CChar](repeating: 0, count: 256)
        let engine = "/nonexistent".withCString { dir -> OpaquePointer? in
            var config = VNBrunsConfig()
            config.gameDir = dir
            return vnbruns_open(&config, &problem, problem.count)
        }
        #expect(engine == nil)
        #expect(String(decoding: problem.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self) == "No Bruns game was found in this folder.")
    }

    @Test func productReadsTheNamesFromTheConfig() throws {
        let root = TestFolders.root.appending(path: "bruns-probe-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root.appending(path: "binary"), withIntermediateDirectories: true)
        try FileManager.default.createDirectory(at: root.appending(path: "scene"), withIntermediateDirectories: true)
        try Data([0]).write(to: root.appending(path: "binary/bruns.exe"))
        try Data([0]).write(to: root.appending(path: "scene/brs009000.bso"))
        try Data("(:product-info\r\n  (:author-name . \"Studio\")\r\n  (:product-name . \"Game\"))\r\n(:window (:window-title \"Title\"))\r\n".utf8).write(to: root.appending(path: "ams.cfg"))
        #expect(BrunsPlugin.product(root) == BrunsPlugin.Product(author: "Studio", name: "Game"))
        var title = [CChar](repeating: 0, count: 64)
        #expect(vnbruns_probe(root.path, &title, title.count))
        #expect(String(decoding: title.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self) == "Title")
    }

    @Test func productNeedsAGame() {
        var author = [CChar](repeating: 0, count: 64)
        var name = [CChar](repeating: 0, count: 64)
        #expect(!vnbruns_product("/nonexistent", &author, author.count, &name, name.count))
    }
}
