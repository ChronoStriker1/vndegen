import Foundation
import TestSupport
import Testing
@testable import ArchiveImport

private let fixtures = URL(fileURLWithPath: #filePath).deletingLastPathComponent().appendingPathComponent("Fixtures")
private var fm: FileManager { .default }

private func temporaryDirectory() throws -> URL {
    let url = TestFolders.root.appendingPathComponent("discimage-\(UUID().uuidString)", isDirectory: true)
    try fm.createDirectory(at: url, withIntermediateDirectories: true)
    return url
}

private func items(_ folder: URL) -> [String] { ((try? fm.contentsOfDirectory(atPath: folder.path)) ?? []).sorted() }

@Test func readsTheDataTrackOfACueSheet() {
    let sheet = CueSheet("FILE \"fas-raid.bin\" BINARY\r\n  TRACK 01 MODE1/2352\r\n    INDEX 01 00:00:00\r\n")
    #expect(sheet == CueSheet(binName: "fas-raid.bin", sectorSize: 2352, dataOffset: 16))
    #expect(CueSheet("FILE game.bin BINARY\nTRACK 01 MODE2/2352\nINDEX 01 00:00:00\n")?.dataOffset == 24)
    #expect(CueSheet("FILE \"C:\\\\Rips\\\\game.img\" BINARY\nTRACK 1 MODE1/2048\n")?.binName == "game.img")
}

@Test func refusesACueSheetWithOnlyAudio() {
    #expect(CueSheet("FILE \"music.bin\" BINARY\nTRACK 01 AUDIO\nINDEX 01 00:00:00\n") == nil)
}

@Test func takesTheBinBesideAPickedCue() throws {
    let source = try ArchiveSource(picked: [fixtures.appendingPathComponent("disc.cue")])
    #expect(source.volumes.map(\.lastPathComponent) == ["disc.cue", "disc.bin"])
    #expect(source.suggestedFolderName == "disc")
    let both = try ArchiveSource(picked: [fixtures.appendingPathComponent("disc.bin"), fixtures.appendingPathComponent("disc.cue")])
    #expect(both.volumes.map(\.lastPathComponent) == ["disc.cue", "disc.bin"])
}

@Test func namesTheMissingBin() throws {
    let folder = try temporaryDirectory()
    let cue = folder.appendingPathComponent("game.cue")
    try Data("FILE \"game.bin\" BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00\n".utf8).write(to: cue)
    #expect(throws: ArchiveError.missingDiscImage("game.bin")) { try ArchiveSource(picked: [cue]) }
}

@MainActor @Test func importsOnlyTheGameFolderOfACDImage() async throws {
    let games = try temporaryDirectory()
    let importer = ArchiveImporter(gamesRoot: games)
    await importer.choose([fixtures.appendingPathComponent("disc.cue")])
    try #require(importer.step == .reviewing, "image opened: \(importer.errorMessage ?? "")")
    importer.startImport()
    let deadline = ContinuousClock.now + .seconds(30)
    while importer.step == .extracting || importer.step == .opening {
        try #require(ContinuousClock.now < deadline, "import finished in time")
        try await Task.sleep(for: .milliseconds(10))
    }
    guard case let .finished(result) = importer.step else { Issue.record("not finished: \(importer.step)"); return }
    let game = games.appendingPathComponent(result.folderName)
    #expect(items(game) == ["GAME.EXE", "lines.txt"])
    let expected = (1...20000).map { "line \($0)\n" }.joined().prefix(20000)
    #expect(String(decoding: try Data(contentsOf: game.appendingPathComponent("lines.txt")), as: UTF8.self) == String(expected))
    #expect(String(decoding: try Data(contentsOf: game.appendingPathComponent("GAME.EXE")), as: UTF8.self) == "game\n")
}

@Test func keepsADiscWhoseGameIsAtItsRoot() throws {
    let disc = try temporaryDirectory()
    try Data().write(to: disc.appendingPathComponent("AUTORUN.INF"))
    try Data().write(to: disc.appendingPathComponent("Game.exe"))
    try fm.createDirectory(at: disc.appendingPathComponent("DirectX"), withIntermediateDirectories: true)
    try Data().write(to: disc.appendingPathComponent("DirectX/dxsetup.exe"))
    #expect(ArchiveExtractor.gameRoot(in: disc) == disc)
}
