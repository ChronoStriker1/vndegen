import EngineRPGMaker
import Foundation
import Rsync
import RsyncTestSupport
import Testing
import VNCore
@testable import SaveSync

/// A key the test server presents until the service trusts it, as an unpinned SSH server does.
private let testHostKey = "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIEjLQDhA2AsybzqQPBopa1MQAmYUvW1j+fU1RqRUtncb"

/// A VNDegen library with No Socks RPG, and a save service whose server is a store folder reached through a real
/// `rsync --server`, the program VNDegen runs over SSH.
@MainActor
private struct Setup {
    let base: URL
    let library: LibraryStore
    let service: SaveSyncService
    let locations: [SaveLocation]
    let store: URL
    let id = GameID("No Socks RPG")

    init(saves: [String: String] = [:], metadata: GameMetadata? = nil, manifest: URL? = nil, otherGames: [String: String] = [:]) async throws {
        base = try temporaryDirectory()
        locations = try makeGame(in: base, saves: saves)
        library = LibraryStore(paths: LibraryPaths(gamesRoot: base.appendingPathComponent("Games"), support: base.appendingPathComponent("Support")),
                               engines: [RPGMakerPlugin(.mz)])
        await library.refresh()
        if let metadata { library.update(GameID("No Socks RPG")) { $0.metadata = metadata } }
        store = base.appendingPathComponent("server/Ludusavi", isDirectory: true)
        try fm.createDirectory(at: store, withIntermediateDirectories: true)
        // Other games already in the store, by title, each with one save at the given path.
        for (title, path) in otherGames {
            var mapping = LudusaviMapping(name: title, drives: ["drive-0": ""])
            mapping.backups = [FullBackup(name: "backup-1", when: Timestamp.epoch, os: "linux", locked: false,
                                          files: [path: FileEntry(hash: sha1("x"), size: 1)], registryHash: nil, children: [])]
            let folder = store.appendingPathComponent(LudusaviMapping.escapeFolderName(title))
            try fm.createDirectory(at: folder, withIntermediateDirectories: true)
            try Data(mapping.yamlString().utf8).write(to: folder.appendingPathComponent("mapping.yaml"))
        }
        service = SaveSyncService(library: library, connector: { server in
            guard server.hostKey == testHostKey else { throw SSHError.unknownHostKey(testHostKey) }
            return ProcessConnection()
        }, manifest: manifest)
        service.setServer(host: "server", port: 22, user: "player", path: store.path)
        await service.testConnection()
        #expect(service.connection == .untrusted(hostKey: testHostKey))
        await service.trustHostKey()
        #expect(service.connection == .connected)
        await service.refresh()
        await service.manifestRefreshed()
    }

    /// Ludusavi on the Steam Deck backing up its own saves of the game into the store.
    func deckBacksUp(_ saves: [String: String], at hour: Double) throws {
        let deck = base.appendingPathComponent("Deck/NoSocksRPG", isDirectory: true)
        try fm.createDirectory(at: deck.appendingPathComponent("js"), withIntermediateDirectories: true)
        try Data().write(to: deck.appendingPathComponent("js/rmmz_core.js"))
        let deckLocations = RPGMakerPlugin(.mz).saveLocations(folder: deck, dataDirectory: base.appendingPathComponent("Deck/Data"))
        try? fm.removeItem(at: deckLocations[0].directory)
        try write(saves, to: deckLocations[0])
        _ = try LudusaviStore(root: store).backUp(folder: "No Socks RPG", title: "No Socks RPG", game: id, locations: deckLocations,
                                                  retention: Retention(full: 3, differential: 2),
                                                  now: Date(timeIntervalSince1970: 1_800_000_000 + hour * 3600), os: "linux", comment: nil)
    }

    func serverMapping() throws -> LudusaviMapping { try LudusaviStore(root: store).mapping("No Socks RPG") }
}

// MARK: - Manifest and matching

@Test func normalizesTitlesLikeLudusavi() {
    for title in ["foo bar", "Foo Bar", "Foo Bar: Any Arbitrary Edition", "Foo Bar - Any Arbitrary Edition", "Foo Bar™ Any Arbitrary Edition",
                  "Foo Bar® - Any Arbitrary Edition", "Foo Bar Game of the Year Edition", "Foo Bar Special Edition", "Foo Bar (2000)",
                  "Foo:Bar", "Foo: Bar", "Fo'o Bar", "Foo \"Bar\"", "  Foo  Bar  "] {
        #expect(LudusaviTitle.normalize(title) == "foo bar", "\(title)")
    }
    #expect(LudusaviTitle.letters("No Socks RPG") == LudusaviTitle.letters("NoSocksRPG"))
}

