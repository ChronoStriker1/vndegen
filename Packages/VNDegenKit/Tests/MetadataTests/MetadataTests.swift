import Foundation
import Testing
import VNCore
@testable import Metadata

private let allKeys: [Credential: String] = [
    .steamGridDBKey: StubNetwork.steamGridDBKey, .igdbClientID: "client", .igdbClientSecret: "secret",
]

@MainActor
private func makeLibrary(_ games: [String]) async throws -> LibraryStore {
    let base = FileManager.default.temporaryDirectory.appending(path: UUID().uuidString)
    let paths = LibraryPaths(gamesRoot: base.appending(path: "Games"), support: base.appending(path: "Support"))
    for name in games {
        try FileManager.default.createDirectory(at: paths.folder(for: GameID(name)), withIntermediateDirectories: true)
    }
    let library = LibraryStore(paths: paths, engines: [])
    await library.refresh()
    return library
}

@MainActor
private func storedURL(_ slot: ArtworkSlot, _ id: GameID, _ library: LibraryStore) throws -> String? {
    try library.artworkURL(slot, for: id).map { String(decoding: try Data(contentsOf: $0), as: UTF8.self) }
}

@Test func titlesMatchIgnoringCaseAndPunctuation() {
    let candidate = { MetadataCandidate(ref: ExternalRef(source: .steam, id: "1"), title: $0) }
    #expect(TitleMatch.confidentMatch(in: [candidate("Doki Doki Literature Club!")], for: "doki-doki literature club") != nil)
    #expect(TitleMatch.confidentMatch(in: [candidate("No Socks RPG Demo")], for: "No Socks RPG") == nil)
    #expect(TitleMatch.confidentMatch(in: [candidate("Socks")], for: "No Socks RPG") == nil)
}

@Test @MainActor func searchRanksEveryConfiguredSourceByTitle() async {
    let network = StubNetwork()
    let service = MetadataService(credentials: MemoryCredentials(allKeys), http: network.http)

    let results = await service.search("Doki Doki Literature Club")

    #expect(results.first?.ref == ExternalRef(source: .vndb, id: "v21905", url: URL(string: "https://vndb.org/v21905")))
    #expect(results.first?.year == "2017")
    let exact = results.prefix { TitleMatch.normalize($0.title) == "doki doki literature club" }
    #expect(Set(exact.map(\.ref.source)) == [.vndb, .igdb, .steam, .steamGridDB])
    #expect(exact.contains { $0.ref.source == .steam && $0.ref.id == "698780" })
    #expect(exact.first { $0.ref.source == .igdb }?.ref.id == "55935")
    #expect(results.contains { $0.ref.id == "1388880" && $0.title == "Doki Doki Literature Club Plus!" })
}

@Test @MainActor func sourcesWithoutKeysAreSkipped() async {
    let network = StubNetwork()
    let service = MetadataService(credentials: MemoryCredentials(), http: network.http)

    #expect(service.activeSources == [.vndb, .steam])
    let results = await service.search("Doki Doki Literature Club")

    #expect(!results.isEmpty)
    #expect(network.requests(to: "www.steamgriddb.com").isEmpty)
    #expect(network.requests(to: "api.igdb.com").isEmpty)
    #expect(network.requests(to: "id.twitch.tv").isEmpty)
}

@Test @MainActor func applyMergesEverySourceAndFillsOnlyEmptySlotsBestSourceFirst() async throws {
    let network = StubNetwork()
    let service = MetadataService(credentials: MemoryCredentials(allKeys), http: network.http)
    let id = GameID("Doki Doki Literature Club")
    let library = try await makeLibrary([id.rawValue])
    try library.setArtwork(Data("mine".utf8), fileExtension: "png", slot: .icon, for: id)

    let vndb = MetadataCandidate(ref: ExternalRef(source: .vndb, id: "v21905"), title: "Doki Doki Literature Club!")
    try await service.apply(vndb, to: id, in: library)

    let game = try #require(library.game(id))
    let metadata = try #require(game.metadata)
    #expect(game.title == "Doki Doki Literature Club!")
    #expect(Dictionary(uniqueKeysWithValues: metadata.refs.map { ($0.source, $0.id) })
        == [.vndb: "v21905", .steam: "698780", .steamGridDB: "21043", .igdb: "55935"])
    #expect(metadata.summary?.hasPrefix("Hi, Monika here!") == true)
    #expect(metadata.summary?.contains("[url") == false)
    #expect(metadata.summary?.hasSuffix("[From Itch.io]") == true)
    #expect(metadata.developers == ["Team Salvato"])
    #expect(metadata.publishers.contains("Team Salvato"))
    #expect(metadata.releaseDate == "2017-09-22")
    #expect(metadata.rating == 69.1)

    #expect(try storedURL(.capsule, id, library) == "https://cdn2.steamgriddb.com/grid/66cff14a77fb713dc60adc123a8861bd.png")
    #expect(try storedURL(.hero, id, library)?.hasPrefix("https://cdn2.steamgriddb.com/hero/") == true)
    #expect(try storedURL(.logo, id, library)?.hasPrefix("https://cdn2.steamgriddb.com/logo/") == true)
    #expect(try storedURL(.header, id, library)
        == "https://shared.akamai.steamstatic.com/store_item_assets/steam/apps/698780/header.jpg?t=1681943582")
    #expect(try storedURL(.icon, id, library) == "mine")
}

