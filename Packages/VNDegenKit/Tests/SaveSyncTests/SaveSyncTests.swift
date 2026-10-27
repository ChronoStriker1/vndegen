import CryptoKit
import EngineRenPy
import EngineRPGMaker
import Foundation
import Testing
import VNCore
@testable import SaveSync

let fixtures = URL(fileURLWithPath: #filePath).deletingLastPathComponent().appendingPathComponent("Fixtures")
let deckSave = "/home/deck/.local/share/Steam/steamapps/common/NoSocksRPG/save/"
var fm: FileManager { .default }

func temporaryDirectory() throws -> URL {
    let url = fm.temporaryDirectory.appendingPathComponent("savesync-\(UUID().uuidString)", isDirectory: true)
    try fm.createDirectory(at: url, withIntermediateDirectories: true)
    return url
}

/// An RPG Maker MZ game folder with the given saves.
func makeGame(in base: URL, saves: [String: String] = [:]) throws -> [SaveLocation] {
    let game = base.appendingPathComponent("Games/No Socks RPG", isDirectory: true)
    try fm.createDirectory(at: game.appendingPathComponent("js"), withIntermediateDirectories: true)
    try Data().write(to: game.appendingPathComponent("js/rmmz_core.js"))
    let locations = RPGMakerPlugin(.mz).saveLocations(folder: game, dataDirectory: base.appendingPathComponent("Data"))
    try write(saves, to: locations[0])
    return locations
}

func write(_ saves: [String: String], to location: SaveLocation) throws {
    try fm.createDirectory(at: location.directory, withIntermediateDirectories: true)
    for (name, content) in saves { try Data(content.utf8).write(to: location.directory.appendingPathComponent(name)) }
}

func contents(of location: SaveLocation) throws -> [String: String] {
    Dictionary(uniqueKeysWithValues: try LudusaviStore.regularFiles(in: location.directory).map {
        ($0, String(decoding: try Data(contentsOf: location.directory.appendingPathComponent($0)), as: UTF8.self))
    })
}

func sha1(_ text: String) -> String {
    Insecure.SHA1.hash(data: Data(text.utf8)).map { String(format: "%02x", $0) }.joined()
}

/// Every file under `root` with its SHA-1, to prove a backup left existing folders alone.
func tree(_ root: URL) throws -> [String: String] {
    Dictionary(uniqueKeysWithValues: try LudusaviStore.regularFiles(in: root).map {
        ($0, LudusaviStore.sha1(try Data(contentsOf: root.appendingPathComponent($0))))
    })
}

// MARK: - mapping.yaml

@Test func realMappingRoundTripsByteForByte() throws {
    let text = try String(contentsOf: fixtures.appendingPathComponent("nosocks-mapping.yaml"), encoding: .utf8)
    let mapping = try LudusaviMapping(yaml: text)
    #expect(mapping.name == "No Socks RPG")
    #expect(mapping.drives == ["drive-0": ""])
    #expect(mapping.backups.count == 1)
    let full = try #require(mapping.backups.first)
    #expect(full.name == "backup-20260923T022236Z" && full.os == "linux" && full.files.count == 23 && full.registryHash == nil)
    #expect(full.files[deckSave + "file0.rmmzsave"] == FileEntry(hash: "3397ec66e7d2eec108de0e458e28eddb683b273e", size: 174733))
    let diff = try #require(full.children.first)
    #expect(diff.name == "backup-20261025T164619Z-diff" && diff.os == "mac" && diff.files.count == 3 && diff.registry == nil)

    let written = mapping.yamlString()
    #expect(written == text)
    #expect(try LudusaviMapping(yaml: written) == mapping)
}

@Test func yamlQuotesWhatSerdeQuotes() throws {
    var mapping = LudusaviMapping(name: "Doki Doki: Literature Club!", drives: ["drive-C": "C:"])
    mapping.backups = [FullBackup(name: "backup-1", when: "2026-01-01T00:00:00Z", os: "windows", comment: "it's \"quoted\"\nnext",
                                  locked: true, semantics: nil, files: ["C:/Users/a b/save #1.dat": FileEntry(hash: "123", size: 4)],
                                  registryHash: "abc", children: [])]
    let text = mapping.yamlString()
    #expect(text.contains("name: \"Doki Doki: Literature Club!\""))
    #expect(text.contains("\"C:/Users/a b/save #1.dat\":"))
    #expect(text.contains("hash: \"123\""))
    #expect(text.contains("locked: true"))
    #expect(try LudusaviMapping(yaml: text) == mapping)
}

@Test func differentialRemovalsAndRegistryOverridesRoundTrip() throws {
    let text = """
    ---
    name: Game
    drives:
      drive-0: ""
    backups:
      - name: "."
        when: "2026-01-01T00:00:00Z"
        files:
          /a/save/x.rpgsave:
            hash: aa
            size: 1
        registry:
          hash: ff
        children:
          - name: backup-20260102T000000Z-diff
            when: "2026-01-02T00:00:00.5Z"
            files:
              /a/save/x.rpgsave: ~
            registry:
              hash: ~

    """
    let mapping = try LudusaviMapping(yaml: text)
    let diff = try #require(mapping.backups.first?.children.first)
    #expect(diff.files["/a/save/x.rpgsave"] == .removed)
    #expect(diff.registry == .init(hash: nil))
    #expect(mapping.effectiveFiles()?.isEmpty == true)
    #expect(mapping.storedPath(of: "/a/save/x.rpgsave", in: ".") == "drive-0/a/save/x.rpgsave")
    #expect(try LudusaviMapping(yaml: mapping.yamlString()) == mapping)
}

@Test func effectiveFilesAreTheFullBackupPlusTheChosenDifferential() throws {
    let mapping = try LudusaviMapping(yaml: String(contentsOf: fixtures.appendingPathComponent("nosocks-mapping.yaml"), encoding: .utf8))
    let latest = try #require(mapping.effectiveFiles())
    #expect(latest.count == 23)
    #expect(latest[deckSave + "file0.rmmzsave"] == StoredFile(entry: FileEntry(hash: "f5999febed67cebc1eda7b8d2414764f6ebe8e0a", size: 148141),
                                                              backup: "backup-20261025T164619Z-diff"))
    #expect(latest[deckSave + "file1.rmmzsave"]?.backup == "backup-20260923T022236Z")
    #expect(latest.values.filter { $0.backup.hasSuffix("-diff") }.count == 3)

    let full = try #require(mapping.effectiveFiles("backup-20260923T022236Z"))
    #expect(full[deckSave + "file0.rmmzsave"]?.entry.hash == "3397ec66e7d2eec108de0e458e28eddb683b273e")
    #expect(mapping.effectiveFiles("backup-nope") == nil)

    #expect(mapping.summaries.map(\.name) == ["backup-20260923T022236Z", "backup-20261025T164619Z-diff"])
    #expect(mapping.summaries.map(\.fileCount) == [23, 23])
    #expect(mapping.summaries.last?.changedCount == 3)
    #expect(mapping.storedPath(of: deckSave + "file0.rmmzsave", in: "backup-20261025T164619Z-diff")
            == "backup-20261025T164619Z-diff/drive-0/home/deck/.local/share/Steam/steamapps/common/NoSocksRPG/save/file0.rmmzsave")
}

