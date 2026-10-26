import EngineRPGMaker
import Foundation
import Rclone
import Testing
import VNCore
@testable import SaveSync

/// How a setup logs in to the test share.
enum Login {
    case guest
    /// The user `player`, with this password.
    case password(String)
}

/// A VNDegen library with No Socks RPG, and a save service whose store is a test server reached through rclone: a local
/// folder through rclone's local backend, or a folder on the test SMB share through the smb backend, as in the app.
@MainActor
struct Setup {
    let base: URL
    let library: LibraryStore
    let service: SaveSyncService
    let locations: [SaveLocation]
    let server: TestServer
    let hooks = Hooks()
    let id = GameID("No Socks RPG")
    let game = "No Socks RPG"

    /// `server` shares another setup's store, as a second device does. Without one, the store is a new local folder.
    /// On SMB, `remoteName` gives this device an rclone remote of its own, as another device has.
    init(saves: [String: String] = [:], metadata: GameMetadata? = nil, manifest: URL? = nil, otherGames: [String: String] = [:],
         server shared: TestServer? = nil, login: Login = .guest, remoteName: String? = nil) async throws {
        base = try temporaryDirectory()
        locations = try makeGame(in: base, saves: saves)
        library = LibraryStore(paths: LibraryPaths(gamesRoot: base.appendingPathComponent("Games"), support: base.appendingPathComponent("Support")),
                               engines: [RPGMakerPlugin(.mz)])
        await library.refresh()
        if let metadata { library.update(GameID("No Socks RPG")) { $0.metadata = metadata } }
        let server = if let shared { shared } else { try await TestServer.make(.local) }
        self.server = server
        // Other games already in the store, by title, each with one save at the given path.
        for (title, path) in otherGames {
            var mapping = LudusaviMapping(name: title, drives: ["drive-0": ""])
            mapping.backups = [FullBackup(name: "backup-1", when: Timestamp.epoch, os: "linux", locked: false,
                                          files: [path: FileEntry(hash: sha1("x"), size: 1)], registryHash: nil, children: [])]
            try await server.write(Data(mapping.yamlString().utf8), to: "\(LudusaviMapping.escapeFolderName(title))/mapping.yaml")
        }
        service = SaveSyncService(library: library, connector: Self.connector(hooks, server, library: library, remoteName: remoteName),
                                  manifest: manifest, configRetention: nil)
        let share = smbTest ?? (host: "server", port: SMBRemote.defaultPort)
        switch login {
        case .guest: try service.setServer(host: share.host, port: share.port, guest: true, username: "", password: "", path: server.path)
        case let .password(password):
            try service.setServer(host: share.host, port: share.port, guest: false, username: "player", password: password, path: server.path)
        }
        await service.testConnection()
        #expect(service.connection == .connected)
        await service.refresh()
        await service.manifestRefreshed()
    }

    /// The local store as it is, or for SMB the app's own connector, which reads the kept password. The hooks sit around
    /// every rc call either way.
    static func connector(_ hooks: Hooks, _ server: TestServer, library: LibraryStore, remoteName: String? = nil) -> SaveSyncService.Connector {
        let production = SaveSyncService.smbConnector(directory: library.paths.support.appendingPathComponent("SaveSync", isDirectory: true))
        return { settings in
            try await hooks.connect {
                switch server.kind {
                case .local: return RemoteAccess(root: server.root)
                case .smb:
                    if let remoteName { return try await RemoteAccess.smb(settings, password: smbPassword, name: remoteName) }
                    return try await production(settings)
                }
            }
        }
    }

    var mappingPath: String { "\(game)/mapping.yaml" }

    /// Ludusavi on the Steam Deck backing up its own saves of the game: it downloads the game folder, backs up into it,
    /// and syncs it back up through its own rclone remote, as Ludusavi does.
    func deckBacksUp(_ saves: [String: String], at hour: Double, comment: String? = nil) async throws {
        let deck = base.appendingPathComponent("Deck/NoSocksRPG", isDirectory: true)
        try fm.createDirectory(at: deck.appendingPathComponent("js"), withIntermediateDirectories: true)
        try Data().write(to: deck.appendingPathComponent("js/rmmz_core.js"))
        let deckLocations = RPGMakerPlugin(.mz).saveLocations(folder: deck, dataDirectory: base.appendingPathComponent("Deck/Data"))
        try? fm.removeItem(at: deckLocations[0].directory)
        try write(saves, to: deckLocations[0])
        let copy = try await server.download(game)
        _ = try LudusaviStore(root: copy).backUp(folder: game, title: game, game: id, locations: deckLocations,
                                                 retention: Retention(full: 3, differential: 2),
                                                 now: Date(timeIntervalSince1970: 1_800_000_000 + hour * 3600), os: "linux", comment: comment)
        try await server.upload(copy, game: game)
    }

