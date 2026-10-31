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
    let config = TyranoGame.config("""
        \u{FEFF};projectID = "testgame03";
        // ;projectID = commented
        ; configSave = webstorage \r
        ;configSave="file"; // was webstorage
        ;empty=
        ;noValue
        ;title=a=b
        ;  url = http://example.com/x;
        \u{3000};spaced = yes\u{3000}
        not ;a=setting
        """)
    #expect(config["projectID"] == "testgame03")
    #expect(config["configSave"] == "file")
    #expect(config["empty"] == "" && config["noValue"] == "")
    #expect(config["title"] == "a")
    #expect(config["url"] == "http:")
    #expect(config["spaced"] == "yes")
    #expect(config["not"] == nil && config["missing"] == nil)
    #expect(TyranoGame.config(";configSave=\"file\";\n")["configSave"] == "file")
    #expect(TyranoGame.version(kagScript: kag450) == "4.50")
    #expect(TyranoGame.version(kagScript: "var version = 'x'") == nil)
}

@Test func aQuotedFileSettingSavesToFilesAndAMissingProjectIDIsTyranosDefault() throws {
    let root = try game([
        "index.html": "<html>", "package.json": "{}", "tyrano/plugins/kag/kag.js": kag450,
        "data/system/Config.tjs": ";configSave=\"file\";\r\n",
    ])
    defer { try? FileManager.default.removeItem(at: root) }
    let tyrano = try #require(TyranoGame(folder: root))
    #expect(tyrano.savesToFiles && tyrano.projectID == "tyranoproject")
}

@Test func anElectronBuildBacksUpItsSaveKeyAndTakesTheKeyOfRestoredSaves() throws {
    let root = try game([
        "resources/app/index.html": "<html>", "resources/app/tyrano/plugins/kag/kag.js": kag520,
        "resources/app/data/system/Config.tjs": ";projectID = testgame02;\n;configSave = file\n;configSaveOverwrite = true;\n",
        // As kag.js writes it: escape(JSON.stringify(...)), here from another install whose key is PCKEY.
        "TestGame02_Tyrano_Data.sav": "%7B%22kind%22%3A%22save%22%2C%22hash%22%3A%22PCKEY0123456789a%22%2C%22data%22%3A%5B%7B%22title%22%3A%22%u3000%22%7D%5D%7D",
    ])
    defer { try? FileManager.default.removeItem(at: root) }
    let data = root.appendingPathComponent("Data")
    let tyrano = try #require(TyranoGame(folder: root))
    let storage = WebStorage(dataDirectory: data)
    #expect(TyranoPlugin.saveKeyName(projectID: "testgame02") == "/game/_testgame02")

    try storage.set("/game/_testgame02", "THISDEVICEKEY000")
    try TyranoPlugin.adoptSaveKey(of: tyrano, folder: root, storage: storage)
    #expect(try storage.value(for: "/game/_testgame02") == "PCKEY0123456789a")
    try TyranoPlugin.adoptSaveKey(of: tyrano, folder: root, storage: storage)
    #expect(try storage.load() == ["/game/_testgame02": "PCKEY0123456789a"])

    let locations = TyranoPlugin().saveLocations(folder: root, dataDirectory: data)
    #expect(locations.count == 2 && locations[1].directory == storage.directory)
    let keyFile = WebStorage.fileName(for: "/game/_testgame02")
    #expect(locations[1].relativePath(storage.directory.appendingPathComponent(keyFile).path) == keyFile)
    #expect(locations[1].relativePath("/VNDegen/game/localStorage/\(keyFile)") == keyFile)
    #expect(locations[1].relativePath(storage.directory.appendingPathComponent("testgame02_other.value").path) == nil)
}

@Test func saveKeysAreLeftAloneWhereTyranoDoesNotCheckThem() throws {
    let save = "%7B%22hash%22%3A%22PCKEY0123456789a%22%7D"
    let nwjs = try game([
        "index.html": "<html>", "package.json": "{}", "tyrano/plugins/kag/kag.js": kag450,
        "data/system/Config.tjs": ";projectID=testgame01\n;configSave=file\n", "testgame01_tyrano_data.sav": save,
    ])
    let webStorage = try game([
        "resources/app/index.html": "<html>", "resources/app/tyrano/plugins/kag/kag.js": kag520,
        "resources/app/data/system/Config.tjs": ";projectID=testgame02\n", "testgame02_tyrano_data.sav": save,
    ])
    defer { for root in [nwjs, webStorage] { try? FileManager.default.removeItem(at: root) } }
    for root in [nwjs, webStorage] {
        let storage = WebStorage(directory: root.appendingPathComponent("Data/localStorage"))
        try TyranoPlugin.adoptSaveKey(of: try #require(TyranoGame(folder: root)), folder: root, storage: storage)
        #expect(try storage.load().isEmpty)
    }
    #expect(TyranoPlugin().saveLocations(folder: nwjs, dataDirectory: nwjs).count == 1)
}

@Test func unescapeMatchesJavaScript() {
    #expect(TyranoPlugin.jsUnescape("%7B%22a%22%3A%u3000%7D") == "{\"a\":\u{3000}}")
    #expect(TyranoPlugin.jsUnescape("100%25 %zz %u12 %+1 %e9") == "100% %zz %u12 %+1 é")
}

@Test func saveKeyReaderReadsNoMoreThanItsLimit() throws {
    let root = FileManager.default.temporaryDirectory.appendingPathComponent("vndegen-bounded-\(UUID().uuidString)", isDirectory: true)
    defer { try? FileManager.default.removeItem(at: root) }
    try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
    let small = root.appendingPathComponent("small.sav"), large = root.appendingPathComponent("large.sav")
    try Data(repeating: 1, count: 10).write(to: small)
    try Data(repeating: 1, count: 11).write(to: large)
    #expect(TyranoPlugin.readBounded(small, limit: 10)?.count == 10)
    #expect(TyranoPlugin.readBounded(large, limit: 10) == nil)
    #expect(TyranoPlugin.readBounded(root, limit: 10) == nil, "a folder is not a save file")
}

@Test func tyranoGamesStartWithTheKeyboardAndMouseLayout() {
    #expect(TyranoPlugin().defaultControls == .keyboardAndMouse)
}
