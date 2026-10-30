import Foundation
import Testing
import VNCore

/// `VNDEGEN_TEST_DIR` is a folder on a case-sensitive volume, which `scripts/build.sh test` mounts as iPhones and iPads
/// have. Without it these tests run on the temporary folder and prove less, since a Mac's usual disk ignores case.
private let scratch = ProcessInfo.processInfo.environment["VNDEGEN_TEST_DIR"].map { URL(fileURLWithPath: $0, isDirectory: true) }
    ?? FileManager.default.temporaryDirectory

private func temporaryFolder(_ files: [String]) throws -> URL {
    let root = scratch.appendingPathComponent("filecase-\(UUID().uuidString)", isDirectory: true)
    for file in files {
        let url = root.appendingPathComponent(file)
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data(file.utf8).write(to: url)
    }
    return root
}

@Test(.enabled(if: ProcessInfo.processInfo.environment["VNDEGEN_TEST_DIR"] != nil))
func theTestVolumeTellsCaseApart() throws {
    let root = try temporaryFolder(["a.txt"])
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(!FileManager.default.fileExists(atPath: root.appendingPathComponent("A.TXT").path))
}

@Test func namesInAnotherCaseResolveToTheFileOnDisk() throws {
    let root = try temporaryFolder(["Img/Pictures/Title.png", "audio/battle1.OGG", "save/.keep"])
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(FileCase.resolve("img/pictures/TITLE.png", in: root).path == root.path + "/Img/Pictures/Title.png")
    #expect(FileCase.resolve("Audio/Battle1.ogg", in: root).path == root.path + "/audio/battle1.OGG")
    #expect(FileCase.spelling(of: "AUDIO/BATTLE1.ogg", in: root) == "audio/battle1.OGG")
    #expect(FileCase.entry("IMG", in: root.path) == "Img")
    #expect(FileCase.entry("missing", in: root.path) == nil)
    // A missing name keeps the game's spelling, inside the folders as they are on disk.
    #expect(FileCase.resolve("SAVE/Save01.rvdata2", in: root).path == root.path + "/save/Save01.rvdata2")
    #expect(FileCase.resolve("IMG/New/Title.png", in: root).path == root.path + "/Img/New/Title.png")
}

@Test func anExactNameWinsOverOneThatDiffersOnlyInCase() throws {
    let root = try temporaryFolder(["data/Map001.rvdata2"])
    defer { try? FileManager.default.removeItem(at: root) }
    let lower = root.appendingPathComponent("data/map001.rvdata2")
    guard (try? Data(contentsOf: lower)) == nil else { return } // a volume that ignores case can't hold both
    try Data("lower".utf8).write(to: lower)
    #expect(FileCase.resolve("data/map001.rvdata2", in: root).lastPathComponent == "map001.rvdata2")
    #expect(FileCase.resolve("data/Map001.rvdata2", in: root).lastPathComponent == "Map001.rvdata2")
    // Neither matches exactly, so the first in sorted order wins, as in casepath.c.
    #expect(FileCase.resolve("data/MAP001.rvdata2", in: root).lastPathComponent == "Map001.rvdata2")
}

@Test func aFileMadeAfterItsFolderWasListedIsFound() throws {
    let root = try temporaryFolder(["save/Save01.rvdata2"])
    defer { try? FileManager.default.removeItem(at: root) }
    #expect(FileCase.resolve("save/SAVE01.rvdata2", in: root).lastPathComponent == "Save01.rvdata2")
    try Data().write(to: root.appendingPathComponent("save/Save02.rvdata2"))
    #expect(FileCase.resolve("save/save02.RVDATA2", in: root).lastPathComponent == "Save02.rvdata2")
}