@Test func scansTheManifestFormat() throws {
    let index = ManifestIndex(scanning: try String(contentsOf: fixtures.appendingPathComponent("manifest-excerpt.yaml"), encoding: .utf8))
    #expect(index.games.count == 4)
    #expect(index.game("Test Game 01") == ManifestGame(steamIDs: [1_000_001], installDirs: ["TestGame01"], saveRules: ["<base>/save/*.rmmzsave"]))
    #expect(index.game("Test Game 02") == ManifestGame(steamIDs: [1_000_003, 1_000_004, 1_000_005], installDirs: ["Test Game 02"],
                                                       saveRules: ["<home>/.local/share/testgame02"]))
    #expect(index.title(steamID: 1_000_002) == "Test Game 01")
    #expect(index.title(steamID: 1_000_005) == "Test Game 02")
    #expect(index.title(matching: "test game 03") == "Test Game 03 (2019)")
    #expect(index.title(matching: "TestGame02") == "Test Game 02")
    #expect(index.title(matching: "Test Game 04") == nil)
    let decoded = try JSONDecoder().decode(ManifestIndex.self, from: JSONEncoder().encode(index))
    #expect(decoded.title(steamID: 1_000_001) == "Test Game 01")
}

@MainActor
@Test(.enabled(if: rsyncPath != nil)) func aFirstBackupGoesWhereTheDeckKeepsTheGame() async throws {
    let metadata = GameMetadata(title: "Something Else", refs: [ExternalRef(source: .steam, id: "1000002")])
    let setup = try await Setup(saves: ["file1.rmmzsave": "one"], metadata: metadata,
                                manifest: fixtures.appendingPathComponent("manifest-excerpt.yaml"),
                                otherGames: ["Test Game 02": "/home/deck/.local/share/Steam/steamapps/common/Test Game 02/save/file0.rmmzsave",
                                             "Test Game 04": "C:/Program Files (x86)/Steam/steamapps/common/Four/save/file0.rmmzsave",
                                             "Test Game 05": "/home/deck/.local/share/Steam/steamapps/common/Five/www/save/file1.rpgsave"])
    let match = try #require(setup.service.match(for: setup.id))
    #expect(match == GameMatch(folder: "Test Game 01", title: "Test Game 01", source: .steamID(1_000_002), inStore: false))
    #expect(setup.service.manifestRules(for: setup.id) == ["<base>/save/*.rmmzsave"])

    let report = try await setup.service.backUp(setup.id)
    #expect(report.paths == [.pcFolder: 1])
    let mapping = try LudusaviStore(root: setup.store).mapping("Test Game 01")
    #expect(mapping.name == "Test Game 01")
    #expect(mapping.backups.first?.files.keys.sorted() == ["/home/deck/.local/share/Steam/steamapps/common/TestGame01/save/file1.rmmzsave"])
    // The Deck path runs through a hidden folder, and the bytes must reach the server under it.
    let backup = try #require(mapping.backups.first)
    for (path, entry) in backup.files {
        let stored = setup.store.appendingPathComponent("Test Game 01").appendingPathComponent(try #require(mapping.storedPath(of: path, in: backup.name)))
        #expect(LudusaviStore.sha1(try Data(contentsOf: stored)) == entry.hash)
    }
    #expect(setup.service.match(for: setup.id)?.inStore == true)
}

@MainActor
@Test(.enabled(if: rsyncPath != nil)) func storeGamesMatchByTitleAndTheUserCanPickAnother() async throws {
    let setup = try await Setup(otherGames: ["No Socks RPG": deckSave + "file1.rmmzsave", "Other Game": "/x/save/file1.rmmzsave"])
    #expect(setup.service.match(for: setup.id) == GameMatch(folder: "No Socks RPG", title: "No Socks RPG", source: .storeTitle, inStore: true))

    let model = GameSavesModel(service: setup.service, game: setup.id)
    model.choosingGame = true
    // The filter hides the matched folder, where focus would otherwise start.
    model.query = "Other"
    _ = model.handle(.down)
    _ = model.handle(.down)
    #expect(model.isFocused(.folder("Other Game")))
    _ = model.handle(.confirm)
    #expect(setup.service.match(for: setup.id) == GameMatch(folder: "Other Game", title: "Other Game", source: .picked, inStore: true))
}

// MARK: - Sync

@MainActor
@Test(.enabled(if: rsyncPath != nil)) func backsUpRestoresAndTracksWhoChangedWhat() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "one", "global.rmmzsave": "g"])
    let service = setup.service, id = setup.id
    #expect(try await service.status(for: id) == .noBackups)

    let first = try await service.backUp(id)
    #expect(first.backup != nil && !first.isDifferential)
    #expect(try await service.status(for: id) == .inSync)
    #expect(try setup.serverMapping().backups.count == 1)

    // Automatic backup only when it is on, and only of saves that changed here.
    try write(["file1.rmmzsave": "two"], to: setup.locations[0])
    #expect(try await service.status(for: id) == .localChanged)
    await service.sessionEnded(id)
    #expect(try setup.serverMapping().summaries.count == 1)
    service.autoBackup = true
    await service.sessionEnded(id)
    #expect(try setup.serverMapping().summaries.count == 2)
    #expect(try await service.status(for: id) == .inSync)

    // The Deck backs up newer saves. The game here did not change, so the newer backup is offered before launch.
    try setup.deckBacksUp(["file1.rmmzsave": "deck", "global.rmmzsave": "g"], at: 1)
    let latest = try #require(try setup.serverMapping().summaries.last)
    #expect(latest.os == "linux")
    #expect(await service.launchOffer(id) == .newerBackup(latest))
    let backups = try await service.backups(for: id)
    #expect(try await service.preview(id, backup: latest.name) == RestorePreview(replaced: ["file1.rmmzsave"], unchanged: ["global.rmmzsave"]))
    #expect(backups.last == latest)
    _ = try await service.restore(id, backup: nil)
    #expect(try contents(of: setup.locations[0]) == ["file1.rmmzsave": "deck", "global.rmmzsave": "g"])
    #expect(await service.launchOffer(id) == nil)

    // Both sides change: a conflict. Automatic backup leaves it for the user.
    try write(["file1.rmmzsave": "mine"], to: setup.locations[0])
    try setup.deckBacksUp(["file1.rmmzsave": "deck 2", "global.rmmzsave": "g"], at: 2)
    let deck2 = try #require(try setup.serverMapping().summaries.last)
    #expect(try await service.status(for: id) == .conflict(deck2))
    await service.sessionEnded(id)
    #expect(try setup.serverMapping().summaries.last == deck2)

    // Keeping these saves backs them up on top at the next session end.
    service.keepLocalSaves(id, over: deck2)
    #expect(try await service.status(for: id) == .localChanged)
    await service.sessionEnded(id)
    #expect(try setup.serverMapping().summaries.last?.os == "mac")
    #expect(try await service.status(for: id) == .inSync)

    // Ludusavi on the Mac or Deck can read every backup VNDegen wrote, with the files where mapping.yaml says.
    let mapping = try setup.serverMapping()
    let gameDir = setup.store.appendingPathComponent("No Socks RPG")
    for summary in mapping.summaries {
        for (path, stored) in try #require(mapping.effectiveFiles(summary.name)) {
            let data = try Data(contentsOf: gameDir.appendingPathComponent(try #require(mapping.storedPath(of: path, in: stored.backup))))
            #expect(LudusaviStore.sha1(data) == stored.entry.hash, "\(summary.name) \(path)")
        }
    }
}

