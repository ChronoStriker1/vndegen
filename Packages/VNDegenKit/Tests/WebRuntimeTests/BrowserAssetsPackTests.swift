import Foundation
import Testing
@testable import WebRuntime

/// Writes a pack the way Embedded Browser's build step does, with .NET's `BinaryWriter`: the magic string, the entry
/// count, each entry's path, absolute offset and length, then the files in order. `entries` overrides the index.
func writeBrowserAssets(to url: URL, files: [(String, Data)], entries: [(String, Int64, Int32)]? = nil) throws {
    func string(_ text: String) -> Data {
        var bytes = Data(), length = text.utf8.count
        repeat {
            bytes.append(UInt8(length & 0x7F) | (length > 0x7F ? 0x80 : 0))
            length >>= 7
        } while length > 0
        return bytes + Data(text.utf8)
    }
    func little<T: FixedWidthInteger>(_ value: T) -> Data { withUnsafeBytes(of: value.littleEndian) { Data($0) } }
    let names = files.map { "/" + $0.0 }
    var header = string("zfbRes_v1") + little(Int32(files.count))
    let indexSize = names.reduce(0) { $0 + string($1).count + 12 }
    var offset = Int64(header.count + indexSize)
    var index = Data()
    for (i, (name, data)) in zip(names, files.map(\.1)).enumerated() {
        let entry = entries?[i] ?? (name, offset, Int32(data.count))
        index += string(entry.0) + little(entry.1) + little(entry.2)
        offset += Int64(data.count)
    }
    header += index
    try (header + files.reduce(Data()) { $0 + $1.1 }).write(to: url)
}

private func unityGame(files: [(String, Data)], entries: [(String, Int64, Int32)]? = nil) throws -> URL {
    let root = FileManager.default.temporaryDirectory.appendingPathComponent("vndegen-zfb-\(UUID().uuidString)", isDirectory: true)
    let resources = root.appendingPathComponent("Asher_Data/Resources", isDirectory: true)
    try FileManager.default.createDirectory(at: resources, withIntermediateDirectories: true)
    try writeBrowserAssets(to: resources.appendingPathComponent("browser_assets"), files: files, entries: entries)
    return root
}

private func text(_ response: FileResponse) async throws -> String {
    var body = Data()
    try await response.body?.stream { body.append($0); return true }
    return String(decoding: body, as: UTF8.self)
}

@Test func browserAssetsMembersAreServedFromInsideThePack() async throws {
    let root = try unityGame(files: [
        ("index.html", Data("<html>story</html>".utf8)), ("aya/1_smirk.jpg", Data("jpeg".utf8)), ("Death.jpg", Data("death".utf8)),
    ])
    defer { try? FileManager.default.removeItem(at: root) }
    let folder = GameFolder(root: root, webRoot: "Asher_Data/Resources/browser_assets")
    #expect(folder.archive != nil)
    let index = folder.serve(path: "/")
    #expect(index.status == 200 && index.headers["Content-Type"] == "text/html")
    #expect(try await text(index) == "<html>story</html>")
    #expect(try await text(folder.serve(path: "/aya/1_smirk.jpg")) == "jpeg")
    #expect(try await text(folder.serve(path: "/death.JPG")) == "death")
    #expect(try await text(folder.serve(path: "/aya/../Death.jpg")) == "death")
    let part = folder.serve(path: "/Death.jpg", range: "bytes=1-2")
    #expect(part.status == 206 && part.headers["Content-Range"] == "bytes 1-2/5")
    #expect(try await text(part) == "ea")
    #expect(folder.serve(path: "/aya").status == 404)
    #expect(folder.serve(path: "/missing.jpg").status == 404)
    #expect(folder.serve(path: "/../../../etc/passwd").status == 404)
}

@Test func aBrowserAssetsPackWithASpanOutsideTheFileServesNothing() throws {
    let html = Data("<html></html>".utf8)
    for entry in [("/index.html", Int64(1) << 40, Int32(html.count)), ("/index.html", Int64(0), Int32.max), ("/index.html", Int64(-1), Int32(1)),
                  ("/index.html", Int64(0), Int32(-1))] {
        let root = try unityGame(files: [("index.html", html)], entries: [entry])
        defer { try? FileManager.default.removeItem(at: root) }
        let folder = GameFolder(root: root, webRoot: "Asher_Data/Resources/browser_assets")
        #expect(folder.archive == nil, "\(entry)")
        #expect(folder.serve(path: "/index.html").status == 404)
    }
}

@Test func aTruncatedOrForeignBrowserAssetsFileServesNothing() throws {
    let root = try unityGame(files: [("index.html", Data("<html></html>".utf8))])
    defer { try? FileManager.default.removeItem(at: root) }
    let url = root.appendingPathComponent("Asher_Data/Resources/browser_assets")
    let whole = try Data(contentsOf: url)
    for bytes in [whole.prefix(20), Data("UnityFS".utf8), Data()] {
        try bytes.write(to: url)
        #expect(GameFolder(root: root, webRoot: "Asher_Data/Resources/browser_assets").archive == nil)
    }
}
