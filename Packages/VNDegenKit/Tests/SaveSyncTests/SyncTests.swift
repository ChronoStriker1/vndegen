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
    let hooks = Hooks()
    let id = GameID("No Socks RPG")

    /// `store` shares another setup's server, as a second device does.
    init(saves: [String: String] = [:], metadata: GameMetadata? = nil, manifest: URL? = nil, otherGames: [String: String] = [:],
         store shared: URL? = nil) async throws {
        base = try temporaryDirectory()
        locations = try makeGame(in: base, saves: saves)
        library = LibraryStore(paths: LibraryPaths(gamesRoot: base.appendingPathComponent("Games"), support: base.appendingPathComponent("Support")),
                               engines: [RPGMakerPlugin(.mz)])
        await library.refresh()
        if let metadata { library.update(GameID("No Socks RPG")) { $0.metadata = metadata } }
        store = shared ?? base.appendingPathComponent("server/Ludusavi", isDirectory: true)
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
        service = SaveSyncService(library: library, connector: Self.connector(hooks), manifest: manifest)
        service.setServer(host: "server", port: 22, user: "player", path: store.path)
        await service.testConnection()
        #expect(service.connection == .untrusted(hostKey: testHostKey))
        await service.trustHostKey()
        #expect(service.connection == .connected)
        await service.refresh()
        await service.manifestRefreshed()
    }

    static func connector(_ hooks: Hooks) -> SaveSyncService.Connector {
        { server in
            guard server.hostKey == testHostKey else { throw SSHError.unknownHostKey(testHostKey) }
            return try await hooks.connect()
        }
    }

    /// Ludusavi on the Steam Deck backing up its own saves of the game into the store.
    func deckBacksUp(_ saves: [String: String], at hour: Double, comment: String? = nil) throws {
        let deck = base.appendingPathComponent("Deck/NoSocksRPG", isDirectory: true)
        try fm.createDirectory(at: deck.appendingPathComponent("js"), withIntermediateDirectories: true)
        try Data().write(to: deck.appendingPathComponent("js/rmmz_core.js"))
        let deckLocations = RPGMakerPlugin(.mz).saveLocations(folder: deck, dataDirectory: base.appendingPathComponent("Deck/Data"))
        try? fm.removeItem(at: deckLocations[0].directory)
        try write(saves, to: deckLocations[0])
        _ = try LudusaviStore(root: store).backUp(folder: "No Socks RPG", title: "No Socks RPG", game: id, locations: deckLocations,
                                                  retention: Retention(full: 3, differential: 2),
                                                  now: Date(timeIntervalSince1970: 1_800_000_000 + hour * 3600), os: "linux", comment: comment)
    }

    /// Ludusavi on the Steam Deck with full retention 1, which writes its one full backup into the game folder itself
    /// (`.`) and replaces it at each backup.
    func deckReplacesItsSoloBackup(_ saves: [String: String], at hour: Double) throws {
        let gameDir = store.appendingPathComponent("No Socks RPG", isDirectory: true)
        try? fm.removeItem(at: gameDir.appendingPathComponent("drive-0"))
        var files: [String: FileEntry] = [:]
        for (name, content) in saves {
            let url = gameDir.appendingPathComponent("drive-0" + deckSave + name)
            try fm.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
            try Data(content.utf8).write(to: url)
            files[deckSave + name] = FileEntry(hash: sha1(content), size: UInt64(content.utf8.count))
        }
        var mapping = LudusaviMapping(name: "No Socks RPG", drives: ["drive-0": ""])
        mapping.backups = [FullBackup(name: ".", when: Timestamp.format(Date(timeIntervalSince1970: 1_800_000_000 + hour * 3600)), os: "linux",
                                      locked: false, files: files, registryHash: nil, children: [])]
        try Data(mapping.yamlString().utf8).write(to: gameDir.appendingPathComponent("mapping.yaml"))
    }

    var mappingURL: URL { store.appendingPathComponent("No Socks RPG/mapping.yaml") }

    /// The backup folders in the game folder on the server.
    func serverFolders() throws -> Set<String> {
        Set(try fm.contentsOfDirectory(atPath: store.appendingPathComponent("No Socks RPG").path).filter { $0.hasPrefix("backup-") })
    }

    /// Fails unless every backup the server's mapping.yaml lists has every file, with the recorded hash, and, with
    /// `strays` false, no backup folder is left that it does not list.
    func expectStoreIsWhole(strays: Bool = false, sourceLocation: SourceLocation = #_sourceLocation) throws {
        let mapping = try serverMapping()
        let gameDir = store.appendingPathComponent("No Socks RPG")
        for summary in mapping.summaries {
            for (path, stored) in mapping.effectiveFiles(summary.name) ?? [:] {
                let url = gameDir.appendingPathComponent(mapping.storedPath(of: path, in: stored.backup) ?? "")
                let hash = (try? Data(contentsOf: url)).map(LudusaviStore.sha1)
                #expect(hash == stored.entry.hash, "\(summary.name) \(path)", sourceLocation: sourceLocation)
            }
        }
        if !strays { #expect(try serverFolders() == Set(mapping.summaries.map(\.name).filter { $0 != "." }), sourceLocation: sourceLocation) }
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

// MARK: - Other devices at the same time

/// Lets a test act between the steps of an operation, the way another device would.
final class Hooks: @unchecked Sendable {
    private let lock = NSLock()
    private var command: (@Sendable ([String]) async throws -> Void)?
    private var connect: (@Sendable (Int) async throws -> Void)?
    private var next: (any RsyncConnection)?
    private var connections = 0

    /// Runs before each rsync command the service starts, with its arguments.
    var beforeCommand: (@Sendable ([String]) async throws -> Void)? {
        get { lock.withLock { command } }
        set { lock.withLock { command = newValue } }
    }

    /// Runs before each connection opens, with how many opened before it.
    var beforeConnect: (@Sendable (Int) async throws -> Void)? {
        get { lock.withLock { connect } }
        set { lock.withLock { connect = newValue } }
    }

    /// Used in place of the local rsync for the next connection only.
    var nextConnection: (any RsyncConnection)? {
        get { lock.withLock { next } }
        set { lock.withLock { next = newValue } }
    }

    func connect() async throws -> any RsyncConnection {
        let (hook, index, replacement) = lock.withLock {
            defer { connections += 1; next = nil }
            return (connect, connections, next)
        }
        try await hook?(index)
        return replacement ?? HookedConnection(hooks: self)
    }
}

private struct HookedConnection: RsyncConnection {
    let hooks: Hooks

    func start(_ arguments: [String]) async throws -> any RsyncTransport {
        try await hooks.beforeCommand?(arguments)
        return try await ProcessConnection().start(arguments)
    }

    func close() async {}
}

/// A server that accepts every command and then never sends a byte, until the connection is closed.
final class StallingConnection: RsyncConnection, @unchecked Sendable {
    private let lock = NSLock()
    private var streams: [AsyncThrowingStream<[UInt8], any Error>.Continuation] = []
    private var isClosed = false

    var closed: Bool { lock.withLock { isClosed } }

    func start(_ arguments: [String]) async throws -> any RsyncTransport {
        let (stream, continuation) = AsyncThrowingStream<[UInt8], any Error>.makeStream()
        if lock.withLock({ streams.append(continuation); return isClosed }) { continuation.finish() }
        return StalledTransport(output: stream)
    }

    func close() async {
        lock.withLock { isClosed = true; return streams }.forEach { $0.finish() }
    }

    private struct StalledTransport: RsyncTransport {
        let output: AsyncThrowingStream<[UInt8], any Error>
        func write(_ bytes: [UInt8]) async throws {}
        func finish() async throws -> (status: Int32, errorOutput: String) { (-1, "") }
    }
}

final class Locked<Value>: @unchecked Sendable {
    private let lock = NSLock()
    private var value: Value

    init(_ value: Value) { self.value = value }

    func callAsFunction<R>(_ body: (inout Value) -> R) -> R { lock.withLock { body(&value) } }
}

/// What a service's rsync command does: sends files (a push), reads the server, or deletes backups.
private enum Command {
    case push, read, delete

    init(_ arguments: [String]) {
        self = arguments.contains("--sender") ? .read : arguments.contains("--delete") ? .delete : .push
    }
}

/// Calls `action` once, at the first command after the `pushes`th push.
private func afterPush(_ pushes: Int, _ action: @escaping @Sendable () async throws -> Void) -> @Sendable ([String]) async throws -> Void {
    let seen = Locked(0), done = Locked(false)
    return { arguments in
        if Command(arguments) == .push, seen({ $0 += 1; return $0 }) <= pushes { return }
        guard seen({ $0 }) >= pushes, done({ defer { $0 = true }; return !$0 }) else { return }
        try await action()
    }
}

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
@Test(.enabled(if: rsyncPath != nil)) func twoDevicesBackingUpInTheSameSecondNeverShareAFolder() async throws {
    let a = try await Setup(saves: ["file1.rmmzsave": "one"])
    let b = try await Setup(saves: ["file1.rmmzsave": "one"], store: a.store)
    _ = try await a.service.backUp(a.id)
    let second = Date(timeIntervalSince1970: 1_800_000_000)
    a.service.clock = { second }
    b.service.clock = { second }
    try write(["file1.rmmzsave": "from a"], to: a.locations[0])
    try write(["file1.rmmzsave": "from b, longer"], to: b.locations[0])

    // Device B backs up from the same mapping in the same second, while device A's folder is on the server and
    // its mapping.yaml is not yet.
    let other = b.service, otherID = b.id
    let made = Locked<String?>(nil)
    a.hooks.beforeCommand = afterPush(1) {
        let name = try await other.backUp(otherID).backup
        made { $0 = name }
    }
    await #expect(throws: SaveSyncError.changedDuringBackup) { try await a.service.backUp(a.id) }

    let mapping = try a.serverMapping()
    let bName = try #require(made { $0 })
    #expect(mapping.summaries.last?.name == bName)
    #expect(bName.hasSuffix("-" + b.service.deviceID))
    try a.expectStoreIsWhole()
    #expect(try await b.service.status(for: b.id) == .inSync)
}

@MainActor
@Test(.enabled(if: rsyncPath != nil)) func aChangeThatKeepsSizeAndTimeStopsTheBackupAndItsDeletions() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "v0"])
    setup.service.manualRetention = Retention(full: 1, differential: 1)
    _ = try await setup.service.backUp(setup.id)
    try setup.deckBacksUp(["file1.rmmzsave": "deck"], at: 1, comment: String(repeating: "x", count: 40))
    let deckDiff = try #require(try setup.serverMapping().summaries.last?.name)
    try write(["file1.rmmzsave": "v1"], to: setup.locations[0])

    // The Deck locks its backup, which this backup's retention would drop, while this device's folder goes up.
    let url = setup.mappingURL
    setup.hooks.beforeCommand = afterPush(1) { try lockKeepingSizeAndTime(url, backup: deckDiff) }
    await #expect(throws: SaveSyncError.changedDuringBackup) { try await setup.service.backUp(setup.id) }

    let mapping = try setup.serverMapping()
    #expect(mapping.backups[0].children.map(\.name) == [deckDiff])
    #expect(mapping.backups[0].children[0].locked)
    #expect(try setup.serverFolders().contains(deckDiff))
    try setup.expectStoreIsWhole()
}

