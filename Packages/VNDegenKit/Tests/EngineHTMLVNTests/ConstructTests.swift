import Foundation
import Testing
import VNCore
import WebRuntime
@testable import EngineHTMLVN

private func exportFiles(_ prefix: String) -> [String: Data] {
    ["index.html", "data.json", "Scripts/c3runtime.js", "Scripts/main.js", "workermain.js", "images/bg-sheet0.webp"].reduce(into: [:]) { files, name in
        files[prefix + name] = Data("x".utf8)
    }
}

private func webView2Files(_ prefix: String) -> [String: Data] {
    exportFiles(prefix + "www/").merging([
        prefix + "TranspileGirl Rescue Operation!.exe": Data(count: 2048), prefix + "WebView2Loader.dll": Data(), prefix + "WV2ExtManager.dll": Data(),
        prefix + "D3D11Overlay_x64.ext.dll": Data(), prefix + "package.json": Data("{}".utf8),
    ]) { a, _ in a }
}

private func detect(_ root: URL) -> EngineMatch? { ConstructPlugin().detect(FolderProbe(scanning: root)) }

@Test func detectsAWebView2ExportWithItsGameInWww() throws {
    let root = try temporaryFolder(webView2Files(""))
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(detect(root) == EngineMatch(engine: .construct, confidence: .certain, version: "Construct 3", evidence: "www/scripts/c3runtime.js"))
    #expect(ConstructGame(folder: root) == ConstructGame(root: "", webRoot: "www"))
}

@Test func detectsAnHTML5ExportAtTheTop() throws {
    let root = try temporaryFolder(exportFiles(""))
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(detect(root) == EngineMatch(engine: .construct, confidence: .certain, version: "Construct 3", evidence: "scripts/c3runtime.js"))
}

@Test func detectsAGameInsideTheArchivesFolder() throws {
    let root = try temporaryFolder(webView2Files("Transpile Girl Rescue Operation! [v2024.03.03]/"))
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(ConstructGame(folder: root) == ConstructGame(root: "Transpile Girl Rescue Operation! [v2024.03.03]", webRoot: "www"))
    #expect(detect(root) == EngineMatch(engine: .construct, confidence: .likely, version: "Construct 3",
                                       evidence: "Transpile Girl Rescue Operation! [v2024.03.03]/www/scripts/c3runtime.js"))
}

@Test func otherHTMLGamesAreNotConstruct() throws {
    var files = exportFiles("")
    files["Scripts/c3runtime.js"] = nil
    files["c2runtime.js"] = Data()
    files["js/rpg_core.js"] = Data()
    let root = try temporaryFolder(files)
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(detect(root) == nil)
    let construct = try temporaryFolder(webView2Files(""))
    defer { try? FileManager.default.removeItem(at: construct) }
    #expect(VisualNovelMakerPlugin().detect(FolderProbe(scanning: construct)) == nil)
    #expect(TwinePlugin().detect(FolderProbe(scanning: construct)) == nil)
}

@Test func savesStayInTheGamesLocalStorageFiles() throws {
    let data = URL(fileURLWithPath: "/VNDegen/game", isDirectory: true)
    let location = try #require(ConstructPlugin().saveLocations(folder: data, dataDirectory: data).first)
    #expect(location.directory == data.appendingPathComponent("localStorage", isDirectory: true))
    #expect(location.relativePath("/VNDegen/game/localStorage/c3-savegames-z6syg6o8s3d%2Fslot1.value") == "c3-savegames-z6syg6o8s3d%2Fslot1.value")
}

@Test func controlsAreTheVisualNovelLayoutWithAClicking() {
    #expect(ConstructPlugin().defaultControls == .visualNovel)
    #expect(ConstructPlugin().defaultControlChanges == [.a: .click(.primary)])
}

@Test func aWebView2ExportListsItsWrapperAndKeepsTheGame() throws {
    let root = try temporaryFolder(webView2Files(""))
    defer { try? FileManager.default.removeItem(at: root) }
    let list = try #require(ConstructPlugin().engineFileList(FolderProbe(scanning: root)))
    #expect(list.files == ["transpilegirl rescue operation!.exe", "webview2loader.dll", "wv2extmanager.dll", "d3d11overlay_x64.ext.dll"])
    #expect(list.folders.isEmpty && list.trees.isEmpty)
    #expect(!list.lists(file: "package.json", size: 2) && !list.lists(file: "www/index.html", size: 1))

    let wrapped = try temporaryFolder(webView2Files("Transpile Girl/"))
    defer { try? FileManager.default.removeItem(at: wrapped) }
    #expect(ConstructPlugin().engineFileList(FolderProbe(scanning: wrapped)) == nil)
    let export = try temporaryFolder(exportFiles(""))
    defer { try? FileManager.default.removeItem(at: export) }
    #expect(ConstructPlugin().engineFileList(FolderProbe(scanning: export)) == nil)
}

@Test func twoExecutablesLeaveBothInPlace() throws {
    let root = try temporaryFolder(webView2Files("").merging(["Launcher.exe": Data()]) { a, _ in a })
    defer { try? FileManager.default.removeItem(at: root) }
    let list = try #require(ConstructPlugin().engineFileList(FolderProbe(scanning: root)))
    #expect(list.files == ["webview2loader.dll", "wv2extmanager.dll", "d3d11overlay_x64.ext.dll"])
}

private func nwjsFiles() -> [String: Data] {
    exportFiles("package.nw/").merging([
        "DungeonVixens.exe": Data(count: 2048), "nw.dll": Data(), "node.dll": Data(), "nw_elf.dll": Data(), "icudtl.dat": Data(),
        "resources.pak": Data(), "locales/en-US.pak": Data(), "goggame-1817545222.info": Data("{}".utf8),
    ]) { a, _ in a }
}

@Test func detectsAnNWjsExportUnpackedIntoPackageNW() throws {
    let root = try temporaryFolder(nwjsFiles())
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(detect(root) == EngineMatch(engine: .construct, confidence: .certain, version: "Construct 3", evidence: "package.nw/scripts/c3runtime.js"))
    #expect(ConstructGame(folder: root) == ConstructGame(root: "", webRoot: "package.nw"))
}

@Test func anNWjsExportListsNWjsAndKeepsTheGame() throws {
    let root = try temporaryFolder(nwjsFiles())
    defer { try? FileManager.default.removeItem(at: root) }
    let list = try #require(ConstructPlugin().engineFileList(FolderProbe(scanning: root)))
    #expect(["nw.dll", "node.dll", "nw_elf.dll", "icudtl.dat", "resources.pak"].allSatisfy { list.lists(file: $0, size: 0) })
    #expect(list.lists(file: "locales/en-US.pak", size: 0))
    #expect(!list.lists(file: "package.nw/index.html", size: 1) && !list.lists(file: "goggame-1817545222.info", size: 2))
}
