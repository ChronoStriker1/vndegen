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

/// The data bytes of every file under `folder`.
private func bytesWritten(in folder: URL) -> Int {
    (fm.enumerator(at: folder, includingPropertiesForKeys: [.fileSizeKey])?.allObjects as? [URL] ?? [])
        .reduce(0) { $0 + ((try? $1.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0) }
}

/// A zip whose zip64 fields list each entry at the given size, though each holds only an empty Deflate stream.
private func zip64(listing sizes: [UInt64]) -> Data {
    func le<T: FixedWidthInteger>(_ value: T, _ data: inout Data) { withUnsafeBytes(of: value.littleEndian) { data.append(contentsOf: $0) } }
    var local = Data(), central = Data()
    for (index, size) in sizes.enumerated() {
        let name = Data("file\(index)".utf8)
        var extra = Data()
        le(UInt16(1), &extra); le(UInt16(16), &extra); le(size, &extra); le(UInt64(2), &extra)
        let offset = UInt32(local.count)
        le(UInt32(0x0403_4b50), &local); le(UInt16(45), &local); le(UInt16(0), &local); le(UInt16(8), &local)
        le(UInt16(0), &local); le(UInt16(0x21), &local); le(UInt32(0), &local); le(UInt32.max, &local); le(UInt32.max, &local)
        le(UInt16(name.count), &local); le(UInt16(extra.count), &local)
        local += name + extra + [0x03, 0x00]
        le(UInt32(0x0201_4b50), &central); le(UInt16(45), &central); le(UInt16(45), &central); le(UInt16(0), &central)
        le(UInt16(8), &central); le(UInt16(0), &central); le(UInt16(0x21), &central); le(UInt32(0), &central)
        le(UInt32.max, &central); le(UInt32.max, &central); le(UInt16(name.count), &central); le(UInt16(extra.count), &central)
        le(UInt16(0), &central); le(UInt16(0), &central); le(UInt16(0), &central); le(UInt32(0), &central); le(offset, &central)
        central += name + extra
    }
    var end = Data()
    le(UInt32(0x0605_4b50), &end); le(UInt16(0), &end); le(UInt16(0), &end); le(UInt16(sizes.count), &end)
    le(UInt16(sizes.count), &end); le(UInt32(central.count), &end); le(UInt32(local.count), &end); le(UInt16(0), &end)
    return local + central + end
}

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
    ("Test Game 01.7z", "Test Game 01"),
    ("Test Game 02 [ENG Ver.].rar", "Test Game 02 [ENG Ver.]"),
    ("Test Game 03 v1.1.0.7z", "Test Game 03 v1.1.0"),
    ("Game v1.02.part03.rar", "Game v1.02"),
    ("tg-testgame04.r27", "tg-testgame04"),
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

@Test func refusesPartNumbersTooLongForInt() {
    #expect(VolumeName("Game.part99999999999999999999.rar") == nil)
    #expect(throws: ArchiveError.notAnArchive) {
        try ArchiveSource(picked: [URL(fileURLWithPath: "/tmp/Game.part99999999999999999999.rar")])
    }
}

@Test func keepsSplitZipAndSplit7zApart() throws {
    let folder = try temporaryDirectory()
    for name in ["Game.zip.001", "Game.zip.002", "Game.7z.001"] { try Data().write(to: folder.appendingPathComponent(name)) }
    let source = try ArchiveSource(picked: [folder.appendingPathComponent("Game.zip.002")])
    #expect(source.volumes.map(\.lastPathComponent) == ["Game.zip.001", "Game.zip.002"])
    #expect(throws: ArchiveError.severalArchives) {
        try ArchiveSource(picked: ["Game.zip.001", "Game.7z.001"].map { folder.appendingPathComponent($0) })
    }
    #expect(LocalArchives.scan([folder]).map(\.fileName) == ["Game.7z.001", "Game.zip.001"])
}

