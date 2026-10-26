import Foundation
import Testing
@testable import WebRuntime

/// Writes an app.asar the way @electron/asar does: a pickle holding the header pickle's size, the header pickle holding
/// the JSON index as a length-prefixed string padded to 4 bytes, then the packed files' bytes.
func writeAsar(to url: URL, files: [String: Data], links: [String: String] = [:], unpacked: Set<String> = []) throws {
    var root: [String: Any] = [:]
    var body = Data()
    func insert(_ path: String, _ entry: [String: Any]) {
        let parts = path.split(separator: "/").map(String.init)
        func put(_ node: inout [String: Any], _ index: Int) {
            if index == parts.count - 1 { node[parts[index]] = entry; return }
            var directory = node[parts[index]] as? [String: Any] ?? ["files": [String: Any]()]
            var children = directory["files"] as! [String: Any]
            put(&children, index + 1)
            directory["files"] = children
            node[parts[index]] = directory
        }
        put(&root, 0)
    }
    for (path, data) in files.sorted(by: { $0.key < $1.key }) {
        if unpacked.contains(path) {
            insert(path, ["size": data.count, "unpacked": true])
        } else {
            insert(path, ["size": data.count, "offset": String(body.count)])
            body.append(data)
        }
    }
    for (path, target) in links { insert(path, ["link": target]) }
    try writeRawAsar(to: url, index: try JSONSerialization.data(withJSONObject: ["files": root]), body: body)
}

/// An app.asar with exactly this JSON index and these bytes after the header.
func writeRawAsar(to url: URL, index json: Data, body: Data) throws {
    var payload = uint32(json.count) + json
    payload.append(Data(count: (4 - payload.count % 4) % 4))
    let headerPickle = uint32(payload.count) + payload
    try (uint32(4) + uint32(headerPickle.count) + headerPickle + body).write(to: url)
}

private func uint32(_ value: Int) -> Data { withUnsafeBytes(of: UInt32(value).littleEndian) { Data($0) } }

private func electronGame() throws -> URL {
    let root = FileManager.default.temporaryDirectory.appendingPathComponent("vndegen-asar-\(UUID().uuidString)", isDirectory: true)
    let resources = root.appendingPathComponent("resources", isDirectory: true)
    try FileManager.default.createDirectory(at: resources.appendingPathComponent("app.asar.unpacked/data/video"), withIntermediateDirectories: true)
    try writeAsar(to: resources.appendingPathComponent("app.asar"), files: [
        "index.html": Data("<html>asar</html>".utf8),
        "tyrano/plugins/kag/kag.js": Data("tyrano.plugin.kag={version:520,".utf8),
        "data/scenario/first.ks": Data("[cm]\nHello".utf8),
        "data/video/op.webm": Data("webm".utf8),
    ], links: ["data/first.ks": "data/scenario/first.ks", "data/escape.ks": "../../secret"], unpacked: ["data/video/op.webm"])
    try Data("unpacked webm".utf8).write(to: resources.appendingPathComponent("app.asar.unpacked/data/video/op.webm"))
    return root
}

private func text(_ response: FileResponse) async throws -> String {
    var body = Data()
    try await response.body?.stream { body.append($0); return true }
    return String(decoding: body, as: UTF8.self)
}

@Test func asarMembersAreServedFromInsideTheArchive() async throws {
    let root = try electronGame()
    defer { try? FileManager.default.removeItem(at: root) }
    let folder = GameFolder(root: root, webRoot: "resources/app.asar")
    #expect(folder.archive != nil)
    #expect(folder.mainModule == "/game/resources/app.asar/index.html")

    let index = folder.serve(path: "/")
    #expect(index.status == 200 && index.headers["Content-Type"] == "text/html")
    #expect(try await text(index) == "<html>asar</html>")
    #expect(try await text(folder.serve(path: "/data/scenario/first.ks")) == "[cm]\nHello")
    #expect(try await text(folder.serve(path: "/DATA/Scenario/FIRST.ks")) == "[cm]\nHello")
    #expect(try await text(folder.serve(path: "/data/first.ks")) == "[cm]\nHello")

    let part = folder.serve(path: "/data/scenario/first.ks", range: "bytes=5-")
    #expect(part.status == 206 && part.headers["Content-Range"] == "bytes 5-9/10")
    #expect(try await text(part) == "Hello")

    #expect(try await text(folder.serve(path: "/data/video/op.webm")) == "unpacked webm")
    #expect(folder.serve(path: "/data/escape.ks").status == 404)
    #expect(folder.serve(path: "/data/scenario").status == 404)
    #expect(folder.serve(path: "/missing.js").status == 404)
    #expect(folder.read(servedPath: "/tyrano/plugins/kag/kag.js", limit: 30) == Data("tyrano.plugin.kag={version:520".utf8))
}

