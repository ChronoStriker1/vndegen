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

@Test func keysThatDifferOnlyInCaseKeepSeparateFiles() throws {
    let base = FileManager.default.temporaryDirectory.appendingPathComponent("vndegen-storage-\(UUID().uuidString)", isDirectory: true)
    defer { try? FileManager.default.removeItem(at: base) }
    let storage = WebStorage(directory: base)
    try storage.set("Save1", "upper")
    try storage.set("save1", "lower")
    try storage.set("SAVE1", "shout")
    #expect(try storage.load() == ["Save1": "upper", "save1": "lower", "SAVE1": "shout"])
    try storage.remove("save1")
    #expect(try storage.load() == ["Save1": "upper", "SAVE1": "shout"])
    for name in try FileManager.default.contentsOfDirectory(atPath: base.path) {
        #expect(name.replacingOccurrences(of: #"%[0-9A-F]{2}"#, with: "", options: .regularExpression) == name.lowercased()
            .replacingOccurrences(of: #"%[0-9a-f]{2}"#, with: "", options: .regularExpression), "\(name)")
    }
}

@Test func filesNamedTheOldWayMoveToTheirNewNames() throws {
    let fm = FileManager.default
    let base = fm.temporaryDirectory.appendingPathComponent("vndegen-storage-\(UUID().uuidString)", isDirectory: true)
    defer { try? fm.removeItem(at: base) }
    try fm.createDirectory(at: base, withIntermediateDirectories: true)
    let storage = WebStorage(directory: base)
    try Data("old".utf8).write(to: base.appendingPathComponent("TestGame01_sf.value"))
    try Data("kept".utf8).write(to: base.appendingPathComponent("lower_only.value"))
    // Restores that brought back a file under its old name beside a current one: the newer file wins.
    try storage.set("Config", "current")
    try Data("stale".utf8).write(to: base.appendingPathComponent("Config.value"))
    try fm.setAttributes([.modificationDate: Date(timeIntervalSinceNow: -3600)], ofItemAtPath: base.appendingPathComponent("Config.value").path)
    try storage.set("Other", "older")
    try fm.setAttributes([.modificationDate: Date(timeIntervalSinceNow: -3600)], ofItemAtPath: base.appendingPathComponent("%4Fther.value").path)
    try Data("restored".utf8).write(to: base.appendingPathComponent("Other.value"))

    #expect(try storage.load() == ["TestGame01_sf": "old", "lower_only": "kept", "Config": "current", "Other": "restored"])
    #expect(try Set(fm.contentsOfDirectory(atPath: base.path)) == ["%54est%47ame01_sf.value", "lower_only.value", "%43onfig.value", "%4Fther.value"])
    #expect(try storage.value(for: "TestGame01_sf") == "old")
    #expect(try storage.value(for: "missing") == nil)
}
