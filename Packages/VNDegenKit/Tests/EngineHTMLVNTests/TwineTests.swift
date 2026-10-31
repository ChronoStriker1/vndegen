import Foundation
import Testing
import VNCore
import WebRuntime
@testable import EngineHTMLVN

private let harlowe = """
<!DOCTYPE html><html><head><meta charset="utf-8"><title>Asher</title></head><body><tw-story></tw-story>\
<tw-storydata name="Asher" startnode="240" creator="Twine" creator-version="2.2.1" ifid="5760B25D" format="Harlowe" \
format-version="2.1.0" options="" hidden><tw-passagedata pid="1" name="Start">Hello</tw-passagedata></tw-storydata></body></html>
"""

private func sugarCube(padding: Int) -> Data {
    Data(("<!DOCTYPE html><html><head><title>EVISCERATETHISGIRL.COM</title><style>" + String(repeating: "x", count: padding)
        + "</style></head><body><tw-storydata name=\"E\" format=\"SugarCube\" format-version=\"2.36.1\" hidden></tw-storydata></body></html>").utf8)
}

private func detect(_ root: URL) -> EngineMatch? { TwinePlugin().detect(FolderProbe(scanning: root)) }

@Test func detectsAStoryAndItsFormat() throws {
    let root = try temporaryFolder(["EVISCERATETHISGIRL.COM_v2.1/index.html": sugarCube(padding: 10), "EVISCERATETHISGIRL.COM_v2.1/README.txt": Data()])
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(detect(root) == EngineMatch(engine: .twine, confidence: .certain, version: "SugarCube 2.36.1",
                                       evidence: "EVISCERATETHISGIRL.COM_v2.1/index.html"))
    #expect(TwineGame(folder: root) == TwineGame(webRoot: "EVISCERATETHISGIRL.COM_v2.1", page: "index.html", format: "SugarCube 2.36.1"))
}

/// SugarCube puts its own styles and code before the story, about 240 KB of them, and the story may cross the end of
/// a read.
@Test func findsAStoryPastTheFirstRead() throws {
    for padding in [300 << 10, (256 << 10) - 60, (256 << 10) - 100] {
        let root = try temporaryFolder(["story.html": sugarCube(padding: padding)])
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(detect(root)?.version == "SugarCube 2.36.1", "\(padding)")
    }
    let root = try temporaryFolder(["story.html": sugarCube(padding: TwineGame.scanLimit)])
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(detect(root) == nil)
}

@Test func prefersTheShallowestStoryAndIndexHTML() throws {
    let root = try temporaryFolder([
        "a.html": Data("<html>about</html>".utf8), "b.html": Data(harlowe.utf8), "index.html": Data(harlowe.utf8),
        "extras/index.html": sugarCube(padding: 1),
    ])
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(TwineGame(folder: root) == TwineGame(webRoot: "", page: "index.html", format: "Harlowe 2.1.0"))
}

@Test func detectsTwine1AndStoriesWithNoFormat() throws {
    let root = try temporaryFolder(["game/Story.htm": Data("<html><body><div id=\"storeArea\" data-size=\"3\" hidden></div></body></html>".utf8)])
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(detect(root)?.version == "Twine 1")
    #expect(detect(root)?.evidence == "game/Story.htm")

    let bare = try temporaryFolder(["index.html": Data("<tw-storydata name=\"x\" hidden></tw-storydata>".utf8)])
    defer { try? FileManager.default.removeItem(at: bare) }
    #expect(detect(bare) == EngineMatch(engine: .twine, confidence: .certain, version: nil, evidence: "index.html"))
}

@Test func otherHTMLIsNotTwine() throws {
    let root = try temporaryFolder([
        "index.html": Data("<html><script src=\"data/ENGINE.js\"></script></html>".utf8),
        "credits.html": Data(harlowe.utf8), "LICENSES.chromium.html": Data(harlowe.utf8), "notes.txt": Data(harlowe.utf8),
    ])
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(detect(root) == nil)
}

/// Asher's Windows release is a Unity player that shows the story through Zen Fulcrum's Embedded Browser.
@Test func detectsAStoryInAnEmbeddedBrowserPack() throws {
    let root = try temporaryFolder(["Asher.exe": Data(), "Asher_Data/Plugins/zf_cef.dll": Data()])
    defer { try? FileManager.default.removeItem(at: root) }
    try writePack(to: root.appendingPathComponent("Asher_Data/Resources/browser_assets"),
                  files: [("AllWell.jpg", Data("jpeg".utf8)), ("index.html", Data(harlowe.utf8))])
    #expect(detect(root) == EngineMatch(engine: .twine, confidence: .certain, version: "Harlowe 2.1.0",
                                       evidence: "Asher_Data/Resources/browser_assets"))
    let game = try #require(TwineGame(folder: root))
    #expect(game == TwineGame(webRoot: "Asher_Data/Resources/browser_assets", page: "index.html", format: "Harlowe 2.1.0"))
    #expect(GameFolder(root: root, webRoot: game.webRoot).read(servedPath: "/allwell.JPG", limit: 10) == Data("jpeg".utf8))
}

@Test func twineSavesAreTheGamesLocalStorage() throws {
    let data = URL(fileURLWithPath: "/tmp/vndegen-data", isDirectory: true)
    let location = try #require(TwinePlugin().saveLocations(folder: data, dataDirectory: data).first)
    #expect(location.directory == data.appendingPathComponent("localStorage", isDirectory: true))
    #expect(location.relativePath("/x/localStorage/%28saved%20game%205760b25d%29%20slot1.value") == "%28saved%20game%205760b25d%29%20slot1.value")
    #expect(location.relativePath("/x/save.value") == nil)
}

@Test func startPagesAreURLPaths() {
    #expect(TwinePlugin.startPage("index.html") == "/index.html")
    #expect(TwinePlugin.startPage("My Story #2?.html") == "/My%20Story%20%232%3F.html")
    #expect(TwinePlugin().defaultControls == .keyboardAndMouse)
}

/// Embedded Browser's `browser_assets`, as `BinaryWriter` writes it.
private func writePack(to url: URL, files: [(String, Data)]) throws {
    func string(_ text: String) -> Data { Data([UInt8(text.utf8.count)]) + Data(text.utf8) }
    func little<T: FixedWidthInteger>(_ value: T) -> Data { withUnsafeBytes(of: value.littleEndian) { Data($0) } }
    let header = string("zfbRes_v1") + little(Int32(files.count))
    var offset = Int64(header.count + files.reduce(0) { $0 + string("/" + $1.0).count + 12 })
    var index = Data()
    for (name, data) in files {
        index += string("/" + name) + little(offset) + little(Int32(data.count))
        offset += Int64(data.count)
    }
    try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
    try (header + index + files.reduce(Data()) { $0 + $1.1 }).write(to: url)
}