    /// Ludusavi on the Steam Deck with full retention 1, which writes its one full backup into the game folder itself
    /// (`.`) and replaces it at each backup.
    func deckReplacesItsSoloBackup(_ saves: [String: String], at hour: Double) async throws {
        let copy = try await server.download(game)
        let gameDir = copy.appendingPathComponent(game, isDirectory: true)
        try? fm.removeItem(at: gameDir.appendingPathComponent("drive-0"))
        var files: [String: FileEntry] = [:]
        for (name, content) in saves {
            let url = gameDir.appendingPathComponent("drive-0" + deckSave + name)
            try fm.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
            try Data(content.utf8).write(to: url)
            files[deckSave + name] = FileEntry(hash: sha1(content), size: UInt64(content.utf8.count))
        }
        var mapping = LudusaviMapping(name: game, drives: ["drive-0": ""])
        mapping.backups = [FullBackup(name: ".", when: Timestamp.format(Date(timeIntervalSince1970: 1_800_000_000 + hour * 3600)), os: "linux",
                                      locked: false, files: files, registryHash: nil, children: [])]
        try Data(mapping.yamlString().utf8).write(to: gameDir.appendingPathComponent("mapping.yaml"))
        try await server.upload(copy, game: game)
    }

    /// The backup folders in the game folder on the server.
    func serverFolders() async throws -> Set<String> {
        try await server.names(in: game).filter { $0.hasPrefix("backup-") }
    }

    /// Fails unless every backup the server's mapping.yaml lists has every file, with the recorded hash, and, with
    /// `strays` false, no backup folder is left that it does not list.
    func expectStoreIsWhole(strays: Bool = false, sourceLocation: SourceLocation = #_sourceLocation) async throws {
        let copy = try await server.download(game)
        let mapping = try LudusaviStore(root: copy).mapping(game)
        let gameDir = copy.appendingPathComponent(game)
        for summary in mapping.summaries {
            for (path, stored) in mapping.effectiveFiles(summary.name) ?? [:] {
                let url = gameDir.appendingPathComponent(mapping.storedPath(of: path, in: stored.backup) ?? "")
                let hash = (try? Data(contentsOf: url)).map(LudusaviStore.sha1)
                #expect(hash == stored.entry.hash, "\(summary.name) \(path)", sourceLocation: sourceLocation)
            }
        }
        if !strays {
            #expect(try await serverFolders() == Set(mapping.summaries.map(\.name).filter { $0 != "." }), sourceLocation: sourceLocation)
        }
    }

