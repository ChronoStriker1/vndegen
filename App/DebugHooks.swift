#if DEBUG
import Foundation
import VNCore
import HubUI
#if canImport(UIKit)
import UIKit
#endif

/// Launch arguments for screenshots and UI tests. None of this ships in release builds.
///   -seedDemoLibrary YES     creates demo games with real Steam library art
///   -demoInput "down,right"  plays controller presses after launch, 0.6 s apart ("wait" pauses one step)
///   -forceOrientation landscape   rotates the window scene, for simulators that have no Simulator app to rotate them
enum DebugHooks {
    static func run(_ hub: Hub) async {
        #if canImport(UIKit)
        if UserDefaults.standard.string(forKey: "forceOrientation") == "landscape",
           let scene = UIApplication.shared.connectedScenes.first as? UIWindowScene {
            scene.requestGeometryUpdate(.iOS(interfaceOrientations: .landscapeRight))
        }
        #endif
        if UserDefaults.standard.bool(forKey: "seedDemoLibrary") {
            await DemoLibrary.seed(hub.library)
        }
    }

    static func playInput(_ hub: Hub) async {
        guard let script = UserDefaults.standard.string(forKey: "demoInput") else { return }
        try? await Task.sleep(for: .seconds(1.5))
        hub.input.noteInput(.gamepad)
        for step in script.split(separator: ",").map({ $0.trimmingCharacters(in: .whitespaces) }) {
            if let button = VirtualButton(rawValue: step) {
                hub.input.onEvent?(.button(button, pressed: true))
                hub.input.onEvent?(.button(button, pressed: false))
            }
            try? await Task.sleep(for: .seconds(0.6))
        }
    }
}

/// Fake game folders that detect as RPG Maker MZ, dressed with real Steam library art and metadata, so the hub can be
/// judged with real artwork before the metadata providers exist. Safe to run on every launch: it only fills what is missing.
enum DemoLibrary {
    struct Entry {
        let folder: String
        let appID: Int
        let metadata: GameMetadata
        var playedDaysAgo: Double?
        var playTime: TimeInterval = 0
        var locked = false
    }

    static let entries: [Entry] = [
        Entry(folder: "OMORI", appID: 1150690, metadata: GameMetadata(
            title: "OMORI",
            summary: "Explore a strange world full of colorful friends and foes. When the time comes, the path you've chosen will determine your fate... and perhaps the fate of others as well.",
            developers: ["OMOCAT"], publishers: ["OMOCAT"], releaseDate: "2020-12-25",
            genres: ["RPG", "Psychological Horror", "Adventure"]), playedDaysAgo: 0.1, playTime: 41 * 3600 + 22 * 60),
        Entry(folder: "Steins;Gate", appID: 412830, metadata: GameMetadata(
            title: "STEINS;GATE",
            summary: "Rintaro Okabe, a self-proclaimed mad scientist, discovers that his microwave can send text messages to the past, and every message changes the world line.",
            developers: ["MAGES.", "5pb."], publishers: ["Spike Chunsoft"], releaseDate: "2016-09-09",
            genres: ["Visual Novel", "Sci-fi", "Thriller"]), playedDaysAgo: 1.2, playTime: 18 * 3600 + 5 * 60),
        Entry(folder: "VA-11 Hall-A", appID: 447530, metadata: GameMetadata(
            title: "VA-11 Hall-A: Cyberpunk Bartender Action",
            summary: "Mix drinks and change lives at a dive bar in a dystopian cyberpunk city, one conversation at a time.",
            developers: ["Sukeban Games"], publishers: ["Ysbryd Games"], releaseDate: "2016-06-21",
            genres: ["Visual Novel", "Cyberpunk", "Simulation"]), playedDaysAgo: 5, playTime: 9 * 3600 + 40 * 60),
        Entry(folder: "CLANNAD", appID: 324160, metadata: GameMetadata(
            title: "CLANNAD",
            summary: "Tomoya Okazaki is a delinquent who finds life dull, until he meets a girl repeating her final year at his school.",
            developers: ["Key", "VisualArts"], publishers: ["Sekai Project"], releaseDate: "2015-11-23",
            genres: ["Visual Novel", "Drama", "Romance"])),
        Entry(folder: "Rakuen", appID: 559210, metadata: GameMetadata(
            title: "Rakuen",
            summary: "A boy and his mom journey into the fantasy world of his favorite storybook to help the neighbors of a hospital ward.",
            developers: ["Laura Shigihara"], publishers: ["Leaf Mobile"], releaseDate: "2017-05-10",
            genres: ["Adventure", "RPG Maker", "Puzzle"])),
        Entry(folder: "To the Moon", appID: 206440, metadata: GameMetadata(
            title: "To the Moon",
            summary: "Two doctors travel through a dying man's memories to grant his last wish: to go to the moon.",
            developers: ["Freebird Games"], publishers: ["Freebird Games"], releaseDate: "2011-11-01",
            genres: ["Adventure", "RPG Maker", "Story Rich"])),
        Entry(folder: "The House in Fata Morgana", appID: 303310, metadata: GameMetadata(
            title: "The House in Fata Morgana",
            summary: "A gothic tale of a cursed mansion and the tragedies that unfold within its walls across a thousand years.",
            developers: ["Novectacle"], publishers: ["MangaGamer"], releaseDate: "2014-07-17",
            genres: ["Visual Novel", "Gothic", "Mystery"])),
        Entry(folder: "Zero Escape", appID: 477740, metadata: GameMetadata(
            title: "Zero Escape: The Nonary Games",
            summary: "Nine people are kidnapped and forced to play a deadly game of escape. Includes 999 and Virtue's Last Reward.",
            developers: ["Spike Chunsoft"], publishers: ["Spike Chunsoft"], releaseDate: "2017-03-24",
            genres: ["Visual Novel", "Puzzle", "Mystery"])),
        Entry(folder: "Higurashi", appID: 310360, metadata: GameMetadata(
            title: "Higurashi When They Cry",
            summary: "A quiet village, a summer festival, and a curse that returns every year.",
            developers: ["07th Expansion"], publishers: ["MangaGamer"], releaseDate: "2015-05-15",
            genres: ["Visual Novel", "Horror", "Mystery"])),
        Entry(folder: "Doki Doki Literature Club", appID: 698780, metadata: GameMetadata(
            title: "Doki Doki Literature Club!",
            summary: "Join the Literature Club and write poems for the girls you like. This game is not suitable for children or those easily disturbed.",
            developers: ["Team Salvato"], publishers: ["Team Salvato"], releaseDate: "2017-09-22",
            genres: ["Visual Novel", "Psychological Horror"]), playedDaysAgo: 12, playTime: 4 * 3600, locked: true),
    ]

