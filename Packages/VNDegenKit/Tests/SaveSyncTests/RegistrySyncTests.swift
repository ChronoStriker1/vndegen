import Foundation
import RegistryFile
import Testing
import VNCore
@testable import SaveSync

/// A Unity game's PlayerPrefs: only its own key moves between a backup's registry and the game's file here.
@Suite struct RegistrySyncTests {
    /// The registry Ludusavi wrote, from regashii, with the hash Ludusavi records for it. RegistryFileTests checks both.
    static let ludusaviReg = fixtures.deletingLastPathComponent().deletingLastPathComponent()
        .appendingPathComponent("RegistryFileTests/Fixtures/ludusavi-registry.reg")
    static let ludusaviYAML = ludusaviReg.deletingPathExtension().appendingPathExtension("yaml")
    static let ludusaviHash = "2fa7b963f765cfa2528c06f2dd94bdf99ee75323"
    static let key = #"HKEY_CURRENT_USER\Software\Studio\Game"#

    struct Setup {
        var store: LudusaviStore
        var gameDir: URL
        var locations: [SaveLocation]
        var save: RegistrySave
        var snapshot: URL
    }

    /// A full backup from a PC with one save file and the fixture's registry, and a game here with no saves yet.
    static func setup(registryFile: String = "registry.reg", saves: Bool = true) throws -> Setup {
        let base = try temporaryDirectory()
        let root = base.appendingPathComponent("backups")
        let gameDir = root.appendingPathComponent("G", isDirectory: true)
        let locations = try makeGame(in: base)
        let backup = gameDir.appendingPathComponent("backup-1", isDirectory: true)
        try fm.createDirectory(at: backup.appendingPathComponent("drive-0" + deckSave), withIntermediateDirectories: true)
        var files: [String: FileEntry] = [:]
        if saves {
            try Data("pc".utf8).write(to: backup.appendingPathComponent("drive-0" + deckSave + "file1.rmmzsave"))
            files[deckSave + "file1.rmmzsave"] = FileEntry(hash: sha1("pc"), size: 2)
        }
        let source = registryFile.hasSuffix(".yaml") ? ludusaviYAML : ludusaviReg
        try fm.copyItem(at: source, to: backup.appendingPathComponent(registryFile))
        var mapping = LudusaviMapping(name: "G", drives: ["drive-0": ""])
        mapping.backups = [FullBackup(name: "backup-1", when: Timestamp.epoch, os: "windows", locked: false, files: files,
                                      registryHash: ludusaviHash, children: [])]
        try Data(mapping.yamlString().utf8).write(to: gameDir.appendingPathComponent("mapping.yaml"))
        let save = RegistrySave(key: key, file: base.appendingPathComponent("Data/PlayerPrefs.reg"))
        return Setup(store: LudusaviStore(root: root), gameDir: gameDir, locations: locations, save: save,
                     snapshot: base.appendingPathComponent("snapshot"))
    }

    static func fixtureData() throws -> RegistryData { try RegistryData(reg: Data(contentsOf: ludusaviReg)) }

    func backUp(_ setup: Setup, _ retention: Retention) throws -> BackupReport {
        try setup.store.backUp(folder: "G", title: "G", game: GameID("G"), locations: setup.locations, registry: setup.save,
                               retention: retention, now: Date(timeIntervalSince1970: 1_800_000_000), os: "mac", comment: nil)
    }

    @Test func restoreWritesOnlyTheGamesKeyAndUndoRemovesIt() throws {
        let setup = try Self.setup()
        let report = try setup.store.restore(folder: "G", backup: nil, locations: setup.locations, registry: setup.save, snapshot: setup.snapshot)
        #expect(report.restoredRegistry && report.restored == ["file1.rmmzsave"])
        let local = try RegistryData(reg: Data(contentsOf: setup.save.file))
        #expect(local.keys.keys.sorted() == [Self.key])
        #expect(local.values(of: Self.key) == (try Self.fixtureData()).values(of: Self.key))
        // In sync now, by the same hashes a status check compares.
        let latest = try #require(try setup.store.latestHashes(folder: "G", locations: setup.locations, registry: setup.save))
        #expect(latest.hashes == (try LudusaviStore.localHashes(setup.locations, registry: setup.save)))
        #expect(latest.hashes[LudusaviStore.registryHashKey] != nil)

        try LudusaviStore.undoRestore(locations: setup.locations, registry: setup.save, snapshot: setup.snapshot)
        #expect(!fm.fileExists(atPath: setup.save.file.path))
    }

