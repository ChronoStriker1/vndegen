import Foundation
import Testing
@testable import ArchiveImport

private let fixtures = URL(fileURLWithPath: #filePath).deletingLastPathComponent().appendingPathComponent("Fixtures")
private var fm: FileManager { .default }

private func temporaryDirectory() throws -> URL {
    let url = fm.temporaryDirectory.appendingPathComponent("archiveimport-\(UUID().uuidString)", isDirectory: true)
    try fm.createDirectory(at: url, withIntermediateDirectories: true)
    return url
}

/// The text make-fixtures.sh puts in its larger files.
private let lines = (1...20000).map { "line \($0)\n" }.joined()

private func text(_ url: URL) throws -> String { String(decoding: try Data(contentsOf: url), as: UTF8.self) }

private func items(_ folder: URL) -> [String] { ((try? fm.contentsOfDirectory(atPath: folder.path)) ?? []).sorted() }

@MainActor
private func waitWhileExtracting(_ importer: ArchiveImporter) async throws {
    let deadline = ContinuousClock.now + .seconds(30)
    while importer.step == .extracting || importer.step == .opening {
        try #require(ContinuousClock.now < deadline, "import finished in time")
        try await Task.sleep(for: .milliseconds(10))
    }
}

/// Picks `archive` and imports it under its suggested name.
@MainActor
private func importArchive(_ urls: [URL], into games: URL, password: String? = nil) async throws -> ArchiveImporter {
    let importer = ArchiveImporter(gamesRoot: games)
    await importer.choose(urls)
    try #require(importer.step == .reviewing, "archive opened: \(importer.errorMessage ?? "")")
    importer.startImport()
    if let password, importer.step == .password(wrong: false) { importer.submitPassword(password) }
    try await waitWhileExtracting(importer)
    return importer
}

// MARK: Names

@Test(arguments: [
    ("Test Game 43.7z", "Test Game 43"),
    ("Test Game 32.rar", "Test Game 32"),
    ("Oral Sex Brothel 2 v1.1.0.7z", "Oral Sex Brothel 2 v1.1.0"),
    ("Game v1.02.part03.rar", "Game v1.02"),
    ("Test Game 40.r27", "Test Game 40"),
    ("Big Game.7z.001", "Big Game"),
    ("SHOUTING.ZIP", "SHOUTING"),
    ("  padded .zip", "padded"),
])
func suggestsFolderNameFromArchiveName(fileName: String, expected: String) {
    #expect(VolumeName(fileName)?.stem == expected)
}

@Test func ignoresFilesThatAreNotArchives() {
    #expect(VolumeName("readme.txt") == nil)
    #expect(VolumeName("data.xp3") == nil)
    #expect(VolumeName(".zip") == nil)
}

@Test func gathersSiblingVolumesInPartOrder() throws {
    let folder = try temporaryDirectory()
    for name in ["Game.part2.rar", "Game.part10.rar", "Game.part1.rar", "Other.part1.rar"]
        + (3...9).map({ "Game.part\($0).rar" }) {
        try Data().write(to: folder.appendingPathComponent(name))
    }
    let source = try ArchiveSource(picked: [folder.appendingPathComponent("Game.part2.rar")])
    #expect(source.volumes.map(\.lastPathComponent) == (1...10).map { "Game.part\($0).rar" })
    #expect(source.suggestedFolderName == "Game")
}

@Test func readsOldStyleRarVolumesAfterTheRarFile() throws {
    let folder = try temporaryDirectory()
    for name in ["game.r01", "game.rar", "game.r00"] { try Data().write(to: folder.appendingPathComponent(name)) }
    let source = try ArchiveSource(picked: [folder.appendingPathComponent("game.r00")])
    #expect(source.volumes.map(\.lastPathComponent) == ["game.rar", "game.r00", "game.r01"])
}

@Test func reportsAMissingVolume() throws {
    let folder = try temporaryDirectory()
    let picked = ["Game.part1.rar", "Game.part3.rar"].map { folder.appendingPathComponent($0) }
    for url in picked { try Data().write(to: url) }
    #expect(throws: ArchiveError.missingVolume(2)) { try ArchiveSource(picked: picked) }
}

@Test func refusesTwoDifferentArchives() {
    #expect(throws: ArchiveError.severalArchives) {
        try ArchiveSource(picked: [URL(fileURLWithPath: "/tmp/a.zip"), URL(fileURLWithPath: "/tmp/b.zip")])
    }
}