@MainActor
@Test(.enabled(if: rsyncPath != nil)) func aMappingReplacedRightAfterTheBackupKeepsEveryOldBackup() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "v0"])
    setup.service.manualRetention = Retention(full: 1, differential: 1)
    _ = try await setup.service.backUp(setup.id)
    try write(["file1.rmmzsave": "v1"], to: setup.locations[0])
    let first = try #require(try await setup.service.backUp(setup.id).backup)
    try write(["file1.rmmzsave": "v2"], to: setup.locations[0])
    try await Task.sleep(for: .seconds(1.1))

    // Another device sends the mapping it started from, which still lists `first`, just after this device's goes up.
    let url = setup.mappingURL, stale = try Data(contentsOf: url)
    setup.hooks.beforeCommand = afterPush(2) { try stale.write(to: url) }
    await #expect(throws: SaveSyncError.changedAfterBackup) { try await setup.service.backUp(setup.id) }

    #expect(try Data(contentsOf: url) == stale)
    #expect(try setup.serverFolders().contains(first))
    // The new folder stays too: the mapping might have named it. Ludusavi deletes it once none does.
    #expect(try setup.serverFolders().count == 3)
    try setup.expectStoreIsWhole(strays: true)
}

@MainActor
@Test(.enabled(if: rsyncPath != nil)) func aReplacedSoloBackupCountsAsNewer() async throws {
    let setup = try await Setup()
    try setup.deckReplacesItsSoloBackup(["file1.rmmzsave": "deck 1"], at: 1)
    _ = try await setup.service.restore(setup.id, backup: nil)
    #expect(try await setup.service.status(for: setup.id) == .inSync)

    // The Deck replaces `.` with newer saves. The name stays the same.
    try setup.deckReplacesItsSoloBackup(["file1.rmmzsave": "deck 2"], at: 2)
    let newer = try #require(try setup.serverMapping().summaries.last)
    #expect(try await setup.service.status(for: setup.id) == .newerBackup(newer))

    // With saves changed here too, it is a conflict, and automatic backup leaves the Deck's backup alone.
    try write(["file1.rmmzsave": "mine"], to: setup.locations[0])
    #expect(try await setup.service.status(for: setup.id) == .conflict(newer))
    setup.service.autoBackup = true
    await setup.service.sessionEnded(setup.id)
    #expect(try setup.serverMapping().summaries == [newer])
}

