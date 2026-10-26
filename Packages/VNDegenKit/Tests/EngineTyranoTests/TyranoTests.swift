import Foundation
import Testing
import VNCore
import WebRuntime
@testable import EngineTyrano

private func game(_ files: [String: String]) throws -> URL {
    let root = FileManager.default.temporaryDirectory.appendingPathComponent("vndegen-tyrano-\(UUID().uuidString)", isDirectory: true)
    for (path, text) in files {
        let url = root.appendingPathComponent(path)
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data(text.utf8).write(to: url)
    }
    return root
}

/// A minimal app.asar: the size pickle, the header pickle with the JSON index, then the files.
private func writeTestAsar(to url: URL, files: [String: String]) throws {
    var root: [String: Any] = [:]
    var body = Data()
    for (path, text) in files.sorted(by: { $0.key < $1.key }) {
        let parts = path.split(separator: "/").map(String.init)
        func put(_ node: inout [String: Any], _ index: Int) {
            if index == parts.count - 1 { node[parts[index]] = ["size": text.utf8.count, "offset": String(body.count)]; return }
            var directory = node[parts[index]] as? [String: Any] ?? ["files": [String: Any]()]
            var children = directory["files"] as! [String: Any]
            put(&children, index + 1)
            directory["files"] = children
            node[parts[index]] = directory
        }
        put(&root, 0)
        body.append(Data(text.utf8))
    }
    let json = try JSONSerialization.data(withJSONObject: ["files": root])
    func uint32(_ value: Int) -> Data { withUnsafeBytes(of: UInt32(value).littleEndian) { Data($0) } }
    var payload = uint32(json.count) + json
    payload.append(Data(count: (4 - payload.count % 4) % 4))
    let headerPickle = uint32(payload.count) + payload
    try (uint32(4) + uint32(headerPickle.count) + headerPickle + body).write(to: url)
}

private let kag450 = "tyrano.plugin.kag={version:450,tyrano:null,kag:null"
private let kag520 = "tyrano.plugin.kag = {\n    version: 520,\n    tyrano: null,"

private func detect(_ root: URL) -> EngineMatch? { TyranoPlugin().detect(FolderProbe(scanning: root)) }

@Test func detectsALooseNWjsBuildWithFileSaves() throws {
    let root = try game([
        "index.html": "<html>", "package.json": #"{"main":"app://./index.html"}"#,
        "tyrano/plugins/kag/kag.js": kag450,
        "data/system/Config.tjs": ";projectID=TestGame01\n;configSave=file\n;configSaveSlotNum=14\n",
    ])
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(detect(root) == EngineMatch(engine: .tyrano, confidence: .certain, version: "4.50", evidence: "tyrano/plugins/kag/kag.js"))
    let tyrano = try #require(TyranoGame(folder: root))
    #expect(tyrano == TyranoGame(webRoot: "", packaging: .nwjs, version: "4.50", projectID: "TestGame01", savesToFiles: true))
    #expect(TyranoPlugin.runtime(for: tyrano) == .nwjs(execPath: "/game/Game.app/Contents/MacOS/nwjs"))

    let location = try #require(TyranoPlugin().saveLocations(folder: root, dataDirectory: root).first)
    #expect(location.directory == root)
    #expect(location.relativePath("C:/Games/Test Game 01/TestGame01_sf.sav") == "TestGame01_sf.sav")
    #expect(location.relativePath("/home/player/_TyranoGameData/testgame01_tyrano_data.sav") == "testgame01_tyrano_data.sav")
    #expect(location.relativePath("C:/Games/Test Game 01/OtherGame_sf.sav") == nil)
    #expect(location.relativePath("C:/Games/Test Game 01/TestGame01.exe") == nil)
}

@Test func detectsAnElectronBuildInResourcesApp() throws {
    let root = try game([
        "resources/app/index.html": "<html>", "resources/app/package.json": "{}", "resources/app/main.js": "",
        "resources/app/tyrano/plugins/kag/kag.js": kag520,
        "resources/app/data/system/Config.tjs": "// 設定\n;projectID = testgame02;\n;configSave     = file\n",
    ])
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(detect(root)?.evidence == "resources/app/tyrano/plugins/kag/kag.js")
    #expect(detect(root)?.version == "5.20")
    let tyrano = try #require(TyranoGame(folder: root))
    #expect(tyrano.packaging == .electron && tyrano.projectID == "testgame02" && tyrano.savesToFiles)
    #expect(TyranoPlugin.runtime(for: tyrano) == .electron(appPath: "/game/Game.app/Contents/Resources/app"))
}

@Test func detectsAnElectronBuildPackedInAppAsar() throws {
    let root = try game(["testgame02.exe": ""])
    defer { try? FileManager.default.removeItem(at: root) }
    try FileManager.default.createDirectory(at: root.appendingPathComponent("resources"), withIntermediateDirectories: true)
    try writeTestAsar(to: root.appendingPathComponent("resources/app.asar"), files: [
        "index.html": "<html>", "tyrano/plugins/kag/kag.js": kag520,
        "data/system/Config.tjs": ";projectID = packed;\n;configSave = webstorage\n",
    ])
    let tyrano = try #require(TyranoGame(folder: root))
    #expect(tyrano == TyranoGame(webRoot: "resources/app.asar", packaging: .electron, version: "5.20", projectID: "packed", savesToFiles: false))
    #expect(TyranoPlugin.runtime(for: tyrano) == .electron(appPath: "/game/Game.app/Contents/Resources/app.asar"))
    #expect(detect(root)?.evidence == "resources/app.asar/tyrano/plugins/kag/kag.js")
}

@Test func aBrowserExportSavesToLocalStorage() throws {
    let root = try game([
        "index.html": "<html>", "tyrano/plugins/kag/kag.js": kag450,
        "data/system/Config.tjs": ";projectID=web\n;configSave=file\n",
    ])
    defer { try? FileManager.default.removeItem(at: root) }
    let tyrano = try #require(TyranoGame(folder: root))
    #expect(tyrano.packaging == .browser && !tyrano.savesToFiles)
    #expect(TyranoPlugin.runtime(for: tyrano) == .browser)
    let data = root.appendingPathComponent("Data")
    let location = try #require(TyranoPlugin().saveLocations(folder: root, dataDirectory: data).first)
    #expect(location.directory == data.appendingPathComponent("localStorage", isDirectory: true))
    #expect(location.relativePath(data.path + "/localStorage/web_sf.value") == "web_sf.value")
}

@Test func otherFoldersAreNotTyrano() throws {
    let root = try game(["js/rmmz_core.js": "", "index.html": "<html>", "tyrano/readme.txt": ""])
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(detect(root) == nil)
    #expect(TyranoPlugin().saveLocations(folder: root, dataDirectory: root).isEmpty)
}

@Test func configSettingsReadLikeTyrano() {
    let config = "// ;projectID = commented\n;projectID = testgame03;\n; configSave = webstorage \n;empty=\n"
    #expect(TyranoGame.setting("projectID", in: config) == "testgame03")
    #expect(TyranoGame.setting("configSave", in: config) == "webstorage")
    #expect(TyranoGame.setting("empty", in: config) == nil)
    #expect(TyranoGame.setting("missing", in: config) == nil)
    #expect(TyranoGame.version(kagScript: kag450) == "4.50")
    #expect(TyranoGame.version(kagScript: "var version = 'x'") == nil)
}