    func serverMappingData() async throws -> Data { try #require(try await server.read(mappingPath)) }

    func serverMapping() async throws -> LudusaviMapping {
        try LudusaviMapping(yaml: String(decoding: try await serverMappingData(), as: UTF8.self))
    }
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
@Test func aFirstBackupGoesWhereTheDeckKeepsTheGame() async throws {
    let metadata = GameMetadata(title: "Something Else", refs: [ExternalRef(source: .steam, id: "1000002")])
    let setup = try await Setup(saves: ["file1.rmmzsave": "one"], metadata: metadata,
                                manifest: fixtures.appendingPathComponent("manifest-excerpt.yaml"),
                                otherGames: ["Test Game 02": "/home/deck/.local/share/Steam/steamapps/common/Test Game 02/save/file0.rmmzsave",
                                             "Test Game 04": "C:/Program Files (x86)/Steam/steamapps/common/Four/save/file0.rmmzsave",
                                             "Test Game 05": "/home/deck/.local/share/Steam/steamapps/common/Five/www/save/file1.rpgsave"])
    let match = try #require(setup.service.match(for: setup.id))
    #expect(match == GameMatch(folder: "Test Game 01", title: "Test Game 01", source: .steamID(1_000_002), inStore: false))
    #expect(setup.service.manifestRules(for: setup.id) == ["<base>/save/*.rmmzsave"])
    #expect(setup.service.foundGames == 3)

    let report = try await setup.service.checkedBackUp(setup.id)
    #expect(report.paths == [.pcFolder: 1])
    let store = try #require(setup.server.folder)
    let mapping = try LudusaviStore(root: store).mapping("Test Game 01")
    #expect(mapping.name == "Test Game 01")
    #expect(mapping.backups.first?.files.keys.sorted() == ["/home/deck/.local/share/Steam/steamapps/common/TestGame01/save/file1.rmmzsave"])
    // The Deck path runs through a hidden folder, and the bytes must reach the server under it.
    let backup = try #require(mapping.backups.first)
    for (path, entry) in backup.files {
        let stored = store.appendingPathComponent("Test Game 01").appendingPathComponent(try #require(mapping.storedPath(of: path, in: backup.name)))
        #expect(LudusaviStore.sha1(try Data(contentsOf: stored)) == entry.hash)
    }
    #expect(setup.service.match(for: setup.id)?.inStore == true)
}

@MainActor
@Test func storeGamesMatchByTitleAndTheUserCanPickAnother() async throws {
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

@MainActor
@Test func gameFoldersGoneFromTheServerLeaveTheMirror() async throws {
    let setup = try await Setup(otherGames: ["Other Game": "/x/save/file1.rmmzsave", "Test Game [Deluxe]": "/x/save/file2.rmmzsave"])
    #expect(Set(setup.service.storeGames.keys) == ["Other Game", "Test Game [Deluxe]"])
    try fm.removeItem(at: try #require(setup.server.folder).appendingPathComponent("Other Game"))
    await setup.service.refresh()
    #expect(Set(setup.service.storeGames.keys) == ["Test Game [Deluxe]"])
}

// MARK: - Sync

@MainActor
@Test(arguments: StoreKind.all) func backsUpRestoresAndTracksWhoChangedWhat(_ kind: StoreKind) async throws {
    try await TestServer.with(kind) { server in
        let setup = try await Setup(saves: ["file1.rmmzsave": "one", "global.rmmzsave": "g"], server: server)
        let service = setup.service, id = setup.id
        #expect(try await service.status(for: id) == .noBackups)

        let first = try await service.checkedBackUp(id)
        #expect(first.backup != nil && !first.isDifferential)
        #expect(try await service.status(for: id) == .inSync)
        #expect(try await setup.serverMapping().backups.count == 1)

        // Automatic backup only when it is on, and only of saves that changed here.
        try write(["file1.rmmzsave": "two"], to: setup.locations[0])
        #expect(try await service.status(for: id) == .localChanged)
        await service.sessionEnded(id)
        #expect(try await setup.serverMapping().summaries.count == 1)
        service.autoBackup = true
        await service.sessionEnded(id)
        #expect(try await setup.serverMapping().summaries.count == 2)
        #expect(try await service.status(for: id) == .inSync)

        // The Deck backs up newer saves. The game here did not change, so the newer backup is offered before launch.
        try await setup.deckBacksUp(["file1.rmmzsave": "deck", "global.rmmzsave": "g"], at: 1)
        let latest = try #require(try await setup.serverMapping().summaries.last)
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
        try await setup.deckBacksUp(["file1.rmmzsave": "deck 2", "global.rmmzsave": "g"], at: 2)
        let deck2 = try #require(try await setup.serverMapping().summaries.last)
        #expect(try await service.status(for: id) == .conflict(deck2))
        await service.sessionEnded(id)
        #expect(try await setup.serverMapping().summaries.last == deck2)

        // Keeping these saves backs them up on top at the next session end.
        service.keepLocalSaves(id, over: deck2)
        #expect(try await service.status(for: id) == .localChanged)
        await service.sessionEnded(id)
        #expect(try await setup.serverMapping().summaries.last?.os == "mac")
        #expect(try await service.status(for: id) == .inSync)

        // Ludusavi on the Mac or Deck can read every backup VNDegen wrote, with the files where mapping.yaml says.
        try await setup.expectStoreIsWhole()
    }
}

@MainActor
@Test func retentionDropsOldBackupsFromTheListAndLeavesTheirFolders() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "v0"])
    setup.service.manualRetention = Retention(full: 1, differential: 1)
    var pruned: [String] = []
    for version in 1 ... 4 {
        try write(["file1.rmmzsave": "v\(version)"], to: setup.locations[0])
        pruned += try await setup.service.checkedBackUp(setup.id).pruned
        try await Task.sleep(for: .seconds(1.1))
    }
    let mapping = try await setup.serverMapping()
    // With one full backup, Ludusavi keeps adding differentials and drops the oldest past the limit.
    #expect(mapping.backups.count == 1 && mapping.backups[0].children.count == 1)
    #expect(pruned.count == 2)
    // Ludusavi deletes the dropped folders at its next backup of the game. VNDegen leaves them.
    #expect(try await setup.serverFolders() == Set(mapping.summaries.map(\.name) + pruned))
    #expect(try contents(of: setup.locations[0]) == ["file1.rmmzsave": "v4"])
}

@MainActor
@Test func theServiceRunsOneOperationAtATime() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "one"])
    let judged = try await setup.service.check(setup.id).revision
    let attempts = (0 ..< 3).map { _ in Task { try await setup.service.backUp(setup.id, judged: judged) } }
    var made = 0, refused = 0
    for attempt in attempts {
        switch await attempt.result {
        case .success: made += 1
        case .failure(SaveSyncError.changedSinceCheck): refused += 1
        case let .failure(error): Issue.record(error)
        }
    }
    // The first one changes the server, so the others no longer start from the state that was checked.
    #expect(made == 1 && refused == 2)
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

// MARK: - Other devices at the same time