@Test func aCorruptArchiveServesNothing() throws {
    let root = try electronGame()
    defer { try? FileManager.default.removeItem(at: root) }
    try Data("not an archive at all".utf8).write(to: root.appendingPathComponent("resources/app.asar"))
    let folder = GameFolder(root: root, webRoot: "resources/app.asar")
    #expect(folder.archive == nil)
    #expect(folder.serve(path: "/index.html").status == 404)
}

/// Opens an archive whose index has one file, `a.txt`, with this entry, over a 10-byte body.
private func archive(entry: String, body: Data = Data(count: 10)) throws -> AsarArchive? {
    let url = FileManager.default.temporaryDirectory.appendingPathComponent("vndegen-asar-\(UUID().uuidString).asar")
    defer { try? FileManager.default.removeItem(at: url) }
    try writeRawAsar(to: url, index: Data(#"{"files":{"a.txt":\#(entry)}}"#.utf8), body: body)
    return try? AsarArchive(reading: FileHandle(forReadingFrom: url))
}

@Test func packedSpansMustLieInsideTheArchive() throws {
    #expect(try archive(entry: #"{"size":10,"offset":"0"}"#)?.member("a.txt") != nil)
    #expect(try archive(entry: #"{"size":4,"offset":"6"}"#)?.member("a.txt") != nil)
    #expect(try archive(entry: #"{"size":0,"offset":"10"}"#)?.member("a.txt") != nil)
    for bad in [
        #"{"size":10,"offset":"18446744073709551615"}"#, #"{"size":18446744073709551615,"offset":"0"}"#,
        #"{"size":-1,"offset":"0"}"#, #"{"size":1e30,"offset":"0"}"#, #"{"size":2.5,"offset":"0"}"#, #"{"size":true,"offset":"0"}"#,
        #"{"size":11,"offset":"0"}"#, #"{"size":5,"offset":"6"}"#, #"{"size":1,"offset":"-1"}"#, #"{"size":1}"#,
        #"{"size":-1,"unpacked":true}"#,
    ] {
        #expect(try archive(entry: bad) == nil, "\(bad)")
    }
}

@Test func aBadArchiveIsNotServedAndDoesNotTrap() throws {
    let root = try electronGame()
    defer { try? FileManager.default.removeItem(at: root) }
    try writeRawAsar(to: root.appendingPathComponent("resources/app.asar"),
                     index: Data(#"{"files":{"index.html":{"size":4,"offset":"18446744073709551615"}}}"#.utf8), body: Data("html".utf8))
    let folder = GameFolder(root: root, webRoot: "resources/app.asar")
    #expect(folder.archive == nil)
    #expect(folder.read(servedPath: "/index.html", limit: 4) == nil)
}

@Test func theIndexIsBoundedBeforeItIsRead() throws {
    let url = FileManager.default.temporaryDirectory.appendingPathComponent("vndegen-asar-\(UUID().uuidString).asar")
    defer { try? FileManager.default.removeItem(at: url) }
    func opens(_ bytes: Data) throws -> Bool {
        try bytes.write(to: url)
        return (try? AsarArchive(reading: FileHandle(forReadingFrom: url))) != nil
    }
    // A header that claims close to 4 GiB of JSON, in a file of a few bytes.
    #expect(try !opens(uint32(4) + uint32(0xFFFF_FFF8) + uint32(0xFFFF_FFF4) + uint32(0xFFFF_FFF0) + Data("{}".utf8)))
    // A header larger than the file, with a small JSON size.
    #expect(try !opens(uint32(4) + uint32(4096) + uint32(4092) + uint32(13) + Data(#"{"files":{}}"#.utf8)))
    // A JSON size over the cap, even in a file that holds that much.
    let big = AsarArchive.maxIndexBytes + 4
    #expect(try !opens(uint32(4) + uint32(big + 8) + uint32(big + 4) + uint32(big) + Data(count: big)))

    func nested(_ depth: Int) -> Data {
        Data((String(repeating: #"{"files":{"d":"#, count: depth) + #"{"files":{}}"# + String(repeating: "}}", count: depth)).utf8)
    }
    try writeRawAsar(to: url, index: nested(AsarArchive.maxDepth - 1), body: Data())
    #expect((try? AsarArchive(reading: FileHandle(forReadingFrom: url))) != nil)
    try writeRawAsar(to: url, index: nested(5000), body: Data())
    #expect((try? AsarArchive(reading: FileHandle(forReadingFrom: url))) == nil)
    #expect(AsarArchive.nesting(of: Data(#"{"a":"}{[\"","b":[[1]]}"#.utf8)) == 3)
}