    @Test func undoPutsTheEarlierValuesBack() throws {
        let setup = try Self.setup()
        try LudusaviStore.writeLocalRegistry(["mine": .dword(1)], to: setup.save)
        let before = try Data(contentsOf: setup.save.file)
        let preview = try setup.store.preview(folder: "G", backup: nil, locations: setup.locations, registry: setup.save)
        #expect(preview.replaced == [RestorePreview.registryName] && preview.added == ["file1.rmmzsave"])
        _ = try setup.store.restore(folder: "G", backup: nil, locations: setup.locations, registry: setup.save, snapshot: setup.snapshot)
        #expect(try Data(contentsOf: setup.save.file) != before)
        try LudusaviStore.undoRestore(locations: setup.locations, registry: setup.save, snapshot: setup.snapshot)
        #expect(try Data(contentsOf: setup.save.file) == before)
    }

    /// Many Unity games keep everything in PlayerPrefs, so a backup holding nothing else still restores.
    @Test func aBackupOfOnlyTheRegistryRestores() throws {
        let setup = try Self.setup(saves: false)
        let report = try setup.store.restore(folder: "G", backup: nil, locations: setup.locations, registry: setup.save, snapshot: setup.snapshot)
        #expect(report.restoredRegistry && report.restored.isEmpty)
        #expect(try LudusaviStore.localRegistryValues(setup.save)?["Screenmanager Resolution Width_h182942802"] == .dword(1280))
    }

    @Test func anOlderBackupsRegistryYAMLRestores() throws {
        let setup = try Self.setup(registryFile: "registry.yaml")
        _ = try setup.store.restore(folder: "G", backup: nil, locations: setup.locations, registry: setup.save, snapshot: setup.snapshot)
        #expect(try LudusaviStore.localRegistryValues(setup.save) == (try Self.fixtureData()).values(of: Self.key))
    }

    /// The backup's registry is Ludusavi's with only the game's key changed, every other key written as Ludusavi wrote
    /// it, and the hash Ludusavi would record for that text.
    @Test func aBackupChangesOnlyTheGamesKey() throws {
        let setup = try Self.setup()
        _ = try setup.store.restore(folder: "G", backup: nil, locations: setup.locations, registry: setup.save, snapshot: setup.snapshot)
        var values = try #require(try LudusaviStore.localRegistryValues(setup.save))
        values["Screenmanager Resolution Width_h182942802"] = .dword(1920)
        try LudusaviStore.writeLocalRegistry(values, to: setup.save)

        let report = try backUp(setup, Retention(full: 2, differential: 0))
        let name = try #require(report.backup)
        let written = try String(contentsOf: setup.gameDir.appendingPathComponent("\(name)/registry.reg"), encoding: .utf8)
        #expect(try setup.store.mapping("G").backups.last?.registryHash == LudusaviStore.sha1(Data(written.utf8)))
        var expected = try Self.fixtureData()
        expected.setValues(values, of: Self.key)
        #expect(try RegistryData(reg: written) == expected)
        let original = try String(contentsOf: Self.ludusaviReg, encoding: .utf8).components(separatedBy: "\n\n")
        let sections = written.components(separatedBy: "\n\n")
        #expect(sections.count == original.count && Array(sections.dropFirst(2)) == Array(original.dropFirst(2)))
        #expect(report.saves[LudusaviStore.registryHashKey] == LudusaviStore.registryHash(values, key: Self.key))
    }

    @Test func unchangedValuesMakeNoBackup() throws {
        let setup = try Self.setup()
        _ = try setup.store.restore(folder: "G", backup: nil, locations: setup.locations, registry: setup.save, snapshot: setup.snapshot)
        #expect(try backUp(setup, Retention(full: 2, differential: 0)).backup == nil)
    }