/// Locks the differential backup `name` in the mapping.yaml at `url`, as Ludusavi on another device would, and trims
/// that backup's comment so the file keeps its size. Then puts the modification time back: a change that size and
/// whole-second time can't show. Runs inside the service's operation, outside the test's task, so it throws rather
/// than records issues.
private func lockKeepingSizeAndTime(_ url: URL, backup name: String) throws {
    struct Unexpected: Error {}
    let original = try Data(contentsOf: url)
    guard let modified = try fm.attributesOfItem(atPath: url.path)[.modificationDate] as? Date else { throw Unexpected() }
    var mapping = try LudusaviMapping(yaml: String(decoding: original, as: UTF8.self))
    guard let full = mapping.backups.firstIndex(where: { $0.children.contains { $0.name == name } }),
          let diff = mapping.backups[full].children.firstIndex(where: { $0.name == name }),
          let comment = mapping.backups[full].children[diff].comment else { throw Unexpected() }
    mapping.backups[full].children[diff].locked = true
    let excess = mapping.yamlString().utf8.count - original.count
    mapping.backups[full].children[diff].comment = String(comment.dropLast(excess))
    let text = Data(mapping.yamlString().utf8)
    guard text.count == original.count, text != original else { throw Unexpected() }
    try text.write(to: url)
    try fm.setAttributes([.modificationDate: modified], ofItemAtPath: url.path)
}

@MainActor
@Test(arguments: StoreKind.all) func twoDevicesBackingUpInTheSameSecondNeverShareAFolder(_ kind: StoreKind) async throws {
    try await TestServer.with(kind) { server in
        let a = try await Setup(saves: ["file1.rmmzsave": "one"], server: server)
        let b = try await Setup(saves: ["file1.rmmzsave": "one"], server: server, remoteName: "device-b-\(UUID().uuidString.prefix(8))")
        _ = try await a.service.checkedBackUp(a.id)
        let second = Date(timeIntervalSince1970: 1_800_000_000)
        a.service.clock = { second }
        b.service.clock = { second }
        try write(["file1.rmmzsave": "from a"], to: a.locations[0])
        try write(["file1.rmmzsave": "from b, longer"], to: b.locations[0])

        // Device B backs up from the same mapping in the same second, while device A's folder is on the server and its
        // mapping.yaml is not yet.
        let other = b.service, otherID = b.id
        let made = Locked<String?>(nil)
        a.hooks.after("sync/copy") {
            let name = try await other.checkedBackUp(otherID).backup
            made { $0 = name }
        }
        await #expect(throws: SaveSyncError.changedDuringBackup) { try await a.service.checkedBackUp(a.id) }

        let mapping = try await a.serverMapping()
        let bName = try #require(made { $0 })
        #expect(mapping.summaries.last?.name == bName)
        #expect(bName.hasSuffix("-" + b.service.deviceID))
        try await a.expectStoreIsWhole()
        #expect(try await b.service.status(for: b.id) == .inSync)
    }
}

@MainActor
@Test func aChangeThatKeepsSizeAndTimeStopsTheBackupAndItsDeletions() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "v0"])
    setup.service.manualRetention = Retention(full: 1, differential: 1)
    _ = try await setup.service.checkedBackUp(setup.id)
    try await setup.deckBacksUp(["file1.rmmzsave": "deck"], at: 1, comment: String(repeating: "x", count: 40))
    let deckDiff = try #require(try await setup.serverMapping().summaries.last?.name)
    try write(["file1.rmmzsave": "v1"], to: setup.locations[0])

    // The Deck locks its backup, which this backup's retention would drop, while this device's folder goes up.
    let url = try #require(setup.server.folder).appendingPathComponent(setup.mappingPath)
    setup.hooks.after("sync/copy") { try lockKeepingSizeAndTime(url, backup: deckDiff) }
    await #expect(throws: SaveSyncError.changedDuringBackup) { try await setup.service.checkedBackUp(setup.id) }

    let mapping = try await setup.serverMapping()
    #expect(mapping.backups[0].children.map(\.name) == [deckDiff])
    #expect(mapping.backups[0].children[0].locked)
    #expect(try await setup.serverFolders().contains(deckDiff))
    try await setup.expectStoreIsWhole()
}

@MainActor
@Test(arguments: StoreKind.all) func aMappingReplacedRightAfterTheBackupKeepsEveryOldBackup(_ kind: StoreKind) async throws {
    try await TestServer.with(kind) { server in
        let setup = try await Setup(saves: ["file1.rmmzsave": "v0"], server: server)
        setup.service.manualRetention = Retention(full: 1, differential: 1)
        _ = try await setup.service.checkedBackUp(setup.id)
        try write(["file1.rmmzsave": "v1"], to: setup.locations[0])
        let first = try #require(try await setup.service.checkedBackUp(setup.id).backup)
        try write(["file1.rmmzsave": "v2"], to: setup.locations[0])
        try await Task.sleep(for: .seconds(1.1))

        // Another device sends the mapping it started from, which still lists `first`, just after this device's goes up.
        let stale = try await setup.serverMappingData(), path = setup.mappingPath
        setup.hooks.after("vndegen/replacefile") { try await server.write(stale, to: path) }
        await #expect(throws: SaveSyncError.changedAfterBackup) { try await setup.service.checkedBackUp(setup.id) }

        #expect(try await setup.serverMappingData() == stale)
        #expect(try await setup.serverFolders().contains(first))
        // The new folder stays too: the mapping might have named it. Ludusavi deletes it once none does.
        #expect(try await setup.serverFolders().count == 3)
        try await setup.expectStoreIsWhole(strays: true)
    }
}

