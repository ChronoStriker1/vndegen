import Foundation
import Testing
import VNCore
@testable import EngineN2System

@Suite struct N2SystemLayoutTests {
    private func probe(_ entries: [String]) -> FolderProbe { FolderProbe(root: URL(fileURLWithPath: "/game"), entries: Set(entries)) }

    @Test func findsTheArchivesBesideTheExe() {
        let layout = N2SystemLayout(probe(["hanachirasu.exe", "system.npa", "nss.npa", "cg.npa", "movie", "movie/op.mpg"]))
        #expect(layout?.root == "")
        #expect(layout?.evidence == "system.npa")
    }

    @Test func findsTheShallowestGameFolderIgnoringCase() {
        let layout = N2SystemLayout(probe(["The Song of Saya/SYSTEM.NPA", "The Song of Saya/Nss.npa", "extra/a/system.npa", "extra/a/nss.npa"]))
        #expect(layout?.root == "The Song of Saya")
        #expect(layout?.evidence == "The Song of Saya/SYSTEM.NPA")
    }

    @Test func wantsTheScriptArchiveBesideTheSystemOne() {
        #expect(N2SystemLayout(probe(["system.npa", "cg.npa"])) == nil)
        #expect(N2SystemLayout(probe(["system.npa", "data/nss.npa"])) == nil)
        #expect(N2SystemLayout(probe(["a/b/c/system.npa", "a/b/c/nss.npa"])) == nil)
        #expect(N2SystemLayout(probe(["cg.npk", "script.npk"])) == nil)
    }
}

@Suite struct N2SystemSaveTests {
    private let product = N2SystemPlugin.Product(name: "Hanachirasu", version: "version1.00")
    private let data = URL(fileURLWithPath: "/data/game1")

    @Test func keepsSavesWhereThePCKeepsThemUnderAppData() {
        let directory = N2SystemPlugin.saveDirectory(data, product: product)
        #expect(directory.path == "/data/game1/AppData/Roaming/Nitroplus/Hanachirasu/version1.00")
    }

    @Test func mapsAWindowsBackupPathToTheSlotFile() {
        let directory = N2SystemPlugin.saveDirectory(data, product: product)
        let windows = "C:/Users/player/AppData/Roaming/Nitroplus/Hanachirasu/version1.00/0001/script.npf"
        #expect(N2SystemPlugin.saveRelativePath(windows, directory: directory, product: product) == "0001/script.npf")
        let backslashes = #"C:\Users\player\AppData\Roaming\nitroplus\HANACHIRASU\Version1.00\common\val.npf"#
        #expect(N2SystemPlugin.saveRelativePath(backslashes, directory: directory, product: product) == "common/val.npf")
    }

    @Test func mapsALocalSaveAndRejectsOtherFiles() {
        let directory = N2SystemPlugin.saveDirectory(data, product: product)
        #expect(N2SystemPlugin.saveRelativePath(directory.path + "/0002/val.npf", directory: directory, product: product) == "0002/val.npf")
        #expect(N2SystemPlugin.saveRelativePath("C:/Users/player/AppData/Roaming/Nitroplus/Other/version1.00/0001/val.npf",
                                                directory: directory, product: product) == nil)
        #expect(N2SystemPlugin.saveRelativePath("C:/Games/Hanachirasu/cg.npa", directory: directory, product: product) == nil)
    }
}