@Test func refusesFolderNamesAlreadyInGames() throws {
    let games = try temporaryDirectory()
    try fm.createDirectory(at: games.appendingPathComponent("Test Game 43"), withIntermediateDirectories: true)
    #expect(GameFolderName.problem(with: "summer sisters", in: games)?.contains("already has") == true)
    #expect(GameFolderName.problem(with: "  ", in: games) != nil)
    #expect(GameFolderName.problem(with: "a/b", in: games) != nil)
    #expect(GameFolderName.problem(with: ".hidden", in: games) != nil)
    #expect(GameFolderName.problem(with: "Test Game 43 2", in: games) == nil)
}

// MARK: Name decoding

@Test func decodesCodePage932Names() {
    // "セーブ①.txt" as Japanese Windows stores it. ① needs code page 932, not plain Shift-JIS.
    let bytes: [UInt8] = [0x83, 0x5A, 0x81, 0x5B, 0x83, 0x75, 0x87, 0x40, 0x2E, 0x74, 0x78, 0x74]
    #expect(ArchiveReader.decodeName(bytes) == "セーブ①.txt")
    #expect(ArchiveReader.decodeName(Array("ゲーム/www".utf8)) == "ゲーム/www")
}

@Test func dropsPathPartsThatEscapeTheFolder() {
    #expect(ArchiveReader.pathComponents("../../etc/passwd") == ["etc", "passwd"])
    #expect(ArchiveReader.pathComponents("Game\\www\\js/./rpg_core.js") == ["Game", "www", "js", "rpg_core.js"])
    #expect(ArchiveReader.pathComponents("/absolute/path") == ["absolute", "path"])
}

// MARK: Importing

@MainActor @Test func importsShiftJISZipWithDecodedNames() async throws {
    let games = try temporaryDirectory()
    let importer = try await importArchive([fixtures.appendingPathComponent("sjis.zip")], into: games)
    guard case let .finished(result) = importer.step else { Issue.record("not finished: \(importer.step)"); return }
    #expect(result.folderName == "sjis")
    let game = games.appendingPathComponent("sjis")
    // The archive's one top folder, ゲーム, became the game folder itself.
    #expect(items(game) == ["www", "セーブ①.txt"])
    #expect(try text(game.appendingPathComponent("www/js/rpg_core.js")) == "core")
}

@MainActor @Test func keepsArchivesWithSeveralTopLevelItemsAsIs() async throws {
    let games = try temporaryDirectory()
    let importer = try await importArchive([fixtures.appendingPathComponent("two-items.zip")], into: games)
    #expect(importer.step.isFinished)
    #expect(items(games.appendingPathComponent("two-items")) == ["a.txt", "folder"])
}

@MainActor @Test func extractsDeflate64Entries() async throws {
    let games = try temporaryDirectory()
    let importer = try await importArchive([fixtures.appendingPathComponent("deflate64.zip")], into: games)
    #expect(importer.step.isFinished, "\(importer.errorMessage ?? "")")
    #expect(try text(games.appendingPathComponent("deflate64/lines.txt")) == lines)
    #expect(try text(games.appendingPathComponent("deflate64/small.txt")) == "small\n")
}

@MainActor @Test func readsSplit7zFromAnyPickedPart() async throws {
    let games = try temporaryDirectory()
    let importer = try await importArchive([fixtures.appendingPathComponent("split.7z.003")], into: games)
    #expect(importer.step.isFinished, "\(importer.errorMessage ?? "")")
    #expect(try text(games.appendingPathComponent("split/lines.txt")) == lines)
}