@MainActor
@Test(.enabled(if: rsyncPath != nil)) func retentionDeletesOldBackupsOnTheServer() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "v0"])
    setup.service.manualRetention = Retention(full: 1, differential: 1)
    var pruned: [String] = []
    for version in 1 ... 4 {
        try write(["file1.rmmzsave": "v\(version)"], to: setup.locations[0])
        pruned += try await setup.service.backUp(setup.id).pruned
        try await Task.sleep(for: .seconds(1.1))
    }
    let mapping = try setup.serverMapping()
    // With one full backup, Ludusavi keeps adding differentials and drops the oldest past the limit.
    #expect(mapping.backups.count == 1 && mapping.backups[0].children.count == 1)
    #expect(pruned.count == 2)
    let folders = try fm.contentsOfDirectory(atPath: setup.store.appendingPathComponent("No Socks RPG").path).filter { $0.hasPrefix("backup-") }
    #expect(Set(folders) == Set(mapping.summaries.map(\.name)))
    #expect(try contents(of: setup.locations[0]) == ["file1.rmmzsave": "v4"])
}

@MainActor
@Test(.enabled(if: rsyncPath != nil)) func theServiceRunsOneOperationAtATime() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "one"])
    async let first = setup.service.backUp(setup.id)
    async let second = setup.service.backUp(setup.id)
    async let third = setup.service.backUp(setup.id)
    let made = try await [first, second, third].compactMap(\.backup)
    #expect(made.count == 1)
}

@Test func forgettingExcessBackupsMatchesLudusavi() {
    func full(_ name: String, locked: Bool = false, _ children: [String] = []) -> FullBackup {
        FullBackup(name: name, when: Timestamp.epoch, locked: locked, files: [:], registryHash: nil,
                   children: children.map { DifferentialBackup(name: $0, when: Timestamp.epoch, locked: false, files: [:], registry: nil) })
    }
    var mapping = LudusaviMapping(name: "G", drives: ["drive-0": ""])
    mapping.backups = [full(".", ["d0"]), full("f1", locked: true), full("f2", ["d1", "d2", "d3"]), full("f3")]
    #expect(mapping.forgetExcessBackups(Retention(full: 2, differential: 2)) == ["drive-0", "d0", "d1"])
    #expect(mapping.backups.map(\.name) == ["f1", "f2", "f3"])
    #expect(mapping.backups[1].children.map(\.name) == ["d2", "d3"])
}
