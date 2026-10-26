import Foundation
import Rclone
import Testing

private struct Version: Decodable { var goVersion: String }
private struct Listing: Decodable { var list: [Item] }
private struct Item: Decodable { var Name: String; var Size: Int64; var IsDir: Bool }
private struct Replaced: Decodable { var replaced: Bool }

private func folder() throws -> URL {
    let url = FileManager.default.temporaryDirectory.appendingPathComponent("RcloneTests-\(UUID().uuidString)", isDirectory: true)
    try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
    return url
}

@Test func answersAnRcCall() async throws {
    let version = try await Rclone.call("core/version", [String: String](), as: Version.self)
    #expect(version.goVersion.hasPrefix("go"))
}

@Test func listsAFolder() async throws {
    let root = try folder()
    try Data("abc".utf8).write(to: root.appendingPathComponent("a.txt"))
    let listing = try await Rclone.call("operations/list", ["fs": root.path, "remote": ""], as: Listing.self)
    #expect(listing.list.map(\.Name) == ["a.txt"])
    #expect(listing.list.first?.Size == 3)
}

@Test func replacesAFileOnlyWhileItHoldsTheExpectedBytes() async throws {
    let root = try folder()
    try Data("old".utf8).write(to: root.appendingPathComponent("mapping.yaml"))
    try Data("new".utf8).write(to: root.appendingPathComponent("next.tmp"))
    let stale = try await Rclone.call("vndegen/replacefile", ["fs": root.path, "srcRemote": "next.tmp", "dstRemote": "mapping.yaml",
                                                               "expected": Data("other".utf8).base64EncodedString()], as: Replaced.self)
    #expect(!stale.replaced)
    #expect(try String(contentsOf: root.appendingPathComponent("mapping.yaml"), encoding: .utf8) == "old")
    let fresh = try await Rclone.call("vndegen/replacefile", ["fs": root.path, "srcRemote": "next.tmp", "dstRemote": "mapping.yaml",
                                                               "expected": Data("old".utf8).base64EncodedString()], as: Replaced.self)
    #expect(fresh.replaced)
    #expect(try String(contentsOf: root.appendingPathComponent("mapping.yaml"), encoding: .utf8) == "new")
    #expect(!FileManager.default.fileExists(atPath: root.appendingPathComponent("next.tmp").path))
}

private struct Stat: Decodable { var item: Item? }

@Test func aMissingFileStatsAsNoneAndDeletesAsA404() async throws {
    let root = try folder()
    #expect(try await Rclone.call("operations/stat", ["fs": root.path, "remote": "nothing/here.txt"], as: Stat.self).item == nil)
    do {
        try await Rclone.call("operations/deletefile", ["fs": root.path, "remote": "nothing.txt"])
        Issue.record("deleted a file that is not there")
    } catch let error as RcloneError {
        #expect(error.status == 404)
    }
}

@Test func aCancelledCallNeverRenamesAndLaterCallsStillRun() async throws {
    let root = try folder()
    try Data("old".utf8).write(to: root.appendingPathComponent("mapping.yaml"))
    try Data("new".utf8).write(to: root.appendingPathComponent("next.tmp"))
    let input = ["fs": root.path, "srcRemote": "next.tmp", "dstRemote": "mapping.yaml", "expected": Data("old".utf8).base64EncodedString()]
    let cancelled = Task {
        withUnsafeCurrentTask { $0?.cancel() }
        return try await Rclone.call("vndegen/replacefile", input, as: Replaced.self)
    }
    await #expect(throws: CancellationError.self) { try await cancelled.value }
    #expect(try String(contentsOf: root.appendingPathComponent("mapping.yaml"), encoding: .utf8) == "old")
    #expect(try await Rclone.call("vndegen/replacefile", input, as: Replaced.self).replaced)
}