@Test func timestampsMatchLudusavi() throws {
    let date = try #require(Timestamp.parse("2026-10-25T16:46:19.618203Z"))
    #expect(abs(date.timeIntervalSince1970 - 1_792_946_779.618203) < 0.0001)
    #expect(Timestamp.parse("2026-09-23T02:22:36.001866802Z") != nil)
    #expect(Timestamp.folderStamp(date) == "20261025T164619Z")
    #expect(Timestamp.format(Date(timeIntervalSince1970: 1_792_946_779.5)) == "2026-10-25T16:46:19.500Z")
}

@Test func configGivesRetention() throws {
    let config = try LudusaviConfig(yaml: """
    ---
    runtime:
      threads: ~
    backup:
      path: /Users/me/ludusavi-backup
      ignoredGames: []
      retention:
        full: 1
        differential: 5
      format:
        chosen: simple
    restore:
      path: /Users/me/ludusavi-backup
    """)
    #expect(config.retention == Retention(full: 1, differential: 5))
}

// MARK: - RPG Maker save rule

@Test func rpgMakerRuleMapsDeckPaths() throws {
    let base = try temporaryDirectory()
    let location = try #require(try makeGame(in: base).first)
    #expect(location.directory.lastPathComponent == "save")
    #expect(location.directory.deletingLastPathComponent().lastPathComponent == "No Socks RPG")
    let rule = location.relativePath
    #expect(rule(deckSave + "file0.rmmzsave") == "file0.rmmzsave")
    #expect(rule(deckSave + "global.rmmzsave") == "global.rmmzsave")
    #expect(rule("C:/Program Files (x86)/Steam/steamapps/common/Game/www/save/file3.rpgsave") == "file3.rpgsave")
    #expect(rule("/home/deck/.local/share/Steam/steamapps/common/NoSocksRPG/img/file0.rmmzsave") == nil)
    #expect(rule(deckSave + "notes.txt") == nil)
    #expect(rule("/save/file0.rmmzsave") == nil)
    #expect(rule(location.directory.appendingPathComponent("file0.rmmzsave").path) == "file0.rmmzsave")
}

@Test func rpgMakerMVSavesUnderTheWebRoot() throws {
    let game = try temporaryDirectory().appendingPathComponent("MV Game")
    try fm.createDirectory(at: game.appendingPathComponent("www/js"), withIntermediateDirectories: true)
    try Data().write(to: game.appendingPathComponent("www/js/rpg_core.js"))
    let location = try #require(RPGMakerPlugin(.mv).saveLocations(folder: game, dataDirectory: game).first)
    #expect(location.directory.path.hasSuffix("MV Game/www/save"))
}