    @Test func aDifferentialHoldsTheNewRegistry() throws {
        let setup = try Self.setup()
        _ = try setup.store.restore(folder: "G", backup: nil, locations: setup.locations, registry: setup.save, snapshot: setup.snapshot)
        try LudusaviStore.writeLocalRegistry(["only": .dword(2)], to: setup.save)
        let report = try backUp(setup, Retention(full: 1, differential: 2))
        let name = try #require(report.backup)
        #expect(report.isDifferential)
        let text = try Data(contentsOf: setup.gameDir.appendingPathComponent("\(name)/registry.reg"))
        let diff = try #require(try setup.store.mapping("G").backups[0].children.last)
        #expect(diff.registry == .init(hash: LudusaviStore.sha1(text)))
        // The full backup's own registry is untouched, and a restore now reads the differential's.
        #expect(try Data(contentsOf: setup.gameDir.appendingPathComponent("backup-1/registry.reg")) == Data(contentsOf: Self.ludusaviReg))
        try LudusaviStore.writeLocalRegistry([:], to: setup.save)
        _ = try setup.store.restore(folder: "G", backup: nil, locations: setup.locations, registry: setup.save, snapshot: setup.snapshot)
        #expect(try LudusaviStore.localRegistryValues(setup.save) == ["only": .dword(2)])
    }

    /// A game's first backup with no registry before starts one holding its key alone.
    @Test func aFirstRegistryHoldsOnlyTheGamesKey() throws {
        let base = try temporaryDirectory()
        let store = LudusaviStore(root: base.appendingPathComponent("backups"))
        let save = RegistrySave(key: Self.key, file: base.appendingPathComponent("PlayerPrefs.reg"))
        try LudusaviStore.writeLocalRegistry(["a": .dword(1)], to: save)
        let report = try store.backUp(folder: "G", title: "G", game: GameID("G"), locations: try makeGame(in: base), registry: save,
                                      retention: Retention(full: 1, differential: 0), os: "mac", comment: nil)
        let name = try #require(report.backup)
        let text = try String(contentsOf: base.appendingPathComponent("backups/G/\(name)/registry.reg"), encoding: .utf8)
        #expect(text == "Windows Registry Editor Version 5.00\n\n[\(Self.key)]\n\"a\"=dword:00000001\n")
    }

    /// When the backup's registry can't be read, its values can't be merged, so it carries forward as it was.
    @Test func anUnreadableRegistryIsCarriedNotReplaced() throws {
        let setup = try Self.setup(registryFile: "registry.yaml")
        try Data("not a mapping".utf8).write(to: setup.gameDir.appendingPathComponent("backup-1/registry.yaml"))
        try LudusaviStore.writeLocalRegistry(["a": .dword(1)], to: setup.save)
        try write(["file1.rmmzsave": "mine"], to: setup.locations[0])
        let name = try #require(try backUp(setup, Retention(full: 2, differential: 0)).backup)
        #expect(try String(contentsOf: setup.gameDir.appendingPathComponent("\(name)/registry.yaml"), encoding: .utf8) == "not a mapping")
        #expect(!fm.fileExists(atPath: setup.gameDir.appendingPathComponent("\(name)/registry.reg").path))
        #expect(try setup.store.mapping("G").backups.last?.registryHash == Self.ludusaviHash)
    }

    /// The file here may hold the game's key alone, so nothing else can reach a backup through it.
    @Test func aFileWithAnotherKeyIsRefused() throws {
        let setup = try Self.setup()
        try fm.createDirectory(at: setup.save.file.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data("Windows Registry Editor Version 5.00\n\n[HKEY_CURRENT_USER\\Software\\Other]\n\"a\"=dword:00000001\n".utf8).write(to: setup.save.file)
        #expect(throws: SaveSyncError.unreadable(setup.save.file.path)) { try LudusaviStore.localRegistryValues(setup.save) }
        #expect(throws: SaveSyncError.unreadable(setup.save.file.path)) { try backUp(setup, Retention(full: 2, differential: 0)) }
    }
}