@MainActor
@Test func aReplacedSoloBackupCountsAsNewer() async throws {
    let setup = try await Setup()
    try await setup.deckReplacesItsSoloBackup(["file1.rmmzsave": "deck 1"], at: 1)
    _ = try await setup.service.restore(setup.id, backup: nil)
    #expect(try await setup.service.status(for: setup.id) == .inSync)

    // The Deck replaces `.` with newer saves. The name stays the same.
    try await setup.deckReplacesItsSoloBackup(["file1.rmmzsave": "deck 2"], at: 2)
    let newer = try #require(try await setup.serverMapping().summaries.last)
    #expect(try await setup.service.status(for: setup.id) == .newerBackup(newer))
    _ = try await setup.service.restore(setup.id, backup: nil)
    #expect(try contents(of: setup.locations[0]) == ["file1.rmmzsave": "deck 2"])

    // With saves changed here too, it is a conflict, and automatic backup leaves the Deck's backup alone.
    try await setup.deckReplacesItsSoloBackup(["file1.rmmzsave": "deck 3"], at: 3)
    let newest = try #require(try await setup.serverMapping().summaries.last)
    try write(["file1.rmmzsave": "mine"], to: setup.locations[0])
    #expect(try await setup.service.status(for: setup.id) == .conflict(newest))
    setup.service.autoBackup = true
    await setup.service.sessionEnded(setup.id)
    #expect(try await setup.serverMapping().summaries == [newest])
}

@MainActor
@Test func aSyncRecordFromBeforeBackupTimesAsksInsteadOfBackingUp() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "one"])
    _ = try await setup.service.checkedBackUp(setup.id)
    try write(["file1.rmmzsave": "two"], to: setup.locations[0])

    // The record as VNDegen wrote it before: the latest backup by name only.
    let url = setup.library.paths.support.appendingPathComponent("SaveSync/settings.json")
    var settings = try #require(try JSONSerialization.jsonObject(with: Data(contentsOf: url)) as? [String: Any])
    var synced = try #require(settings["synced"] as? [String: [String: Any]])
    var record = try #require(synced[setup.id.rawValue])
    let latest = try #require(try await setup.serverMapping().summaries.last)
    record["latest"] = nil
    record["backup"] = latest.name
    synced[setup.id.rawValue] = record
    settings["synced"] = synced
    try JSONSerialization.data(withJSONObject: settings).write(to: url)

    let service = SaveSyncService(library: setup.library, connector: Setup.connector(setup.hooks, setup.server, library: setup.library),
                                 manifest: nil, configRetention: nil)
    #expect(try await service.status(for: setup.id) == .conflict(latest))
    service.autoBackup = true
    await service.sessionEnded(setup.id)
    #expect(try await setup.serverMapping().summaries == [latest])
}

@MainActor
@Test func anAutomaticBackupNeverLandsOnABackupItsCheckDidNotSee() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "one"])
    _ = try await setup.service.checkedBackUp(setup.id)
    try write(["file1.rmmzsave": "two"], to: setup.locations[0])
    setup.service.autoBackup = true

    // The Deck backs up after the check found only local changes, before the backup reaches the store.
    let opened = Locked<Int?>(nil)
    setup.hooks.beforeConnect = { index in
        let first = opened { start in
            if start == nil { start = index }
            return start!
        }
        if index == first + 1 { try await setup.deckBacksUp(["file1.rmmzsave": "deck"], at: 1) }
    }
    await setup.service.sessionEnded(setup.id)
    setup.hooks.beforeConnect = nil

    let latest = try #require(try await setup.serverMapping().summaries.last)
    #expect(latest.os == "linux")
    #expect(try await setup.service.status(for: setup.id) == .conflict(latest))
}

@MainActor
@Test func aDifferentialWithNothingToStoreGoesUp() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "v1", "file2.rmmzsave": "two"])
    setup.service.manualRetention = Retention(full: 1, differential: 2)
    _ = try await setup.service.checkedBackUp(setup.id)
    try write(["file1.rmmzsave": "v2"], to: setup.locations[0])
    _ = try await setup.service.checkedBackUp(setup.id)
    try await Task.sleep(for: .seconds(1.1))

    // Back to what the full backup holds, so the new differential inherits every file and stores none.
    try write(["file1.rmmzsave": "v1"], to: setup.locations[0])
    let report = try await setup.service.checkedBackUp(setup.id)
    let name = try #require(report.backup)
    #expect(report.isDifferential && report.filesWritten == 0)
    let diff = try #require(try await setup.serverMapping().backups.last?.children.last)
    #expect(diff.name == name && diff.files.isEmpty)
    #expect(try await setup.server.names(in: "\(setup.game)/\(name)").isEmpty)
    #expect(try await setup.serverFolders().contains(name))
    #expect(try await setup.service.status(for: setup.id) == .inSync)
    try await setup.expectStoreIsWhole()
}