// MARK: - Restore and back up

@Test func aFirstRenPyBackupRestoresOnAnotherInstall() throws {
    let base = try temporaryDirectory()
    let store = LudusaviStore(root: base.appendingPathComponent("ludusavi-backup"))
    try fm.createDirectory(at: store.root, withIntermediateDirectories: true)
    let game = GameID("Test Game")
    let mac = RenPyPlugin().saveLocations(folder: base, dataDirectory: base.appendingPathComponent("Mac/GameData/Test Game"))
    try write(["1-1-LT1.save": "slot one", "persistent": "p"], to: mac[0])
    let backup = try store.backUp(folder: nil, title: "Test Game", game: game, locations: mac, retention: nil,
                                  now: Date(timeIntervalSince1970: 1_792_886_400), os: "mac", comment: nil)
    #expect(backup.paths == [.vndegen: 2])
    #expect(try store.mapping("Test Game").backups.first.map { Set($0.files.keys) }
        == ["/VNDegen/Test Game/saves/1-1-LT1.save", "/VNDegen/Test Game/saves/persistent"])

    let other = RenPyPlugin().saveLocations(folder: base, dataDirectory: base.appendingPathComponent("Other/GameData/Test Game"))
    let snapshot = base.appendingPathComponent("Snapshots/Test Game")
    try fm.createDirectory(at: snapshot.deletingLastPathComponent(), withIntermediateDirectories: true)
    let restored = try store.restore(folder: "Test Game", backup: nil, locations: other, snapshot: snapshot)
    #expect(restored.restored == ["1-1-LT1.save", "persistent"])
    #expect(try contents(of: other[0]) == ["1-1-LT1.save": "slot one", "persistent": "p"])
}

@Test func restoreNeverWritesThroughALinkOutOfTheSaveFolder() throws {
    let base = try temporaryDirectory()
    let store = LudusaviStore(root: base.appendingPathComponent("ludusavi-backup"))
    try fm.createDirectory(at: store.root, withIntermediateDirectories: true)
    let game = GameID("Test Game")
    let data = base.appendingPathComponent("Mac/GameData/Test Game")
    let locations = RenPyPlugin().saveLocations(folder: base, dataDirectory: data)
    try fm.createDirectory(at: locations[0].directory.appendingPathComponent("link"), withIntermediateDirectories: true)
    try write(["link/library.json": "escaped", "1-1-LT1.save": "slot one"], to: locations[0])
    _ = try store.backUp(folder: nil, title: "Test Game", game: game, locations: locations, retention: nil, os: "mac", comment: nil)

    try fm.removeItem(at: locations[0].directory.appendingPathComponent("link"))
    try fm.createSymbolicLink(atPath: locations[0].directory.appendingPathComponent("link").path, withDestinationPath: "../../..")
    let snapshot = base.appendingPathComponent("Snapshots/Test Game")
    try fm.createDirectory(at: snapshot.deletingLastPathComponent(), withIntermediateDirectories: true)
    #expect(throws: SaveSyncError.self) { try store.restore(folder: "Test Game", backup: nil, locations: locations, snapshot: snapshot) }
    #expect(!fm.fileExists(atPath: base.appendingPathComponent("Mac/library.json").path))
}

@Test func backUpThenRestoreAndUndo() throws {
    let base = try temporaryDirectory()
    let store = LudusaviStore(root: base.appendingPathComponent("ludusavi-backup"))
    try fm.createDirectory(at: store.root, withIntermediateDirectories: true)
    let locations = try makeGame(in: base, saves: ["file1.rmmzsave": "one", "global.rmmzsave": "g1"])
    let game = GameID("No Socks RPG")
    let start = Date(timeIntervalSince1970: 1_792_886_400)

    let first = try store.backUp(folder: nil, title: "No Socks RPG", game: game, locations: locations, retention: nil, now: start,
                                 os: "mac", comment: "VNDegen on Mac")
    #expect(first.backup == "backup-20261025T000000Z" && !first.isDifferential && first.paths == [.vndegen: 2])
    var mapping = try store.mapping("No Socks RPG")
    #expect(mapping.backups.first?.files["/VNDegen/No Socks RPG/save/file1.rmmzsave"] == FileEntry(hash: sha1("one"), size: 3))

    let again = try store.backUp(folder: "No Socks RPG", title: "No Socks RPG", game: game, locations: locations,
                                 retention: Retention(full: 1, differential: 1), now: start + 60, os: "mac", comment: nil)
    #expect(again.backup == nil)

    try write(["file1.rmmzsave": "one v2", "file2.rmmzsave": "two"], to: locations[0])
    let diff = try store.backUp(folder: "No Socks RPG", title: "No Socks RPG", game: game, locations: locations,
                                retention: Retention(full: 1, differential: 1), now: start + 120, os: "mac", comment: nil)
    #expect(diff.backup == "backup-20261025T000200Z-diff" && diff.isDifferential && diff.filesWritten == 2)
    #expect(diff.paths == [.latestBackup: 2, .commonFolder: 1])
    mapping = try store.mapping("No Socks RPG")
    let child = try #require(mapping.backups.first?.children.first)
    #expect(child.files == ["/VNDegen/No Socks RPG/save/file1.rmmzsave": .changed(FileEntry(hash: sha1("one v2"), size: 6)),
                            "/VNDegen/No Socks RPG/save/file2.rmmzsave": .changed(FileEntry(hash: sha1("two"), size: 3))])

    let snapshot = base.appendingPathComponent("Snapshots/No Socks RPG")
    try fm.createDirectory(at: snapshot.deletingLastPathComponent(), withIntermediateDirectories: true)
    try write(["file3.rmmzsave": "local only"], to: locations[0])
    let restored = try store.restore(folder: "No Socks RPG", backup: "backup-20261025T000000Z", locations: locations, snapshot: snapshot)
    #expect(restored.restored == ["file1.rmmzsave", "global.rmmzsave"])
    #expect(try contents(of: locations[0]) == ["file1.rmmzsave": "one", "file2.rmmzsave": "two", "file3.rmmzsave": "local only",
                                               "global.rmmzsave": "g1"])

    try LudusaviStore.undoRestore(locations: locations, snapshot: snapshot)
    #expect(try contents(of: locations[0]) == ["file1.rmmzsave": "one v2", "file2.rmmzsave": "two", "file3.rmmzsave": "local only",
                                               "global.rmmzsave": "g1"])
    #expect(!fm.fileExists(atPath: snapshot.path))
    #expect(throws: SaveSyncError.noSnapshot) { try LudusaviStore.undoRestore(locations: locations, snapshot: snapshot) }
}

