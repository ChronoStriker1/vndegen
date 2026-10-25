import Foundation
import ImageIO
import Testing
import VNCore
@testable import Metadata

/// Hits the real APIs. Runs only with VNDEGEN_LIVE=1. SteamGridDB and IGDB join when VNDEGEN_STEAMGRIDDB_KEY,
/// IGDB_CLIENT_ID, and IGDB_CLIENT_SECRET are set in the environment.
@Suite(.enabled(if: ProcessInfo.processInfo.environment["VNDEGEN_LIVE"] == "1"), .serialized)
@MainActor
struct LiveTests {
    let service: MetadataService
    let keyed: Bool

    init() {
        let env = ProcessInfo.processInfo.environment
        let keys: [Credential: String] = [
            .steamGridDBKey: env["VNDEGEN_STEAMGRIDDB_KEY"], .igdbClientID: env["IGDB_CLIENT_ID"], .igdbClientSecret: env["IGDB_CLIENT_SECRET"],
        ].compactMapValues { $0 }
        keyed = keys.count == Credential.allCases.count
        service = MetadataService(credentials: MemoryCredentials(keys), http: .live())
    }

    @Test func searchFindsBothGamesOnEverySource() async throws {
        print("LIVE active sources:", service.activeSources.map(\.displayName))
        let ddlc = await service.search("Doki Doki Literature Club")
        print("LIVE DDLC top hits:", ddlc.prefix(6).map { "\($0.ref.source.rawValue):\($0.ref.id) \($0.title)" })
        #expect(ddlc.contains { $0.ref.source == .vndb && $0.ref.id == "v21905" })
        #expect(ddlc.contains { $0.ref.source == .steam && $0.ref.id == "698780" })

        let socks = await service.search("No Socks RPG")
        print("LIVE No Socks top hits:", socks.prefix(6).map { "\($0.ref.source.rawValue):\($0.ref.id) \($0.title)" })
        #expect(socks.first.map { TitleMatch.normalize($0.title) } == "no socks rpg")
        #expect(socks.contains { $0.ref.source == .steam && $0.ref.id == "4929970" })

        if keyed {
            #expect(ddlc.contains { $0.ref.source == .igdb && $0.ref.id == "55935" })
            #expect(ddlc.contains { $0.ref.source == .steamGridDB && $0.ref.id == "21043" })
            #expect(socks.contains { $0.ref.source == .igdb && $0.ref.id == "411478" })
            #expect(socks.contains { $0.ref.source == .steamGridDB && $0.ref.id == "5519261" })
        }
    }

    @Test(arguments: ["No Socks RPG", "Doki Doki Literature Club"])
    func autoMatchStoresRealMetadataAndArt(title: String) async throws {
        let base = FileManager.default.temporaryDirectory.appending(path: UUID().uuidString)
        let paths = LibraryPaths(gamesRoot: base.appending(path: "Games"), support: base.appending(path: "Support"))
        try FileManager.default.createDirectory(at: paths.folder(for: GameID(title)), withIntermediateDirectories: true)
        let library = LibraryStore(paths: paths, engines: [])
        await library.refresh()

        await service.autoMatch(GameID(title), in: library)

        let game = try #require(library.game(GameID(title)))
        let metadata = try #require(game.metadata)
        print("LIVE \(title) refs:", metadata.refs.map { "\($0.source.rawValue):\($0.id)" }, "developers:", metadata.developers,
              "released:", metadata.releaseDate ?? "-")
        #expect(metadata.refs.contains { $0.source == .steam })
        let expected: Set<ArtworkSlot> = keyed ? Set(ArtworkSlot.allCases) : [.capsule, .hero, .header]
        for slot in ArtworkSlot.allCases {
            guard let url = library.artworkURL(slot, for: game.id) else { continue }
            let data = try Data(contentsOf: url)
            let source = try #require(CGImageSourceCreateWithData(data as CFData, nil))
            let image = try #require(CGImageSourceCreateImageAtIndex(source, 0, nil), "\(slot) decodes")
            print("LIVE \(title) \(slot.rawValue): \(url.lastPathComponent) \(data.count) bytes, \(image.width)x\(image.height) px")
        }
        #expect(expected.isSubset(of: Set(game.artwork.keys)), "filled \(game.artwork.keys.map(\.rawValue).sorted())")
    }

    @Test(.enabled(if: ProcessInfo.processInfo.environment["IGDB_CLIENT_SECRET"] != nil))
    func keysPassTheirAccessCheck() async throws {
        try await service.checkAccess(.steamGridDB)
        try await service.checkAccess(.igdb)
        print("LIVE SteamGridDB and IGDB access checks passed")
    }
}