@Test func refusesTwoFilesForOnePart() throws {
    let folder = try temporaryDirectory()
    for name in ["Game.part1.rar", "Game.part01.rar", "Game.part2.rar"] { try Data().write(to: folder.appendingPathComponent(name)) }
    #expect(throws: ArchiveError.duplicateVolume(1)) { try ArchiveSource(picked: [folder.appendingPathComponent("Game.part2.rar")]) }
    #expect(throws: ArchiveError.duplicateVolume(1)) {
        try ArchiveSource(picked: ["Game.part1.rar", "Game.part01.rar"].map { folder.appendingPathComponent($0) })
    }
}

@Test func refusesTwoDifferentArchives() {
    #expect(throws: ArchiveError.severalArchives) {
        try ArchiveSource(picked: [URL(fileURLWithPath: "/tmp/a.zip"), URL(fileURLWithPath: "/tmp/b.zip")])
    }
}

@Test func refusesFolderNamesAlreadyInGames() throws {
    let games = try temporaryDirectory()
    try fm.createDirectory(at: games.appendingPathComponent("Test Game 01"), withIntermediateDirectories: true)
    #expect(GameFolderName.problem(with: "test game 01", in: games)?.contains("already has") == true)
    #expect(GameFolderName.problem(with: "  ", in: games) != nil)
    #expect(GameFolderName.problem(with: "a/b", in: games) != nil)
    #expect(GameFolderName.problem(with: ".hidden", in: games) != nil)
    #expect(GameFolderName.problem(with: "Test Game 01 2", in: games) == nil)
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

// MARK: Limits

@Test(arguments: [
    [UInt64(1) << 62, UInt64(1) << 62],
    [UInt64(1) << 51],
])
func refusesImpossibleListedSizes(sizes: [UInt64]) throws {
    let zip = try temporaryDirectory().appendingPathComponent("bomb.zip")
    try zip64(listing: sizes).write(to: zip)
    #expect(throws: ArchiveError.impossibleSize) { try ArchiveExtractor.list(ArchiveSource(picked: [zip])) }
}

@Test func stopsOnceExtractionWritesMoreThanListed() throws {
    let source = try ArchiveSource(picked: [fixtures.appendingPathComponent("split.7z.001")])
    // Stands in for an archive that lists less than it holds, or that changed between listing and extraction.
    var listed = try ArchiveExtractor.list(source)
    listed.spaceLimit = 10_000
    let staging = try temporaryDirectory()
    #expect(throws: ArchiveError.largerThanListed) {
        try ArchiveExtractor.extract(source, password: nil, into: staging, total: listed) { _ in }
    }
    #expect(bytesWritten(in: staging) <= 10_000)
}

@Test func countsEveryEntryAgainstTheListedSpace() throws {
    let source = try ArchiveSource(picked: [fixtures.appendingPathComponent("two-items.zip")])
    var listed = try ArchiveExtractor.list(source)
    #expect(throws: Never.self) { try ArchiveExtractor.extract(source, password: nil, into: temporaryDirectory(), total: listed) { _ in } }
    // Room for the files' data alone is not enough, so an archive of many empty files can't fill the device either.
    listed.spaceLimit = listed.byteCount
    #expect(throws: ArchiveError.largerThanListed) {
        try ArchiveExtractor.extract(source, password: nil, into: temporaryDirectory(), total: listed) { _ in }
    }
}

@Test func stopsWhenFreeSpaceFallsToTheReserve() throws {
    let source = try ArchiveSource(picked: [fixtures.appendingPathComponent("split.7z.001")])
    let staging = try temporaryDirectory()
    // No volume has this much free, so the first check finds the reserve gone.
    #expect(throws: ArchiveError.ranOutOfSpace) {
        try ArchiveExtractor.extract(source, password: nil, into: staging, total: ArchiveExtractor.list(source), reserve: .max) { _ in }
    }
    #expect(items(staging).isEmpty)
}