@Test func retentionFollowsLudusavi() throws {
    var mapping = LudusaviMapping(name: "G")
    #expect(!Retention(full: 1, differential: 1).allowsDifferential(after: mapping))
    mapping.backups = [FullBackup(name: "f", when: Timestamp.epoch, locked: false, files: [:], registryHash: nil, children: [])]
    #expect(Retention(full: 1, differential: 1).allowsDifferential(after: mapping))
    #expect(!Retention(full: 1, differential: 0).allowsDifferential(after: mapping))
    mapping.backups[0].children = [DifferentialBackup(name: "d", when: Timestamp.epoch, locked: false, files: [:], registry: nil)]
    #expect(!Retention(full: 2, differential: 1).allowsDifferential(after: mapping))
    #expect(Retention(full: 2, differential: 2).allowsDifferential(after: mapping))
    #expect(Retention(inferredFrom: mapping) == Retention(full: 1, differential: 1))
}

@Test func restoreRefusesCorruptBackups() throws {
    let base = try temporaryDirectory()
    let store = LudusaviStore(root: base.appendingPathComponent("backups"))
    let locations = try makeGame(in: base, saves: ["file1.rmmzsave": "one"])
    _ = try store.backUp(folder: nil, title: "G", game: GameID("G"), locations: locations, retention: nil, os: "mac", comment: nil)
    let stored = try #require(LudusaviStore.regularFiles(in: store.root.appendingPathComponent("G")).first { $0.hasSuffix("file1.rmmzsave") })
    try Data("tampered".utf8).write(to: store.root.appendingPathComponent("G").appendingPathComponent(stored))
    try write(["file1.rmmzsave": "mine"], to: locations[0])

    let snapshot = base.appendingPathComponent("snapshot")
    #expect(throws: SaveSyncError.self) {
        try store.restore(folder: "G", backup: nil, locations: locations, snapshot: snapshot)
    }
    #expect(try contents(of: locations[0]) == ["file1.rmmzsave": "mine"])
    #expect(!fm.fileExists(atPath: snapshot.path))
}

@Test func anUnreadableMappingIsNeverReplaced() throws {
    let base = try temporaryDirectory()
    let store = LudusaviStore(root: base.appendingPathComponent("backups"))
    let locations = try makeGame(in: base, saves: ["file1.rmmzsave": "one"])
    let gameDir = store.root.appendingPathComponent("G")
    try fm.createDirectory(at: gameDir, withIntermediateDirectories: true)
    let mappingURL = gameDir.appendingPathComponent("mapping.yaml")
    let notText = Data([0xFF, 0xFE, 0x00, 0x80])
    try notText.write(to: mappingURL)

    #expect(throws: SaveSyncError.unreadable(mappingURL.path)) {
        try store.backUp(folder: "G", title: "G", game: GameID("G"), locations: locations, retention: nil, os: "mac", comment: nil)
    }
    #expect(try Data(contentsOf: mappingURL) == notText)
    #expect(try fm.contentsOfDirectory(atPath: gameDir.path) == ["mapping.yaml"])
    #expect(try LudusaviStore.regularFiles(in: base.appendingPathComponent("no such folder")).isEmpty)
}