@Test @MainActor func autoMatchAppliesOnlyAConfidentMatch() async throws {
    let network = StubNetwork()
    let service = MetadataService(credentials: MemoryCredentials(), http: network.http)
    let library = try await makeLibrary(["No Socks RPG", "Socks"])

    await service.autoMatch(GameID("No Socks RPG"), in: library)
    await service.autoMatch(GameID("Socks"), in: library)

    let game = try #require(library.game(GameID("No Socks RPG")))
    #expect(game.metadata?.refs.map(\.id) == ["4929970"])
    #expect(game.metadata?.developers == ["Spicyfuse"])
    #expect(game.metadata?.releaseDate == "2026-07-27")
    #expect(try storedURL(.capsule, game.id, library)?.contains("4929970/8ebdd91b3872955a97fc3cf7e79b035c75f95f74/library_capsule_2x.jpg") == true)
    #expect(try storedURL(.hero, game.id, library)?.contains("/library_hero_2x.jpg") == true)
    #expect(game.artwork[.logo] == nil, "Steam has no logo at the classic path for this app")
    #expect(library.game(GameID("Socks"))?.metadata == nil)
    #expect(library.game(GameID("Socks"))?.artwork.isEmpty == true)
}

@Test @MainActor func artworkCandidatesComeFromEverySourceThatKnowsTheGame() async throws {
    let network = StubNetwork()
    let service = MetadataService(credentials: MemoryCredentials([.steamGridDBKey: StubNetwork.steamGridDBKey]), http: network.http)
    var game = Game(id: GameID("ddlc"), title: "DDLC")
    game.metadata = GameMetadata(refs: [ExternalRef(source: .steam, id: "698780")])

    let capsules = await service.artworkCandidates(for: game, slot: .capsule)
    let icons = await service.artworkCandidates(for: game, slot: .icon)

    #expect(capsules.map(\.source) == [.steamGridDB, .steamGridDB, .steamGridDB, .steam])
    #expect(capsules.last?.url.absoluteString.contains("698780/library_600x900_2x.jpg") == true)
    #expect(icons.first?.url.pathExtension == "png", "an .ico icon is offered as its PNG render")
    #expect(network.requests(to: "www.steamgriddb.com").allSatisfy { !$0.url!.path.contains("search") },
            "a known Steam app ID finds the SteamGridDB game without a title search")
}

@Test @MainActor func igdbFetchesOneTokenAndReusesIt() async throws {
    let network = StubNetwork()
    let service = MetadataService(credentials: MemoryCredentials([.igdbClientID: "client", .igdbClientSecret: "secret"]),
                                  http: network.http)

    let first = await service.search("Doki Doki Literature Club")
    let second = await service.search("No Socks RPG")
    try await service.checkAccess(.igdb)

    #expect(first.contains { $0.ref.source == .igdb && $0.thumbnail?.absoluteString.hasSuffix("t_cover_small/co6p5e.jpg") == true })
    #expect(second.contains { $0.ref == ExternalRef(source: .igdb, id: "411478", url: nil) })
    #expect(network.requests(to: "id.twitch.tv").count == 2, "one token for searches, a fresh one for the access check")
    #expect(network.requests(to: "api.igdb.com").allSatisfy { $0.value(forHTTPHeaderField: "Client-ID") == "client" })
}

@Test @MainActor func savingKeysEnablesSourcesAndClearingDisablesThem() async throws {
    let credentials = MemoryCredentials()
    let service = MetadataService(credentials: credentials, http: StubNetwork().http)

    try service.setCredentials([.steamGridDBKey: "  \(StubNetwork.steamGridDBKey)\n"])
    #expect(service.activeSources == [.vndb, .steam, .steamGridDB])
    #expect(credentials.values[.steamGridDBKey] == StubNetwork.steamGridDBKey)
    try await service.checkAccess(.steamGridDB)

    try service.setCredentials([.steamGridDBKey: "wrong"])
    await #expect(throws: MetadataError.http(401)) { try await service.checkAccess(.steamGridDB) }

    try service.setCredentials([.steamGridDBKey: ""])
    #expect(service.activeSources == [.vndb, .steam])
    #expect(credentials.values[.steamGridDBKey] == nil)
}