extension SaveSyncService {
    /// A backup on top of what a status check just saw, as automatic backup and the game page do.
    func checkedBackUp(_ id: GameID) async throws -> BackupReport {
        try await backUp(id, judged: check(id).revision)
    }
}

@MainActor
@Test func anOldBackupStaysWhenAnotherDeviceStillListsIt() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "v0"])
    setup.service.manualRetention = Retention(full: 1, differential: 1)
    _ = try await setup.service.checkedBackUp(setup.id)
    try write(["file1.rmmzsave": "v1"], to: setup.locations[0])
    let first = try #require(try await setup.service.checkedBackUp(setup.id).backup)
    try write(["file1.rmmzsave": "v2"], to: setup.locations[0])
    try await Task.sleep(for: .seconds(1.1))
    let stale = try await setup.serverMappingData()

    // This backup drops `first` from the list. Another device then sends a mapping that still lists it.
    let report = try await setup.service.checkedBackUp(setup.id)
    #expect(report.pruned == [first])
    try await setup.server.write(stale, to: setup.mappingPath)

    #expect(try await setup.serverFolders().contains(first))
    try await setup.expectStoreIsWhole(strays: true)
}

@MainActor
@Test(arguments: StoreKind.all) func aMappingChangedJustBeforeItsCommitIsNeverOverwritten(_ kind: StoreKind) async throws {
    try await TestServer.with(kind) { server in
        let setup = try await Setup(saves: ["file1.rmmzsave": "one"], server: server)
        _ = try await setup.service.checkedBackUp(setup.id)
        try write(["file1.rmmzsave": "two"], to: setup.locations[0])
        let before = try await setup.serverFolders()

        // The Deck backs up right before this device replaces mapping.yaml.
        let deck = Locked<Data?>(nil)
        setup.hooks.before("vndegen/replacefile") {
            try await setup.deckBacksUp(["file1.rmmzsave": "deck"], at: 1)
            let written = try await setup.serverMappingData()
            deck { $0 = written }
        }
        await #expect(throws: SaveSyncError.changedDuringBackup) { try await setup.service.checkedBackUp(setup.id) }

        // The Deck's mapping.yaml stands byte for byte, with no temporary file beside it, and this device's unlisted
        // folder is gone. Every other folder stays.
        #expect(try await setup.serverMappingData() == deck { $0 })
        #expect(try await setup.server.names(in: setup.game).filter { $0.hasPrefix(".") }.isEmpty)
        let after = try await setup.serverFolders()
        #expect(after.isSuperset(of: before) && after.subtracting(before).count == 1)
        #expect(try await setup.serverMapping().summaries.last?.os == "linux")
        try await setup.expectStoreIsWhole()
    }
}

@MainActor
@Test func aCommitWithNoAnswerDeletesNothingAndTheNextOneReplacesItsTemporaryFile() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "one"])
    _ = try await setup.service.checkedBackUp(setup.id)
    try write(["file1.rmmzsave": "two"], to: setup.locations[0])
    let mapping = try await setup.serverMappingData(), folders = try await setup.serverFolders()
    let temporary = ".mapping.yaml.vndegen-\(setup.service.deviceID).tmp"

    // The replace call fails without an answer, so VNDegen can't tell whether the rename happened.
    struct NoAnswer: Error {}
    setup.hooks.before("vndegen/replacefile") { throw NoAnswer() }
    do {
        _ = try await setup.service.checkedBackUp(setup.id)
        Issue.record("the backup succeeded")
    } catch SaveSyncError.commitUnconfirmed {}
    #expect(try await setup.serverMappingData() == mapping)
    #expect(try await setup.server.names(in: setup.game).contains(temporary))
    let stray = try await setup.serverFolders().subtracting(folders)
    #expect(stray.count == 1)

    // The next commit replaces the temporary file the failed one left.
    try await Task.sleep(for: .seconds(1.1))
    try write(["file1.rmmzsave": "three"], to: setup.locations[0])
    let report = try await setup.service.checkedBackUp(setup.id)
    #expect(try await setup.serverMapping().summaries.last?.name == report.backup)
    #expect(try await !setup.server.names(in: setup.game).contains(temporary))
    #expect(try await setup.serverFolders().isSuperset(of: stray))
    try await setup.expectStoreIsWhole(strays: true)
}