@MainActor @Test func asksAgainAfterAWrongPassword() async throws {
    let games = try temporaryDirectory()
    let importer = ArchiveImporter(gamesRoot: games)
    await importer.choose([fixtures.appendingPathComponent("password.zip")])
    #expect(importer.contents?.isEncrypted == true)
    importer.startImport()
    #expect(importer.step == .password(wrong: false))

    importer.submitPassword("wrong")
    try await waitWhileExtracting(importer)
    #expect(importer.step == .password(wrong: true))
    #expect(items(games) == [".importing"] || items(games).isEmpty)

    importer.submitPassword("secret")
    try await waitWhileExtracting(importer)
    #expect(importer.step.isFinished)
    #expect(try text(games.appendingPathComponent("password/locked.txt")) == "locked\n")
}

@MainActor @Test func offersToExtractANestedArchive() async throws {
    let games = try temporaryDirectory()
    let importer = try await importArchive([fixtures.appendingPathComponent("nested.zip")], into: games)
    #expect(importer.step == .nested(fileName: "inner.7z"))
    importer.extractNested()
    try await waitWhileExtracting(importer)
    #expect(importer.step.isFinished)
    #expect(items(games.appendingPathComponent("nested")) == ["www"])
    #expect(items(games.appendingPathComponent(".importing")).isEmpty)
}

@MainActor @Test func keepsANestedArchiveWhenDeclined() async throws {
    let games = try temporaryDirectory()
    let importer = try await importArchive([fixtures.appendingPathComponent("nested.zip")], into: games)
    importer.keepNested()
    #expect(importer.step.isFinished)
    #expect(items(games.appendingPathComponent("nested")) == ["inner.7z", "readme.txt"])
}

// MARK: Cleanup

@MainActor @Test func cleansUpAfterAFailedImport() async throws {
    let games = try temporaryDirectory()
    // Damage the Deflate64 data in the middle of the zip, so extraction fails partway through.
    let damaged = try temporaryDirectory().appendingPathComponent("damaged.zip")
    var bytes = [UInt8](try Data(contentsOf: fixtures.appendingPathComponent("deflate64.zip")))
    for index in (bytes.count / 2)..<(bytes.count / 2 + 64) { bytes[index] ^= 0xFF }
    try Data(bytes).write(to: damaged)

    let importer = try await importArchive([damaged], into: games)
    #expect(importer.step == .reviewing)
    #expect(importer.errorMessage?.contains("could not be read") == true)
    #expect(items(games).filter { $0 != ".importing" }.isEmpty)
    #expect(items(games.appendingPathComponent(".importing")).isEmpty)
}

@MainActor @Test func cleansUpAfterACancelledImport() async throws {
    let games = try temporaryDirectory()
    let importer = ArchiveImporter(gamesRoot: games)
    await importer.choose([fixtures.appendingPathComponent("split.7z.001")])
    importer.startImport()
    #expect(importer.step == .extracting)
    importer.cancel()
    try await waitWhileExtracting(importer)
    #expect(importer.step == .reviewing)
    #expect(importer.errorMessage?.contains("cancelled") == true)
    #expect(items(games).filter { $0 != ".importing" }.isEmpty)
    #expect(items(games.appendingPathComponent(".importing")).isEmpty)
}

@MainActor @Test func removesStagingLeftFromACrashedImport() throws {
    let games = try temporaryDirectory()
    let leftover = games.appendingPathComponent(".importing/\(UUID().uuidString)/Half Game")
    try fm.createDirectory(at: leftover, withIntermediateDirectories: true)
    _ = ArchiveImporter(gamesRoot: games)
    #expect(items(games.appendingPathComponent(".importing")).isEmpty)
}

private extension ArchiveImporter.Step {
    var isFinished: Bool { if case .finished = self { true } else { false } }
}