@Test func stopsDeflate64OutputAtTheListedSize() throws {
    var bytes = [UInt8](try Data(contentsOf: fixtures.appendingPathComponent("deflate64.zip")))
    let name = Array("Deflate Game/lines.txt".utf8)
    let record = try #require((0..<(bytes.count - 46 - name.count)).first { at in
        bytes[at..<(at + 4)] == [0x50, 0x4b, 0x01, 0x02] && Array(bytes[(at + 46)..<(at + 46 + name.count)]) == name
    })
    // The central directory now lists lines.txt at 1,000 bytes.
    bytes.replaceSubrange((record + 24)..<(record + 28), with: [0xE8, 0x03, 0x00, 0x00])
    let zip = try temporaryDirectory().appendingPathComponent("lying.zip")
    try Data(bytes).write(to: zip)

    let entries = try Deflate64Entries(zip: zip)
    let location = try #require(entries.entries[name])
    var written = 0
    #expect(throws: ArchiveError.largerThanListed) { try entries.inflate(location) { written += $0.count } }
    #expect(written <= 1000)
}

// MARK: Deleting the archive

@MainActor @Test func offersToDeleteOnlyArchivesInVNDegensOwnFolder() async throws {
    let games = try temporaryDirectory()
    let outside = ArchiveImporter(gamesRoot: games)
    // The fixtures are on a local disk outside Games, like a Dropbox or OneDrive folder would be.
    await outside.choose([fixtures.appendingPathComponent("two-items.zip")])
    #expect(!outside.canDeleteArchive)

    let archive = games.appendingPathComponent("two-items.zip")
    try fm.copyItem(at: fixtures.appendingPathComponent("two-items.zip"), to: archive)
    let inside = ArchiveImporter(gamesRoot: games)
    await inside.choose([archive])
    #expect(inside.canDeleteArchive)
    inside.deleteArchiveAfterImport = true
    inside.startImport()
    try await waitWhileExtracting(inside)
    #expect(inside.step.isFinished)
    #expect(inside.step.keptArchives == [])
    #expect(items(games) == [".importing", "two-items"])
}

@MainActor @Test func keepsAnArchiveReplacedBeforeItsDeletion() async throws {
    let games = try temporaryDirectory()
    let archive = games.appendingPathComponent("nested.zip")
    try fm.copyItem(at: fixtures.appendingPathComponent("nested.zip"), to: archive)
    let importer = try await importArchive([archive], into: games)
    #expect(importer.deleteArchiveAfterImport)
    #expect(importer.step == .nested(fileName: "inner.7z"))

    // The user copies another archive over it in Files while the nested prompt shows.
    try fm.removeItem(at: archive)
    try fm.copyItem(at: fixtures.appendingPathComponent("two-items.zip"), to: archive)
    importer.keepNested()
    #expect(importer.step.isFinished)
    #expect(importer.step.keptArchives == ["nested.zip"])
    #expect(try Data(contentsOf: archive) == Data(contentsOf: fixtures.appendingPathComponent("two-items.zip")))
}

@MainActor @Test func defaultsToDeletingOnlyArchivesListedForImport() async throws {
    let games = try temporaryDirectory()
    let listed = games.appendingPathComponent("two-items.zip")
    try fm.copyItem(at: fixtures.appendingPathComponent("two-items.zip"), to: listed)
    let nested = games.appendingPathComponent("Some Game/two-items.zip")
    try fm.createDirectory(at: nested.deletingLastPathComponent(), withIntermediateDirectories: true)
    try fm.copyItem(at: fixtures.appendingPathComponent("two-items.zip"), to: nested)

    let importer = ArchiveImporter(gamesRoot: games)
    await importer.choose([listed])
    #expect(importer.deleteArchiveAfterImport)
    await importer.choose([nested])
    #expect(importer.canDeleteArchive)
    #expect(!importer.deleteArchiveAfterImport, "an archive inside a game folder is not listed, so it stays unless the user asks")
}

// MARK: Ready to import