@MainActor
@Test func theCommitNeverLeavesAMomentWithoutMappingYaml() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "one"])
    _ = try await setup.service.checkedBackUp(setup.id)
    try write(["file1.rmmzsave": "two"], to: setup.locations[0])
    let url = try #require(setup.server.folder).appendingPathComponent(setup.mappingPath)
    let old = try Data(contentsOf: url)

    // Another reader polls mapping.yaml for as long as the replace call runs.
    let stop = Locked(false), finished = Locked(false), seen = Locked<[Data?: Int]>([:])
    setup.hooks.before("vndegen/replacefile") {
        Thread.detachNewThread {
            while !stop({ $0 }) {
                let read = try? Data(contentsOf: url)
                seen { $0[read, default: 0] += 1 }
            }
            finished { $0 = true }
        }
        try await Task.sleep(for: .milliseconds(20))
    }
    setup.hooks.after("vndegen/replacefile") {
        try await Task.sleep(for: .milliseconds(20))
        stop { $0 = true }
    }
    _ = try await setup.service.checkedBackUp(setup.id)
    for _ in 0 ..< 200 where !finished({ $0 }) { try await Task.sleep(for: .milliseconds(10)) }

    let new = try Data(contentsOf: url)
    let reads = seen { $0 }
    #expect(reads[nil] == nil)
    #expect(Set(reads.keys) == [old, new])
}

@MainActor
@Test func backUpNowWaitsForAStatusCheck() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "one"])
    _ = try await setup.service.checkedBackUp(setup.id)
    try write(["file1.rmmzsave": "two"], to: setup.locations[0])

    // The page can't reach the server, so it never sees the Deck's backup that lands meanwhile.
    let model = GameSavesModel(service: setup.service, game: setup.id)
    let refusals = Locked(1)
    setup.hooks.beforeConnect = { _ in
        if refusals({ defer { $0 -= 1 }; return $0 > 0 }) { throw SaveSyncError.timedOut }
    }
    await model.load()
    try await setup.deckBacksUp(["file1.rmmzsave": "deck"], at: 1)

    model.select(.backUp)
    for _ in 0 ..< 100 where model.busy { try await Task.sleep(for: .milliseconds(50)) }
    #expect(try await setup.serverMapping().summaries.last?.os == "linux")
}

@MainActor
@Test func aGameIsInSyncAfterItsOwnBackupOrRestore() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "one"])
    // The Deck's backup holds a save this device never had.
    try await setup.deckBacksUp(["file1.rmmzsave": "one", "file2.rmmzsave": "deck only"], at: 1)
    let deck = try #require(try await setup.serverMapping().summaries.last)
    try write(["file1.rmmzsave": "two"], to: setup.locations[0])
    _ = try await setup.service.checkedBackUp(setup.id)
    #expect(try await setup.service.status(for: setup.id) == .inSync)

    // Restoring the older backup on purpose leaves the game in sync too, until a save changes.
    _ = try await setup.service.restore(setup.id, backup: deck.name)
    #expect(try contents(of: setup.locations[0]) == ["file1.rmmzsave": "one", "file2.rmmzsave": "deck only"])
    #expect(try await setup.service.status(for: setup.id) == .inSync)
    try write(["file1.rmmzsave": "three"], to: setup.locations[0])
    #expect(try await setup.service.status(for: setup.id) == .localChanged)
}

@MainActor
@Test func aSaveWrittenWhileItsBackupRunsStaysChanged() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "one"])
    let location = setup.locations[0]
    // The game writes a save right after the backup read and copied the saves.
    setup.service.afterLocalBackup = { try? write(["file1.rmmzsave": "two"], to: location) }
    _ = try await setup.service.checkedBackUp(setup.id)
    #expect(Set(try #require(try await setup.serverMapping().effectiveFiles()).values.map(\.entry.hash)) == [sha1("one")])
    #expect(try await setup.service.status(for: setup.id) == .localChanged)
}

@MainActor
@Test func keepingUnchangedSavesOverANewerBackupBacksThemUp() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "one"])
    let service = setup.service, id = setup.id
    _ = try await service.checkedBackUp(id)
    try await setup.deckBacksUp(["file1.rmmzsave": "deck"], at: 1)
    let deck = try #require(try await setup.serverMapping().summaries.last)
    #expect(try await service.status(for: id) == .newerBackup(deck))

    service.keepLocalSaves(id, over: deck)
    #expect(try await service.status(for: id) == .localChanged)
    service.autoBackup = true
    await service.sessionEnded(id)
    #expect(try await setup.serverMapping().summaries.last?.os == "mac")
    #expect(try await service.status(for: id) == .inSync)
}

@MainActor
@Test func keepingSavesOverABackupWithMoreSavesBacksUpOnceThenRests() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "one"])
    let service = setup.service, id = setup.id
    _ = try await service.checkedBackUp(id)
    // The Deck's newer backup has this device's save unchanged, plus one this device doesn't have.
    try await setup.deckBacksUp(["file1.rmmzsave": "one", "file2.rmmzsave": "deck only"], at: 1)
    let deck = try #require(try await setup.serverMapping().summaries.last)
    service.keepLocalSaves(id, over: deck)
    #expect(try await service.status(for: id) == .localChanged)
    service.autoBackup = true
    await service.sessionEnded(id)
    #expect(try await service.status(for: id) == .inSync)
    let count = try await setup.serverMapping().summaries.count
    await service.sessionEnded(id)
    #expect(try await setup.serverMapping().summaries.count == count)
}

