import Foundation
import Testing
import WebRuntime
@testable import EngineRPGMaker

private func temporaryGame() throws -> URL {
    let root = FileManager.default.temporaryDirectory.appendingPathComponent("vndegen-\(UUID().uuidString)", isDirectory: true)
    try FileManager.default.createDirectory(at: root.appendingPathComponent("js"), withIntermediateDirectories: true)
    try Data("<html></html>".utf8).write(to: root.appendingPathComponent("index.html"))
    return root
}

@Test func webRootFollowsTheEngineCore() throws {
    let root = try temporaryGame()
    defer { try? FileManager.default.removeItem(at: root) }
    try Data().write(to: root.appendingPathComponent("js/rmmz_core.js"))
    #expect(GameFolder(rpgMakerGameAt: root).webRoot == "")

    let mv = try temporaryGame()
    defer { try? FileManager.default.removeItem(at: mv) }
    try FileManager.default.createDirectory(at: mv.appendingPathComponent("WWW/JS"), withIntermediateDirectories: true)
    try Data("mv".utf8).write(to: mv.appendingPathComponent("WWW/index.html"))
    try Data().write(to: mv.appendingPathComponent("WWW/JS/rpg_core.js"))
    let folder = GameFolder(rpgMakerGameAt: mv)
    #expect(folder.webRoot == "www")
    #expect(try folder.resolve(servedPath: "/").map { try Data(contentsOf: $0) } == Data("mv".utf8))
}

@Test func savesGoBesideIndexHtmlWhateverTheirCase() throws {
    let root = try temporaryGame()
    defer { try? FileManager.default.removeItem(at: root) }
    try FileManager.default.createDirectory(at: root.appendingPathComponent("Save"), withIntermediateDirectories: true)
    try Data().write(to: root.appendingPathComponent("js/rmmz_core.js"))
    try Data().write(to: root.appendingPathComponent("Save/file1.rmmzsave"))
    let saves = try #require(GameFolder(rpgMakerGameAt: root).saveDirectory)
    #expect(try FileManager.default.contentsOfDirectory(atPath: saves.path) == ["file1.rmmzsave"])
    #expect(GameFolder(root: root, webRoot: "www").saveDirectory?.path == root.appendingPathComponent("www/save").path)
}