/// A failed backup removes its folder, so two backups that started from the same mapping must never share one.
@Test func eachBackupGetsAFolderOfItsOwn() throws {
    let gameDir = try temporaryDirectory().appendingPathComponent("G")
    let mapping = LudusaviMapping(name: "G")
    let now = Date(timeIntervalSince1970: 0)
    let first = try LudusaviStore.makeBackupFolder(differential: false, now: now, mapping: mapping, gameDir: gameDir)
    let second = try LudusaviStore.makeBackupFolder(differential: false, now: now, mapping: mapping, gameDir: gameDir)
    #expect(first == "backup-19700101T000000Z" && second == "backup-19700101T000001Z")
    #expect(try fm.contentsOfDirectory(atPath: gameDir.path).sorted() == [first, second])
    // Another device's folder from the same second has another name.
    let mine = try LudusaviStore.makeBackupFolder(differential: true, now: now, device: "1a2b3c4d", mapping: mapping, gameDir: gameDir)
    let theirs = try LudusaviStore.makeBackupFolder(differential: true, now: now, device: "5e6f7a8b", mapping: mapping, gameDir: gameDir)
    #expect(mine == "backup-19700101T000000Z-diff-1a2b3c4d" && theirs == "backup-19700101T000000Z-diff-5e6f7a8b")
}

@Test func undoRefusesSnapshotsItCannotTrust() throws {
    let base = try temporaryDirectory()
    let store = LudusaviStore(root: base.appendingPathComponent("backups"))
    let locations = try makeGame(in: base, saves: ["file1.rmmzsave": "one"])
    _ = try store.backUp(folder: nil, title: "G", game: GameID("G"), locations: locations, retention: nil, os: "mac", comment: nil)
    try write(["file1.rmmzsave": "mine", "file2.rmmzsave": "two"], to: locations[0])
    let snapshot = base.appendingPathComponent("snapshot")
    _ = try store.restore(folder: "G", backup: nil, locations: locations, snapshot: snapshot)
    #expect(LudusaviStore.isComplete(snapshot: snapshot))
    let afterRestore = ["file1.rmmzsave": "one", "file2.rmmzsave": "two"]

    // A snapshot folder that can't be listed must not read as empty, or undo would delete every save.
    let saved = snapshot.appendingPathComponent("0").path
    try fm.setAttributes([.posixPermissions: 0], ofItemAtPath: saved)
    #expect(throws: SaveSyncError.self) { try LudusaviStore.undoRestore(locations: locations, snapshot: snapshot) }
    try fm.setAttributes([.posixPermissions: 0o755], ofItemAtPath: saved)
    #expect(try contents(of: locations[0]) == afterRestore)

    // A save folder that can't be listed stops a restore before it writes, since its snapshot would be incomplete.
    let saves = locations[0].directory.path
    try fm.setAttributes([.posixPermissions: 0], ofItemAtPath: saves)
    #expect(throws: SaveSyncError.self) { try store.restore(folder: "G", backup: nil, locations: locations, snapshot: snapshot) }
    try fm.setAttributes([.posixPermissions: 0o755], ofItemAtPath: saves)
    #expect(try contents(of: locations[0]) == afterRestore)
    #expect(LudusaviStore.isComplete(snapshot: snapshot))

    try fm.removeItem(at: snapshot.appendingPathComponent("complete"))
    #expect(throws: SaveSyncError.incompleteSnapshot) { try LudusaviStore.undoRestore(locations: locations, snapshot: snapshot) }
    #expect(try contents(of: locations[0]) == afterRestore)
}