@Test func listsEachArchiveInVNDegensFolderOnce() throws {
    let folder = try temporaryDirectory()
    let files = ["Game A.zip", "Game B.7z", "Game C.part1.rar", "Game C.part2.rar", "Game D.rar", "Game D.r00", "Game D.r01",
                 "Game E.7z.001", "Game E.7z.002", "Orphan.part2.rar", "Orphan.r00", "readme.txt", ".hidden.zip"]
    for file in files { try Data(repeating: 1, count: 10).write(to: folder.appendingPathComponent(file)) }
    // Game folders are never archives, whatever their names.
    try fm.createDirectory(at: folder.appendingPathComponent("Game F.zip"), withIntermediateDirectories: true)
    try fm.createDirectory(at: folder.appendingPathComponent("Inbox"), withIntermediateDirectories: true)
    try Data().write(to: folder.appendingPathComponent("Inbox/Copy.zip"))

    let found = LocalArchives.scan([folder])
    #expect(found.map(\.fileName) == ["Game A.zip", "Game B.7z", "Game C.part1.rar", "Game D.rar", "Game E.7z.001"])
    #expect(found.map(\.parts) == [1, 1, 2, 3, 2])
    #expect(found.map(\.byteCount) == [10, 10, 20, 30, 20])
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

@MainActor @Test func cancelArrivingAsExtractionEndsAddsAndDeletesNothing() async throws {
    let games = try temporaryDirectory()
    let archive = games.appendingPathComponent("two-items.zip")
    try fm.copyItem(at: fixtures.appendingPathComponent("two-items.zip"), to: archive)
    let importer = ArchiveImporter(gamesRoot: games)
    await importer.choose([archive])
    importer.deleteArchiveAfterImport = true
    importer.startImport()
    // Once the staging folder exists, the import has handed extraction to the background.
    let deadline = Date.now.addingTimeInterval(10)
    while items(games.appendingPathComponent(".importing")).isEmpty {
        try #require(importer.step == .extracting, "import stopped before extracting: \(importer.errorMessage ?? "no error")")
        try #require(Date.now < deadline, "staging folder never appeared")
        await Task.yield()
    }
    // Holding the main actor lets extraction finish while its result waits here, then Cancel lands before it.
    usleep(1_000_000)
    importer.cancel()
    try await waitWhileExtracting(importer)
    #expect(importer.step == .reviewing)
    #expect(importer.errorMessage?.contains("cancelled") == true)
    #expect(items(games) == [".importing", "two-items.zip"])
    #expect(items(games.appendingPathComponent(".importing")).isEmpty)
}

@MainActor @Test func removesStagingLeftFromACrashedImport() throws {
    let games = try temporaryDirectory()
    let leftover = games.appendingPathComponent(".importing/\(UUID().uuidString)/Half Game")
    try fm.createDirectory(at: leftover, withIntermediateDirectories: true)
    _ = ArchiveImporter(gamesRoot: games)
    #expect(items(games.appendingPathComponent(".importing")).isEmpty)
}

// MARK: Controller

@MainActor @Test func controllerConfirmOpensTheKeyboardOnTextFields() async throws {
    let model = ArchiveImportModel(gamesRoot: try temporaryDirectory())
    await model.start(initialSelection: [fixtures.appendingPathComponent("password.zip")]) { _ in }
    model.handle(.up)
    model.handle(.up)
    #expect(model.isFocused(.folderName))
    model.handle(.confirm)
    #expect(model.editing == .folderName)

    model.handle(.down)
    model.handle(.right)
    model.handle(.confirm)
    #expect(model.importer?.step == .password(wrong: false))
    model.handle(.up)
    #expect(model.isFocused(.passwordField))
    model.handle(.confirm)
    #expect(model.editing == .passwordField)
}

// MARK: Drops

/// What Files hands a drop target: a provider holding a file representation, offered in place or only as a copy.
private func provider(for url: URL, inPlace: Bool) -> NSItemProvider {
    let provider = NSItemProvider()
    provider.registerFileRepresentation(forTypeIdentifier: "public.zip-archive", fileOptions: inPlace ? [.openInPlace] : [],
                                        visibility: .all) { completion in
        completion(url, inPlace, nil)
        return nil
    }
    return provider
}

@MainActor @Test func opensADropInPlace() async throws {
    let archive = try temporaryDirectory().appendingPathComponent("two-items.zip")
    try fm.copyItem(at: fixtures.appendingPathComponent("two-items.zip"), to: archive)
    let urls = await DroppedFiles.urls(from: [provider(for: archive, inPlace: true)])
    #expect(urls.map(\.standardizedFileURL) == [archive.standardizedFileURL])

    let importer = try await importArchive(urls, into: try temporaryDirectory())
    #expect(importer.step.isFinished)
    importer.close()
    #expect(fm.fileExists(atPath: archive.path), "the user's archive is left alone")
}

@MainActor @Test func keepsACopiedDropUntilTheImportCloses() async throws {
    let archive = try temporaryDirectory().appendingPathComponent("two-items.zip")
    try fm.copyItem(at: fixtures.appendingPathComponent("two-items.zip"), to: archive)
    let urls = await DroppedFiles.urls(from: [provider(for: archive, inPlace: false)])
    let copy = try #require(urls.first)
    #expect(urls.count == 1)
    #expect(copy.lastPathComponent == "two-items.zip")
    #expect(copy.standardizedFileURL != archive.standardizedFileURL)
    #expect(fm.fileExists(atPath: copy.path), "the copy outlives the provider's handler")

    let importer = ArchiveImporter(gamesRoot: try temporaryDirectory())
    await importer.choose(urls)
    #expect(importer.step == .reviewing)
    #expect(!importer.canDeleteArchive, "a copy never offers to delete the original")
    importer.startImport()
    try await waitWhileExtracting(importer)
    #expect(importer.step.isFinished)
    importer.close()
    #expect(!fm.fileExists(atPath: copy.deletingLastPathComponent().path), "the copy is deleted after import")
    #expect(fm.fileExists(atPath: archive.path))
}

// MARK: File coordination

/// Stands in for a file provider such as iCloud Drive. Counts the coordinated reads of its file, and while holding, keeps
/// readers waiting the way a provider does while it downloads.
private final class FakeProvider: NSObject, NSFilePresenter, @unchecked Sendable {
    let presentedItemURL: URL?
    let presentedItemOperationQueue = OperationQueue()
    private let lock = NSLock()
    private var readCount = 0
    private var holding = false
    private var waiting: [@Sendable () -> Void] = []

    init(_ url: URL) {
        presentedItemURL = url
        super.init()
        NSFileCoordinator.addFilePresenter(self)
    }

    var reads: Int { lock.withLock { readCount } }
    var hasWaitingReader: Bool { lock.withLock { !waiting.isEmpty } }

    func hold() { lock.withLock { holding = true } }

    func letReadersIn() {
        let readers = lock.withLock {
            holding = false
            defer { waiting = [] }
            return waiting
        }
        readers.forEach { $0() }
    }

    func remove() { NSFileCoordinator.removeFilePresenter(self) }

    func relinquishPresentedItem(toReader reader: @escaping @Sendable ((@Sendable () -> Void)?) -> Void) {
        let proceed = lock.withLock {
            readCount += 1
            if holding { waiting.append { reader(nil) } }
            return !holding
        }
        if proceed { reader(nil) }
    }
}

@MainActor
private func waitUntil(_ condition: () -> Bool, _ comment: Comment) async throws {
    let deadline = ContinuousClock.now + .seconds(10)
    while !condition() {
        try #require(ContinuousClock.now < deadline, comment)
        try await Task.sleep(for: .milliseconds(10))
    }
}

@MainActor @Test func readsADropInPlaceUnderFileCoordination() async throws {
    let archive = try temporaryDirectory().appendingPathComponent("two-items.zip")
    try fm.copyItem(at: fixtures.appendingPathComponent("two-items.zip"), to: archive)
    let presenter = FakeProvider(archive)
    defer { presenter.remove() }
    let urls = await DroppedFiles.urls(from: [provider(for: archive, inPlace: true)])

    let importer = ArchiveImporter(gamesRoot: try temporaryDirectory())
    let beforeListing = presenter.reads
    await importer.choose(urls)
    let afterListing = presenter.reads
    #expect(afterListing > beforeListing, "listing is coordinated")
    importer.startImport()
    try await waitWhileExtracting(importer)
    #expect(importer.step.isFinished)
    #expect(presenter.reads > afterListing, "extraction is coordinated")
}

// MARK: Overlapping choices

@MainActor @Test func aNewChoiceWhileOneIsListingEndsOnTheNewOne() async throws {
    let first = try temporaryDirectory().appendingPathComponent("split.7z.001")
    try fm.copyItem(at: fixtures.appendingPathComponent("split.7z.001"), to: first)
    // A dropped copy, which the importer deletes when it is done with it.
    let copied = await DroppedFiles.urls(from: [provider(for: first, inPlace: false)])
    let copy = try #require(copied.first)
    let presenter = FakeProvider(copy)
    defer { presenter.remove() }
    presenter.hold()

    let importer = ArchiveImporter(gamesRoot: try temporaryDirectory())
    let listingFirst = Task { await importer.choose(copied) }
    try await waitUntil({ presenter.hasWaitingReader }, "the first choice is listing")
    await importer.choose([fixtures.appendingPathComponent("two-items.zip")])
    #expect(importer.step == .reviewing)
    #expect(fm.fileExists(atPath: copy.path), "the first choice's copy stays while it is read")

    presenter.letReadersIn()
    await listingFirst.value
    #expect(importer.step == .reviewing)
    #expect(importer.source?.fileName == "two-items.zip")
    #expect(importer.errorMessage == nil)
    #expect(!fm.fileExists(atPath: copy.deletingLastPathComponent().path), "the first choice's copy is deleted after its read")
}

@MainActor @Test func aNewChoiceWhileExtractingKeepsTheFilesUntilExtractionStops() async throws {
    let copied = await DroppedFiles.urls(from: [provider(for: fixtures.appendingPathComponent("two-items.zip"), inPlace: false)])
    let copy = try #require(copied.first)
    let presenter = FakeProvider(copy)
    defer { presenter.remove() }
    let importer = ArchiveImporter(gamesRoot: try temporaryDirectory())
    await importer.choose(copied)
    presenter.hold()
    importer.startImport()
    try await waitUntil({ presenter.hasWaitingReader }, "extraction started reading")

    await importer.choose([fixtures.appendingPathComponent("password.zip")])
    #expect(fm.fileExists(atPath: copy.path), "the extraction's copy stays while it is read")
    presenter.letReadersIn()
    try await waitUntil({ !fm.fileExists(atPath: copy.deletingLastPathComponent().path) }, "the copy is deleted once extraction stops")
    #expect(importer.step == .reviewing)
    #expect(importer.source?.fileName == "password.zip")
    #expect(importer.errorMessage == nil, "the cancelled extraction leaves the new choice alone")
}

// MARK: Deliveries nobody imports

@MainActor @Test func inboxCleanupKeepsCopiesBeingReadAndCopiesThatJustArrived() async throws {
    let inbox = try temporaryDirectory()
    let reading = inbox.appendingPathComponent("reading.zip")
    let abandoned = inbox.appendingPathComponent("abandoned.zip")
    for url in [reading, abandoned] { try fm.copyItem(at: fixtures.appendingPathComponent("two-items.zip"), to: url) }
    let presenter = FakeProvider(reading)
    defer { presenter.remove() }
    presenter.hold()
    let importer = ArchiveImporter(gamesRoot: try temporaryDirectory())
    let listing = Task { await importer.choose([reading]) }
    try await waitUntil({ presenter.hasWaitingReader }, "the choice is listing")
    // Its sheet closed while the listing still reads.
    importer.close()

    ArchiveImporter.discardCopies(in: inbox, addedBefore: .distantPast)
    #expect(items(inbox) == ["abandoned.zip", "reading.zip"], "copies added after the cutoff stay")
    ArchiveImporter.discardCopies(in: inbox, addedBefore: .now.addingTimeInterval(60))
    #expect(items(inbox) == ["reading.zip"], "a copy a closed choice still reads stays")

    presenter.letReadersIn()
    await listing.value
    ArchiveImporter.discardCopies(in: inbox, addedBefore: .now.addingTimeInterval(60))
    #expect(items(inbox).isEmpty, "the copy goes once nothing reads it")
}

@MainActor @Test func inboxCleanupKeepsSiblingVolumesOfAnOpenChoice() async throws {
    let inbox = try temporaryDirectory()
    for part in 1...4 {
        try fm.copyItem(at: fixtures.appendingPathComponent("split.7z.00\(part)"), to: inbox.appendingPathComponent("split.7z.00\(part)"))
    }
    let importer = ArchiveImporter(gamesRoot: try temporaryDirectory())
    // Only the first part is picked; the importer finds the other three beside it.
    await importer.choose([inbox.appendingPathComponent("split.7z.001")])
    #expect(importer.step == .reviewing)

    ArchiveImporter.discardCopies(in: inbox, addedBefore: .now.addingTimeInterval(60))
    #expect(items(inbox).count == 4, "every volume of the open choice stays")
    importer.close()
}

@MainActor @Test func importFlowDiscardsDeliveriesItCannotTake() async throws {
    let archive = try temporaryDirectory().appendingPathComponent("two-items.zip")
    try fm.copyItem(at: fixtures.appendingPathComponent("two-items.zip"), to: archive)
    let presenter = FakeProvider(archive)
    defer { presenter.remove() }
    let model = ArchiveImportModel(gamesRoot: try temporaryDirectory())
    await model.start(initialSelection: [archive]) { _ in }
    let importer = try #require(model.importer)
    presenter.hold()
    importer.startImport()
    try await waitUntil({ presenter.hasWaitingReader }, "extraction started")

    let duringExtraction = await DroppedFiles.urls(from: [provider(for: fixtures.appendingPathComponent("two-items.zip"), inPlace: false)])
    model.choose(duringExtraction)
    try await waitUntil({ !fm.fileExists(atPath: duringExtraction[0].deletingLastPathComponent().path) },
                       "a drop during extraction is discarded")
    #expect(importer.step == .extracting)

    presenter.letReadersIn()
    try await waitWhileExtracting(importer)
    model.close()
    let afterClose = await DroppedFiles.urls(from: [provider(for: fixtures.appendingPathComponent("two-items.zip"), inPlace: false)])
    model.choose(afterClose)
    try await waitUntil({ !fm.fileExists(atPath: afterClose[0].deletingLastPathComponent().path) },
                       "a drop after the flow ended is discarded")
    #expect(importer.source == nil, "the closed importer stays closed")
    #expect(fm.fileExists(atPath: archive.path))
}

private extension ArchiveImporter.Step {
    var isFinished: Bool { if case .finished = self { true } else { false } }
    var keptArchives: [String]? { if case let .finished(result) = self { result.keptArchives } else { nil } }
}

/// Windows takes `Img/`, `img/`, and `IMG/` in one archive for one folder, so they must stay one folder on an iPhone's
/// disk too. `VNDEGEN_TEST_DIR` puts the extraction on a case-sensitive volume, which `scripts/build.sh test` mounts.
@Test func joinsFoldersThatDifferOnlyInCase() throws {
    let scratch = ProcessInfo.processInfo.environment["VNDEGEN_TEST_DIR"].map { URL(fileURLWithPath: $0, isDirectory: true) }
        ?? fm.temporaryDirectory
    let staging = scratch.appendingPathComponent("archiveimport-\(UUID().uuidString)", isDirectory: true)
    try fm.createDirectory(at: staging, withIntermediateDirectories: true)
    defer { try? fm.removeItem(at: staging) }
    let source = try ArchiveSource(picked: [fixtures.appendingPathComponent("mixed-case.zip")])
    try ArchiveExtractor.extract(source, password: nil, into: staging, total: try ArchiveExtractor.list(source)) { _ in }
    #expect(items(staging.appendingPathComponent("Mixed")) == ["Img"])
    #expect(items(staging.appendingPathComponent("Mixed/Img")) == ["Sub", "a.png", "b.png"])
    #expect(try text(staging.appendingPathComponent("Mixed/Img/Sub/c.png")) == "c")
}