@MainActor
@Test(.enabled(if: rsyncPath != nil)) func aSyncRecordFromBeforeBackupTimesAsksInsteadOfBackingUp() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "one"])
    _ = try await setup.service.backUp(setup.id)
    try write(["file1.rmmzsave": "two"], to: setup.locations[0])

    // The record as VNDegen wrote it before: the latest backup by name only.
    let url = setup.library.paths.support.appendingPathComponent("SaveSync/settings.json")
    var settings = try #require(try JSONSerialization.jsonObject(with: Data(contentsOf: url)) as? [String: Any])
    var synced = try #require(settings["synced"] as? [String: [String: Any]])
    var record = try #require(synced[setup.id.rawValue])
    let latest = try #require(try setup.serverMapping().summaries.last)
    record["latest"] = nil
    record["backup"] = latest.name
    synced[setup.id.rawValue] = record
    settings["synced"] = synced
    try JSONSerialization.data(withJSONObject: settings).write(to: url)

    let service = SaveSyncService(library: setup.library, connector: Setup.connector(setup.hooks), manifest: nil)
    #expect(try await service.status(for: setup.id) == .conflict(latest))
    service.autoBackup = true
    await service.sessionEnded(setup.id)
    #expect(try setup.serverMapping().summaries == [latest])
}