@Test func aSaveBackedUpFromTwoFoldersRestoresAndBacksUpFromTheSameOne() throws {
    let base = try temporaryDirectory()
    let store = LudusaviStore(root: base.appendingPathComponent("backups"))
    let locations = try makeGame(in: base)
    let mac = "/Users/player/Games/NoSocksRPG/save/"
    let stored = [mac + "file1.rmmzsave": "mac", deckSave + "file1.rmmzsave": "deck", deckSave + "file2.rmmzsave": "two",
                  deckSave + "global.rmmzsave": "g"]
    var mapping = LudusaviMapping(name: "G")
    mapping.backups = [FullBackup(name: "backup-1", when: Timestamp.epoch, locked: false,
                                  files: stored.mapValues { FileEntry(hash: sha1($0), size: UInt64($0.utf8.count)) },
                                  registryHash: nil, children: [])]
    let gameDir = store.root.appendingPathComponent("G")
    for (path, content) in stored {
        let url = gameDir.appendingPathComponent(try #require(mapping.storedPath(of: path, in: "backup-1")))
        try fm.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data(content.utf8).write(to: url)
    }
    try Data(mapping.yamlString().utf8).write(to: gameDir.appendingPathComponent("mapping.yaml"))

    let report = try store.restore(folder: "G", backup: nil, locations: locations, snapshot: base.appendingPathComponent("snapshot"))
    #expect(report.skipped == [mac + "file1.rmmzsave"])
    #expect(try contents(of: locations[0])["file1.rmmzsave"] == "deck")

    try write(["file1.rmmzsave": "deck v2"], to: locations[0])
    _ = try store.backUp(folder: "G", title: "G", game: GameID("G"), locations: locations,
                         retention: Retention(full: 1, differential: 1), os: "mac", comment: nil)
    let diff = try #require(try store.mapping("G").backups.last?.children.last)
    #expect(diff.files == [deckSave + "file1.rmmzsave": .changed(FileEntry(hash: sha1("deck v2"), size: 7))])
}

/// Runs against a copy of the user's real No Socks RPG backup, when the Mac running the tests has it.
@Suite(.enabled(if: fm.fileExists(atPath: realSample.path)))
struct RealSampleTests {
    @Test func restoreLatestThenBackUpADifferential() throws {
        let base = try temporaryDirectory()
        let root = base.appendingPathComponent("ludusavi-backup")
        try fm.createDirectory(at: root, withIntermediateDirectories: true)
        try fm.copyItem(at: realSample, to: root.appendingPathComponent("No Socks RPG"))
        let store = LudusaviStore(root: root)
        let locations = try makeGame(in: base)

        let report = try store.restore(folder: "No Socks RPG", backup: nil, locations: locations,
                                       snapshot: base.appendingPathComponent("snapshot"))
        #expect(report.restored.count == 23 && report.skipped.isEmpty)
        let mapping = try store.mapping("No Socks RPG")
        for (path, stored) in try #require(mapping.effectiveFiles()) {
            let local = locations[0].directory.appendingPathComponent(URL(fileURLWithPath: path).lastPathComponent)
            let source = root.appendingPathComponent("No Socks RPG").appendingPathComponent(try #require(mapping.storedPath(of: path, in: stored.backup)))
            #expect(try Data(contentsOf: local) == Data(contentsOf: source))
        }

        let before = try tree(root.appendingPathComponent("No Socks RPG"))
        try Data("new progress".utf8).write(to: locations[0].directory.appendingPathComponent("file1.rmmzsave"))
        try Data("brand new".utf8).write(to: locations[0].directory.appendingPathComponent("file21.rmmzsave"))
        let backup = try store.backUp(folder: "No Socks RPG", title: "No Socks RPG", game: GameID("No Socks RPG"), locations: locations,
                                      retention: nil, os: "mac", comment: "VNDegen test")
        let name = try #require(backup.backup)
        #expect(backup.isDifferential && backup.paths == [.latestBackup: 23, .commonFolder: 1])

        let after = try tree(root.appendingPathComponent("No Socks RPG"))
        for (path, hash) in before where path != "mapping.yaml" { #expect(after[path] == hash, "\(path) changed") }
        #expect(!fm.fileExists(atPath: root.appendingPathComponent("No Socks RPG/mapping.yaml.vndegen-previous").path))

        let updated = try store.mapping("No Socks RPG")
        #expect(Array(updated.backups[0].children.dropLast()) == mapping.backups[0].children)
        let diff = try #require(updated.backups[0].children.last)
        #expect(diff.name == name && diff.registry == nil && diff.comment == "VNDegen test")
        // The earlier diff's three files differ from the full backup, so they carry forward with the two changes.
        #expect(Set(diff.files.keys) == Set(["config", "file0", "file1", "file21", "global"].map { deckSave + $0 + ".rmmzsave" }))
        #expect(diff.files[deckSave + "file1.rmmzsave"] == .changed(FileEntry(hash: sha1("new progress"), size: 12)))
        for (path, change) in diff.files {
            guard case let .changed(entry) = change else { Issue.record("\(path) marked removed"); continue }
            let stored = root.appendingPathComponent("No Socks RPG").appendingPathComponent(try #require(updated.storedPath(of: path, in: name)))
            let data = try Data(contentsOf: stored)
            #expect(LudusaviStore.sha1(data) == entry.hash && UInt64(data.count) == entry.size)
        }
    }
}

private let realSample = fm.homeDirectoryForCurrentUser.appendingPathComponent("vndegen-testgames/ludusavi-sample/No Socks RPG")

@Test func ludusavisOwnRetentionStartsANewFullAfterTwoDifferentials() throws {
    let base = try temporaryDirectory()
    let store = LudusaviStore(root: base.appendingPathComponent("backups"))
    let locations = try makeGame(in: base, saves: ["file1.rmmzsave": "v0"])
    let limits = Retention(full: 3, differential: 2)
    var kinds: [Bool] = []
    for version in 1...4 {
        let report = try store.backUp(folder: "G", title: "G", game: GameID("G"), locations: locations, retention: limits,
                                      now: Date(timeIntervalSince1970: Double(version) * 3600), os: "mac", comment: nil)
        kinds.append(report.isDifferential)
        try write(["file1.rmmzsave": "v\(version)"], to: locations[0])
    }
    #expect(kinds == [false, true, true, false])
    let mapping = try store.mapping("G")
    #expect(mapping.backups.map(\.children.count) == [2, 0])
}

// MARK: - Settings

/// A library with No Socks RPG and the save service's folder, as a fresh install has them.
@MainActor
private func freshLibrary() async throws -> (library: LibraryStore, directory: URL) {
    let base = try temporaryDirectory()
    _ = try makeGame(in: base, saves: ["file1.rmmzsave": "one"])
    let library = LibraryStore(paths: LibraryPaths(gamesRoot: base.appendingPathComponent("Games"), support: base.appendingPathComponent("Support")),
                               engines: [RPGMakerPlugin(.mz)])
    await library.refresh()
    let directory = library.paths.support.appendingPathComponent("SaveSync", isDirectory: true)
    try fm.createDirectory(at: directory, withIntermediateDirectories: true)
    return (library, directory)
}

@MainActor
@Test func settingsFromVersion040LeaveSyncOffUntilTheShareIsEntered() async throws {
    let (library, directory) = try await freshLibrary()
    let id = GameID("No Socks RPG")
    // As VNDegen 0.4.0 saved them, with its SSH key beside them.
    try Data("""
    {"server":{"ssh":{"host":"server","port":22,"user":"player"},"path":"/srv/ludusavi","hostKey":"ssh-ed25519 AAAA"},
     "autoBackup":true,"checkBeforeLaunch":false,"retention":{"full":3,"differential":2},"matches":{"No Socks RPG":"No Socks"},
     "synced":{"No Socks RPG":{"folder":"No Socks","saves":{}}},"deviceID":"1a2b3c4d"}
    """.utf8).write(to: directory.appendingPathComponent("settings.json"))
    let key = directory.appendingPathComponent("ssh-ed25519")
    try Data(repeating: 7, count: 32).write(to: key)
    let connects = Locked(0)
    let connector: SaveSyncService.Connector = { _ in
        connects { $0 += 1 }
        throw SaveSyncError.notSetUp
    }

    let service = SaveSyncService(library: library, connector: connector, manifest: nil, configRetention: nil)
    #expect(service.server == nil && service.needsShare && service.connection == .notSetUp)
    #expect(service.deviceID == "1a2b3c4d")
    #expect(service.autoBackup && !service.checkBeforeLaunch && service.manualRetention == Retention(full: 3, differential: 2))
    #expect(service.match(for: id)?.source == .picked)
    #if os(macOS)
    #expect(!fm.fileExists(atPath: key.path))
    #endif

    // Nothing reaches a server.
    await service.refresh()
    await service.testConnection()
    await service.sessionEnded(id)
    #expect(await service.launchOffer(id) == nil)
    await #expect(throws: SaveSyncError.notSetUp) { try await service.status(for: id) }
    #expect(connects { $0 } == 0)

    // The note stays until the share is entered.
    let again = SaveSyncService(library: library, connector: connector, manifest: nil, configRetention: nil)
    #expect(again.needsShare && again.deviceID == "1a2b3c4d")
    try again.setServer(host: "nas.example", port: 445, guest: true, username: "", password: "", path: "games/Ludusavi")
    #expect(!again.needsShare && again.server?.remote == .smb(SMBRemote(host: "nas.example")))
    #expect(SaveSyncService(library: library, connector: connector, manifest: nil, configRetention: nil).server == again.server)
}

#if os(macOS)
@MainActor
@Test func theMacKeepsTheSharePasswordInAFileOnlyItsUserCanRead() async throws {
    let (library, directory) = try await freshLibrary()
    let service = SaveSyncService(library: library, connector: nil, manifest: nil, configRetention: nil)
    try service.setServer(host: "nas.example", port: 445, guest: false, username: "player", password: "secret", path: "games/Ludusavi")
    let file = directory.appendingPathComponent("smb-password")
    #expect((try fm.attributesOfItem(atPath: file.path)[.posixPermissions] as? Int) == 0o600)
    #expect(try String(contentsOf: file, encoding: .utf8) == "secret")
    #expect(service.storedPassword() == "secret")
    #expect(try !fm.contentsOfDirectory(atPath: directory.path).contains { $0.hasPrefix(".smb-password") })
    // The settings never hold it.
    #expect(try !String(contentsOf: directory.appendingPathComponent("settings.json"), encoding: .utf8).contains("secret"))

    try service.setServer(host: "nas.example", port: 445, guest: false, username: "player", password: "changed", path: "games/Ludusavi")
    #expect(service.storedPassword() == "changed")
    #expect((try fm.attributesOfItem(atPath: file.path)[.posixPermissions] as? Int) == 0o600)

    try service.setServer(host: "nas.example", port: 445, guest: true, username: "player", password: "changed", path: "games/Ludusavi")
    #expect(!fm.fileExists(atPath: file.path))
    #expect(service.storedPassword() == nil)
}
#endif

/// A PC backup's `File1.rmmzsave` restores over a local `file1.rmmzsave`, as it would on Windows, rather than beside it.
/// `VNDEGEN_TEST_DIR` puts the game on a case-sensitive volume, which `scripts/build.sh test` mounts.
@Test func restoreReplacesASaveThatDiffersOnlyInCase() throws {
    let scratch = ProcessInfo.processInfo.environment["VNDEGEN_TEST_DIR"].map { URL(fileURLWithPath: $0, isDirectory: true) }
        ?? fm.temporaryDirectory
    let base = scratch.appendingPathComponent("savesync-\(UUID().uuidString)", isDirectory: true)
    defer { try? fm.removeItem(at: base) }
    let store = LudusaviStore(root: base.appendingPathComponent("ludusavi-backup"))
    try fm.createDirectory(at: store.root, withIntermediateDirectories: true)
    let locations = try makeGame(in: base, saves: ["File1.rmmzsave": "pc"])
    _ = try store.backUp(folder: nil, title: "Test Game 01", game: GameID("Test Game 01"), locations: locations, retention: nil,
                         os: "windows", comment: nil)
    try fm.removeItem(at: locations[0].directory.appendingPathComponent("File1.rmmzsave"))
    try write(["file1.rmmzsave": "local"], to: locations[0])

    let preview = try store.preview(folder: "Test Game 01", backup: nil, locations: locations)
    #expect(preview.replaced == ["file1.rmmzsave"] && preview.added.isEmpty)
    let snapshot = base.appendingPathComponent("Snapshots/Test Game 01")
    try fm.createDirectory(at: snapshot.deletingLastPathComponent(), withIntermediateDirectories: true)
    _ = try store.restore(folder: "Test Game 01", backup: nil, locations: locations, snapshot: snapshot)
    #expect(try contents(of: locations[0]) == ["file1.rmmzsave": "pc"])
}

/// A backup made on a disk that keeps case can hold `File1.rmmzsave` and `file1.rmmzsave`. They are one save on a device
/// that has one of them, and the one spelled as on disk restores.
@Test(.enabled(if: ProcessInfo.processInfo.environment["VNDEGEN_TEST_DIR"] != nil))
func aBackupWithTwoSpellingsOfOneSaveRestoresOne() throws {
    let base = URL(fileURLWithPath: ProcessInfo.processInfo.environment["VNDEGEN_TEST_DIR"]!)
        .appendingPathComponent("savesync-\(UUID().uuidString)", isDirectory: true)
    defer { try? fm.removeItem(at: base) }
    let store = LudusaviStore(root: base.appendingPathComponent("ludusavi-backup"))
    try fm.createDirectory(at: store.root, withIntermediateDirectories: true)
    let locations = try makeGame(in: base, saves: ["File1.rmmzsave": "upper", "file1.rmmzsave": "lower"])
    _ = try store.backUp(folder: nil, title: "Test Game 02", game: GameID("Test Game 02"), locations: locations, retention: nil,
                         os: "linux", comment: nil)
    try fm.removeItem(at: locations[0].directory.appendingPathComponent("File1.rmmzsave"))
    try write(["file1.rmmzsave": "local"], to: locations[0])

    #expect(try store.latestHashes(folder: "Test Game 02", locations: locations)?.hashes == ["0:file1.rmmzsave": sha1("lower")])
    let snapshot = base.appendingPathComponent("Snapshots/Test Game 02")
    try fm.createDirectory(at: snapshot.deletingLastPathComponent(), withIntermediateDirectories: true)
    _ = try store.restore(folder: "Test Game 02", backup: nil, locations: locations, snapshot: snapshot)
    #expect(try contents(of: locations[0]) == ["file1.rmmzsave": "lower"])
}

/// Two spellings of one save restore as one file even when the device has neither yet.
@Test(.enabled(if: ProcessInfo.processInfo.environment["VNDEGEN_TEST_DIR"] != nil))
func twoSpellingsOfANewSaveRestoreAsOneFile() throws {
    let base = URL(fileURLWithPath: ProcessInfo.processInfo.environment["VNDEGEN_TEST_DIR"]!)
        .appendingPathComponent("savesync-\(UUID().uuidString)", isDirectory: true)
    defer { try? fm.removeItem(at: base) }
    let store = LudusaviStore(root: base.appendingPathComponent("ludusavi-backup"))
    try fm.createDirectory(at: store.root, withIntermediateDirectories: true)
    let locations = try makeGame(in: base, saves: ["File1.rmmzsave": "upper", "file1.rmmzsave": "lower"])
    _ = try store.backUp(folder: nil, title: "Test Game 03", game: GameID("Test Game 03"), locations: locations, retention: nil,
                         os: "linux", comment: nil)
    try fm.removeItem(at: locations[0].directory)

    let snapshot = base.appendingPathComponent("Snapshots/Test Game 03")
    try fm.createDirectory(at: snapshot.deletingLastPathComponent(), withIntermediateDirectories: true)
    _ = try store.restore(folder: "Test Game 03", backup: nil, locations: locations, snapshot: snapshot)
    // The backup lists File1 first, as uppercase sorts first, so its spelling and contents win.
    #expect(try contents(of: locations[0]) == ["File1.rmmzsave": "upper"])
}