    static let files: [(ArtworkSlot, String)] = [
        (.capsule, "library_600x900_2x.jpg"), (.hero, "library_hero.jpg"), (.logo, "logo.png"), (.header, "header.jpg"),
    ]

    @MainActor
    static func seed(_ library: LibraryStore) async {
        let fm = FileManager.default
        for entry in entries {
            let marker = library.paths.folder(for: GameID(entry.folder)).appendingPathComponent("js/rmmz_core.js")
            if !fm.fileExists(atPath: marker.path) {
                try? fm.createDirectory(at: marker.deletingLastPathComponent(), withIntermediateDirectories: true)
                fm.createFile(atPath: marker.path, contents: Data("// demo".utf8))
            }
        }
        await library.refresh()

        for entry in entries {
            let id = GameID(entry.folder)
            library.update(id) { game in
                game.metadata = entry.metadata
                game.isLocked = entry.locked
                if game.lastPlayedAt == nil, let days = entry.playedDaysAgo {
                    game.lastPlayedAt = .now.addingTimeInterval(-days * 86_400)
                    game.playTime = entry.playTime
                }
            }
        }

        let missing: [(entry: Entry, slot: ArtworkSlot, file: String)] = entries.flatMap { entry in
            files.filter { library.game(GameID(entry.folder))?.artwork[$0.0] == nil }.map { (entry, $0.0, $0.1) }
        }
        let downloads = await withTaskGroup(of: (GameID, ArtworkSlot, String, Data)?.self) { group in
            for (entry, slot, file) in missing {
                group.addTask {
                    let url = URL(string: "https://cdn.cloudflare.steamstatic.com/steam/apps/\(entry.appID)/\(file)")!
                    guard let (data, response) = try? await URLSession.shared.data(from: url),
                          (response as? HTTPURLResponse)?.statusCode == 200 else { return nil }
                    return (GameID(entry.folder), slot, (file as NSString).pathExtension, data)
                }
            }
            var results: [(GameID, ArtworkSlot, String, Data)] = []
            for await result in group {
                if let result { results.append(result) }
            }
            return results
        }
        for (id, slot, ext, data) in downloads {
            try? library.setArtwork(data, fileExtension: ext, slot: slot, for: id)
        }
    }
}
#endif