@MainActor
@Test(.enabled(if: rsyncPath != nil)) func anAutomaticBackupNeverLandsOnABackupItsCheckDidNotSee() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "one"])
    _ = try await setup.service.backUp(setup.id)
    try write(["file1.rmmzsave": "two"], to: setup.locations[0])
    setup.service.autoBackup = true

    // The Deck backs up after the check found only local changes, before the backup opens its connection.
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

    let latest = try #require(try setup.serverMapping().summaries.last)
    #expect(latest.os == "linux")
    #expect(try await setup.service.status(for: setup.id) == .conflict(latest))
}

@MainActor
@Test(.enabled(if: rsyncPath != nil)) func aDifferentialWithNothingToStoreGoesUp() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "v1", "file2.rmmzsave": "two"])
    setup.service.manualRetention = Retention(full: 1, differential: 2)
    _ = try await setup.service.backUp(setup.id)
    try write(["file1.rmmzsave": "v2"], to: setup.locations[0])
    _ = try await setup.service.backUp(setup.id)
    try await Task.sleep(for: .seconds(1.1))

    // Back to what the full backup holds, so the new differential inherits every file and stores none.
    try write(["file1.rmmzsave": "v1"], to: setup.locations[0])
    let report = try await setup.service.backUp(setup.id)
    let name = try #require(report.backup)
    #expect(report.isDifferential && report.filesWritten == 0)
    let diff = try #require(try setup.serverMapping().backups.last?.children.last)
    #expect(diff.name == name && diff.files.isEmpty)
    #expect(try fm.contentsOfDirectory(atPath: setup.store.appendingPathComponent("No Socks RPG/\(name)").path).isEmpty)
    #expect(try await setup.service.status(for: setup.id) == .inSync)
    try setup.expectStoreIsWhole()
}

@MainActor
@Test(.enabled(if: rsyncPath != nil), .timeLimit(.minutes(1))) func aStalledServerHoldsUpNothingAfterIt() async throws {
    let setup = try await Setup(saves: ["file1.rmmzsave": "one"])
    setup.service.timeouts.launchCheck = .milliseconds(500)
    setup.service.timeouts.operation = .seconds(2)

    // The check before launch gives up, and its connection closes, so the backup after it runs at once.
    let stalled = StallingConnection()
    setup.hooks.nextConnection = stalled
    let start = ContinuousClock.now
    #expect(await setup.service.launchOffer(setup.id) == nil)
    #expect(try await setup.service.backUp(setup.id).backup != nil)
    #expect(stalled.closed)
    #expect(ContinuousClock.now - start < .seconds(10))

    // Any other operation ends at its deadline, and the next one runs.
    setup.hooks.nextConnection = StallingConnection()
    await #expect(throws: SaveSyncError.timedOut) { try await setup.service.status(for: setup.id) }
    #expect(try await setup.service.status(for: setup.id) == .inSync)
}

@Test func onlyUnlockedUnlistedBackupsAreDeleted() {
    func full(_ name: String, locked: Bool = false, _ children: [(String, Bool)] = []) -> FullBackup {
        FullBackup(name: name, when: Timestamp.epoch, locked: locked, files: [:], registryHash: nil,
                   children: children.map { DifferentialBackup(name: $0.0, when: Timestamp.epoch, locked: $0.1, files: [:], registry: nil) })
    }
    var base = LudusaviMapping(name: "G", drives: ["drive-0": ""])
    base.backups = [full(".", [("d0", false)]), full("f1", [("d1", true), ("d2", false)]), full("f2", locked: true)]
    var current = base
    current.backups = [full("f1", [("d1", true)]), full("f3", [("d2", false)])]
    let pruned = ["drive-0", "d0", "f1", "d1", "d2", "f2", "gone"]
    // f1 has a locked differential, so it counts as locked. d2 is still listed. f2 was locked.
    #expect(LudusaviMapping.deletable(pruned, base: base, current: current) == ["drive-0", "d0", "gone"])
    current.backups.append(full("."))
    #expect(LudusaviMapping.deletable(pruned, base: nil, current: current) == ["d0", "f2", "gone"])
}
