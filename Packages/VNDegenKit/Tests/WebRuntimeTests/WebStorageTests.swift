import Foundation
import Testing
@testable import WebRuntime

@Test func storageKeepsEveryKeyInsideItsDirectory() throws {
    let base = FileManager.default.temporaryDirectory.appendingPathComponent("vndegen-storage-\(UUID().uuidString)", isDirectory: true)
    defer { try? FileManager.default.removeItem(at: base) }
    let storage = WebStorage(directory: base.appendingPathComponent("localStorage"))
    #expect(try storage.load().isEmpty)

    let keys = ["testgame03_sf", "../../escape", "a/b", "セーブ", "", "with space.and.dots"]
    for (index, key) in keys.enumerated() { try storage.set(key, "value \(index)") }
    try storage.set("testgame03_sf", "%7B%22a%22%3A1%7D")
    let loaded = try storage.load()
    #expect(loaded.count == keys.count)
    #expect(loaded["testgame03_sf"] == "%7B%22a%22%3A1%7D")
    #expect(loaded["../../escape"] == "value 1")
    #expect(loaded["セーブ"] == "value 3")
    #expect(try FileManager.default.contentsOfDirectory(atPath: base.path) == ["localStorage"])

    try storage.remove("a/b")
    try storage.remove("never set")
    #expect(try storage.load()["a/b"] == nil)
    try storage.clear()
    #expect(try storage.load().isEmpty)
}

@Test func storageRequestsDecodeAndReply() throws {
    func decode(_ json: String) throws -> BridgeRequest { try JSONDecoder().decode(BridgeRequest.self, from: Data(json.utf8)) }
    #expect(try decode(#"{"op":"storageLoad"}"#) == .storageLoad)
    #expect(try decode(#"{"op":"storageSet","key":"k","value":"v"}"#) == .storageSet(key: "k", value: "v"))
    #expect(try decode(#"{"op":"storageRemove","key":"k"}"#) == .storageRemove(key: "k"))
    #expect(try decode(#"{"op":"storageClear"}"#) == .storageClear)
    #expect(BridgeReply.value(.dictionary(["k": "v"])).json == #"{"value":{"k":"v"}}"#)
}