// MARK: - A stalled server

@MainActor
@Test(.timeLimit(.minutes(2))) func aStalledServerHoldsUpNothingAfterIt() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "one"])
    let silent = try SilentServer()
    let idle = Duration.seconds(4), margin = Duration.seconds(12)
    setup.service.timeouts.rclone = RcloneLimits(connect: .seconds(4), idle: idle)
    // An operation limit far past the idle deadline, so only rclone's own deadline can end the call in time.
    setup.service.timeouts.operation = .seconds(100)
    let stalled = SaveServer(remote: .smb(SMBRemote(host: "127.0.0.1", port: silent.port)), path: "games/Ludusavi")
    func stall() async throws { setup.hooks.nextAccess = try await RemoteAccess.smb(stalled, password: nil) }

    // An operation on a share that never answers fails at the idle deadline, and the one queued behind it runs then.
    try await stall()
    var start = ContinuousClock.now
    let connections = setup.hooks.connections
    let first = Task { try await setup.service.status(for: setup.id) }
    for _ in 0 ..< 500 where setup.hooks.connections == connections { try await Task.sleep(for: .milliseconds(10)) }
    let queued = Task { try await setup.service.status(for: setup.id) }
    await #expect(throws: SaveSyncError.timedOut) { try await first.value }
    #expect(try await queued.value == .noBackups)
    #expect(ContinuousClock.now - start < idle + margin)

    // The check before launch gives up at its own limit and lets the game start. The backup queued behind it runs once
    // the check's call has ended.
    try await stall()
    setup.service.timeouts.launchCheck = .milliseconds(300)
    start = ContinuousClock.now
    #expect(await setup.service.launchOffer(setup.id) == nil)
    #expect(ContinuousClock.now - start < .seconds(3))
    #expect(try await setup.service.checkedBackUp(setup.id).backup != nil)
    #expect(ContinuousClock.now - start < idle + margin)

    // The operation limit cancels the call, which then ends at rclone's next step or the idle deadline.
    try await stall()
    setup.service.timeouts.operation = .seconds(1)
    start = ContinuousClock.now
    await #expect(throws: SaveSyncError.timedOut) { try await setup.service.status(for: setup.id) }
    #expect(ContinuousClock.now - start < idle + margin)
    setup.service.timeouts.operation = .seconds(100)
    #expect(try await setup.service.status(for: setup.id) == .inSync)
}

// MARK: - SMB logins

@MainActor
@Test(.enabled(if: smbTest != nil)) func logsInToTheShareAsGuestOrWithAPassword() async throws {
    try await TestServer.with(.smb) { server in
        let guest = try await Setup(saves: ["file1.rmmzsave": "one"], server: server)
        #expect(try await guest.service.checkedBackUp(guest.id).backup != nil)
        guard let smbPassword else { return }
        let player = try await Setup(saves: ["file1.rmmzsave": "two"], server: server, login: .password(smbPassword))
        #expect(player.service.storedPassword() == smbPassword)
        #expect(try await player.service.checkedBackUp(player.id).backup != nil)
        try await player.expectStoreIsWhole()

        let share = try #require(smbTest)
        try player.service.setServer(host: share.host, port: share.port, guest: false, username: "player", password: "not the password", path: server.path)
        await player.service.testConnection()
        #expect(player.service.connection == .failed(SaveSyncError.loginRefused.localizedDescription))
    }
}

@Test func aChangedLoginNamesANewRemote() async throws {
    let server = SaveServer(remote: .smb(SMBRemote(host: "nas.example", username: "player")), path: "/games/Ludusavi/")
    let first = try await RemoteAccess.smb(server, password: "one")
    #expect(first.root.hasPrefix("vndegen-") && first.root.hasSuffix(":games/Ludusavi"))
    #expect(try await RemoteAccess.smb(server, password: "one").root == first.root)
    #expect(try await RemoteAccess.smb(server, password: "two").root != first.root)
    var guest = server
    guest.remote = .smb(SMBRemote(host: "nas.example"))
    #expect(try await RemoteAccess.smb(guest, password: "ignored").root == RemoteAccess.smb(guest, password: nil).root)
}

@MainActor
@Test func aFirstBackupNeverCreatesAMissingStoreFolder() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "one"])
    // The path now names a folder the share does not have, as a typo in Settings would.
    let store = try #require(setup.server.folder)
    try FileManager.default.removeItem(at: store)
    await #expect(throws: SaveSyncError.noStoreFolder(setup.server.path)) { try await setup.service.checkedBackUp(setup.id) }
    #expect(!FileManager.default.fileExists(atPath: store.path))
}
