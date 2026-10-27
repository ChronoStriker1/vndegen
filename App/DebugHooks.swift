#if DEBUG
import Foundation
import VNCore
import GameInput
import HubUI
import EngineRPGMaker
import notify
#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

/// Launch arguments for screenshots and UI tests. None of this ships in release builds.
///   -debugHooks <name>       scopes every notification below to this app, as vndegen.<name>.input.down and so on, so
///                            scripts driving one DEBUG VNDegen never press or quit another on the same Mac. Without it
///                            the names are as written below
///   -logNavigation YES       logs the hub's route, sheet, focus, and on-screen keyboard after each controller press
///                            ("HUBNAV" in the app log)
///   -seedDemoLibrary YES     creates demo games with real Steam library art
///   -demoInput "down,right"  plays controller presses after launch, 0.6 s apart ("wait" pauses one step)
///   notifyutil -p vndegen.input.down   presses one controller button while the app runs, through
///                            `xcrun simctl spawn <device> notifyutil -p ...` on a simulator
///   notifyutil -p vndegen.touch.l2.down   presses a touch gamepad button, and .up releases it, as a finger would
///   notifyutil -s vndegen.pointer.tap 512384 -p vndegen.pointer.tap   taps the running game's view, as a click or touch
///                            on it would. The state's last six digits are x then y in thousandths of the view's width
///                            and height from its top left, here x 0.512 and y 0.384
///   notifyutil -p vndegen.pad.a.down   presses a real controller's button (a GamepadElement), and .up releases it
///   notifyutil -s vndegen.stick.left 20001000 -p vndegen.stick.left   holds a controller's left stick, or .right, at x
///                            then y, each four digits of 1000 plus a thousandth of a full push, here x 1 and y 0. The
///                            stick stays there until the next one, such as 10001000 to let go
///   notifyutil -p vndegen.app.quit   quits the app as Command-Q does, macOS only
///   notifyutil -p vndegen.app.snapshot   writes the main window to the -snapshot path now, macOS only
///   -forceOrientation landscape   rotates the window scene, for simulators that have no Simulator app to rotate them
///   -fakeSession YES         RPG Maker MZ games start a placeholder session instead of the real runtime
///   -frameMonitor YES        logs frame pacing once a second ("FRAMES" in the app log), iOS only
///   -snapshot <path>         writes the main window to a PNG after the input script, macOS only (screencapture needs a login session)
///   -windowSize 900x600      resizes the window before the snapshot
enum DebugHooks {
    static func run(_ hub: Hub) async {
        listenForInput(hub)
        #if canImport(UIKit)
        if UserDefaults.standard.bool(forKey: "frameMonitor") { FrameMonitor.shared.start() }
        #endif
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

    /// Every notification name starts with this, so `-debugHooks <name>` keeps scripts for one app away from another.
    static let prefix = UserDefaults.standard.string(forKey: "debugHooks").map { "vndegen.\($0)" } ?? "vndegen"

    /// Registers one Darwin notification per button. The tokens live for the app's lifetime.
    static func listenForInput(_ hub: Hub) {
        for input in ControlInput.allCases where input.isOnTouchGamepad {
            for pressed in [true, false] {
                var token: Int32 = 0
                notify_register_dispatch("\(prefix).touch.\(input.rawValue).\(pressed ? "down" : "up")", &token, .main) { _ in
                    MainActor.assumeIsolated { hub.input.touch(input, pressed: pressed) }
                }
            }
        }
        for element in GamepadElement.allCases {
            for pressed in [true, false] {
                var token: Int32 = 0
                notify_register_dispatch("\(prefix).pad.\(element.rawValue).\(pressed ? "down" : "up")", &token, .main) { _ in
                    MainActor.assumeIsolated { hub.input.debugPress(element, pressed: pressed) }
                }
            }
        }
        for stick in Stick.allCases {
            var token: Int32 = 0
            notify_register_dispatch("\(prefix).stick.\(stick == .left ? "left" : "right")", &token, .main) { token in
                var state: UInt64 = 0
                notify_get_state(token, &state)
                let x = Float(Int(state / 10000 % 10000) - 1000) / 1000, y = Float(Int(state % 10000) - 1000) / 1000
                MainActor.assumeIsolated { hub.input.debugStick(stick, x: x, y: y) }
            }
        }
        var tapToken: Int32 = 0
        notify_register_dispatch("\(prefix).pointer.tap", &tapToken, .main) { token in
            var state: UInt64 = 0
            notify_get_state(token, &state)
            let point = CGPoint(x: Double(state / 1000 % 1000) / 1000, y: Double(state % 1000) / 1000)
            MainActor.assumeIsolated { tap(hub, at: point) }
        }
        #if os(macOS)
        var quitToken: Int32 = 0
        notify_register_dispatch("\(prefix).app.quit", &quitToken, .main) { _ in
            // From the run loop, as Command-Q's key event arrives. AppKit waits for the reply to a quit inside
            // `terminate`, and called from this block it would hold the main queue, where the reply comes from.
            RunLoop.main.perform(inModes: [.common]) { MainActor.assumeIsolated { NSApp.terminate(nil) } }
        }
        var snapshotToken: Int32 = 0
        notify_register_dispatch("\(prefix).app.snapshot", &snapshotToken, .main) { _ in
            guard let path = UserDefaults.standard.string(forKey: "snapshot") else { return }
            MainActor.assumeIsolated { snapshotWindow(to: URL(fileURLWithPath: path)) }
        }
        #endif
        for button in VirtualButton.allCases {
            var token: Int32 = 0
            notify_register_dispatch("\(prefix).input.\(button.rawValue)", &token, .main) { _ in
                MainActor.assumeIsolated {
                    hub.input.noteInput(.gamepad)
                    hub.input.onEvent?(.button(button, pressed: true))
                    hub.input.onEvent?(.button(button, pressed: false))
                }
            }
        }
    }

    /// Sends the pointer events the game's own view sends for a click, so a verifier can work a game's menus where
    /// nothing can click in the app, such as a Mac reached over SSH. The pointer rests over the point for 0.5 s before
    /// it presses, as a real mouse does, since some games' buttons take a press only after the pointer has been over
    /// them a moment.
    static func tap(_ hub: Hub, at point: CGPoint) {
        guard case let .playing(_, session) = hub.screen else { return }
        Task {
            session.send(.pointer(PointerEvent(phase: .moved, location: point)))
            try? await Task.sleep(for: .milliseconds(500))
            session.send(.pointer(PointerEvent(phase: .began, location: point)))
            try? await Task.sleep(for: .milliseconds(100))
            session.send(.pointer(PointerEvent(phase: .ended, location: point)))
        }
    }

    static func playInput(_ hub: Hub) async {
        let script = UserDefaults.standard.string(forKey: "demoInput") ?? ""
        try? await Task.sleep(for: .seconds(1.5))
        if !script.isEmpty { hub.input.noteInput(.gamepad) }
        for step in script.split(separator: ",").map({ $0.trimmingCharacters(in: .whitespaces) }) {
            if let button = VirtualButton(rawValue: step) {
                hub.input.onEvent?(.button(button, pressed: true))
                hub.input.onEvent?(.button(button, pressed: false))
            }
            try? await Task.sleep(for: .seconds(0.6))
        }
        #if os(macOS)
        if let path = UserDefaults.standard.string(forKey: "snapshot") {
            if let size = UserDefaults.standard.string(forKey: "windowSize")?.split(separator: "x").compactMap({ Double($0) }),
               size.count == 2, let window = NSApp.windows.first(where: \.isVisible) {
                window.setContentSize(NSSize(width: size[0], height: size[1]))
            }
            try? await Task.sleep(for: .seconds(3))
            snapshotWindow(to: URL(fileURLWithPath: path))
        }
        #endif
    }

    #if os(macOS)
    /// The main window, or the sheet shown over it, and the sheet over that one.
    static func snapshotWindow(to url: URL) {
        var window = NSApp.windows.first(where: \.isVisible)
        while let sheet = window?.attachedSheet { window = sheet }
        guard let view = window?.contentView,
              let rep = view.bitmapImageRepForCachingDisplay(in: view.bounds) else { return }
        view.cacheDisplay(in: view.bounds, to: rep)
        try? rep.representation(using: .png, properties: [:])?.write(to: url)
    }
    #endif
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
        brief("Danganronpa", 413410, "Danganronpa: Trigger Happy Havoc", "Spike Chunsoft", "2016-02-18", ["Visual Novel", "Mystery"]),
        brief("Ace Attorney Trilogy", 787480, "Phoenix Wright: Ace Attorney Trilogy", "Capcom", "2019-04-09", ["Visual Novel", "Courtroom"]),
        brief("CHAOS;CHILD", 970570, "CHAOS;CHILD", "MAGES.", "2019-02-26", ["Visual Novel", "Thriller"]),
        brief("Muv-Luv", 802880, "Muv-Luv", "âge", "2016-12-07", ["Visual Novel", "Sci-fi"]),
        brief("Coffee Talk", 914800, "Coffee Talk", "Toge Productions", "2020-01-29", ["Visual Novel", "Cozy"]),
        brief("Ib", 1901370, "Ib", "kouri", "2022-04-11", ["Horror", "RPG Maker"]),
        brief("Undertale", 391540, "Undertale", "tobyfox", "2015-09-15", ["RPG", "Bullet Hell"]),
        brief("Yume Nikki", 650700, "Yume Nikki", "Kikiyama", "2018-02-23", ["Exploration", "RPG Maker"]),
        brief("LISA", 335670, "LISA: The Painful", "Dingaling Productions", "2014-12-15", ["RPG", "Post-apocalyptic"]),
        brief("Corpse Party", 251270, "Corpse Party", "Team GrisGris", "2016-04-25", ["Horror", "RPG Maker"]),
        brief("The Witch's House MV", 885810, "The Witch's House MV", "Fummy", "2018-10-26", ["Horror", "Puzzle"]),
        brief("Mad Father", 483980, "Mad Father", "sen", "2016-05-27", ["Horror", "RPG Maker"]),
        brief("Needy Streamer Overload", 1451940, "NEEDY STREAMER OVERLOAD", "WSS playground", "2022-01-21", ["Visual Novel", "Simulation"]),
        brief("AI The Somnium Files", 948740, "AI: The Somnium Files", "Spike Chunsoft", "2019-09-17", ["Visual Novel", "Detective"]),
        brief("planetarian", 316720, "planetarian ~the reverie of a little planet~", "Key", "2014-09-15", ["Visual Novel", "Kinetic Novel"]),
        Entry(folder: "Doki Doki Literature Club", appID: 698780, metadata: GameMetadata(
            title: "Doki Doki Literature Club!",
            summary: "Join the Literature Club and write poems for the girls you like. This game is not suitable for children or those easily disturbed.",
            developers: ["Team Salvato"], publishers: ["Team Salvato"], releaseDate: "2017-09-22",
            genres: ["Visual Novel", "Psychological Horror"]), playedDaysAgo: 12, playTime: 4 * 3600, locked: true),
    ]

    private static func brief(_ folder: String, _ appID: Int, _ title: String, _ developer: String, _ release: String, _ genres: [String]) -> Entry {
        Entry(folder: folder, appID: appID, metadata: GameMetadata(title: title, developers: [developer], releaseDate: release, genres: genres))
    }

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
/// Stands in for the RPG Maker MZ runtime so the player screen, its overlay, and orientation can be tested before the
/// real runtime exists. It shows the game title, the view's shape, and the last controller event.
nonisolated struct FakeSessionPlugin: EnginePlugin {
    var descriptor: EngineDescriptor {
        EngineDescriptor(id: .rpgMakerMZ, displayName: "RPG Maker MZ (fake session)", shortName: "MZ", platforms: [.macOS, .iOS])
    }

    func detect(_ probe: FolderProbe) -> EngineMatch? { RPGMakerPlugin(.mz).detect(probe) }

    @MainActor func makeSession(_ launch: GameLaunch) throws -> any EngineSession { FakeSession(launch) }
}

final class FakeSession: EngineSession {
    let view: PlatformView
    private let label: FakeSessionLabel

    init(_ launch: GameLaunch) {
        label = FakeSessionLabel(title: launch.game.metadata?.title ?? launch.game.title)
        view = label
    }

    func send(_ event: ControllerEvent) { label.lastEvent = "\(event)" }
    func setPaused(_ paused: Bool) { label.paused = paused }
    func terminate() {}
}

#if canImport(UIKit)
final class FakeSessionLabel: UILabel {
    let title: String
    var lastEvent = "none" { didSet { refresh() } }
    var paused = false { didSet { refresh() } }

    init(title: String) {
        self.title = title
        super.init(frame: .zero)
        backgroundColor = UIColor(red: 0.1, green: 0.25, blue: 0.4, alpha: 1)
        textColor = .white
        textAlignment = .center
        numberOfLines = 0
        font = .monospacedSystemFont(ofSize: 18, weight: .semibold)
    }

    required init?(coder: NSCoder) { fatalError() }

    override func layoutSubviews() {
        super.layoutSubviews()
        refresh()
    }

    private func refresh() {
        let shape = bounds.width > bounds.height ? "landscape" : "portrait"
        text = "FAKE SESSION\n\(title)\n\(Int(bounds.width)) x \(Int(bounds.height)) \(shape)\(paused ? "\npaused" : "")\nlast event: \(lastEvent)"
    }
}

/// Logs frame pacing from a display link: frames per second, hitches (frames that took over 1.5 intervals), and the worst frame.
final class FrameMonitor: NSObject {
    static let shared = FrameMonitor()
    private var link: CADisplayLink?
    private var last: CFTimeInterval = 0
    private var windowStart: CFTimeInterval = 0
    private var frames = 0
    private var hitches = 0
    private var worst: CFTimeInterval = 0

    func start() {
        guard link == nil else { return }
        let link = CADisplayLink(target: self, selector: #selector(tick))
        link.add(to: .main, forMode: .common)
        self.link = link
    }

    @objc private func tick(_ link: CADisplayLink) {
        let now = link.timestamp
        let interval = link.targetTimestamp - link.timestamp
        if last > 0 {
            let delta = now - last
            frames += 1
            worst = max(worst, delta)
            if delta > interval * 1.5 { hitches += 1 }
        } else {
            windowStart = now
        }
        last = now
        if now - windowStart >= 1 {
            NSLog("FRAMES fps=%d hitches=%d worst=%.1fms target=%.1fms", frames, hitches, worst * 1000, interval * 1000)
            frames = 0
            hitches = 0
            worst = 0
            windowStart = now
        }
    }
}
#else
final class FakeSessionLabel: NSTextField {
    let title: String
    var lastEvent = "none" { didSet { refresh() } }
    var paused = false { didSet { refresh() } }

    init(title: String) {
        self.title = title
        super.init(frame: .zero)
        isEditable = false
        isBordered = false
        drawsBackground = true
        backgroundColor = NSColor(red: 0.1, green: 0.25, blue: 0.4, alpha: 1)
        textColor = .white
        alignment = .center
        font = .monospacedSystemFont(ofSize: 18, weight: .semibold)
        refresh()
    }

    required init?(coder: NSCoder) { fatalError() }

    private func refresh() {
        stringValue = "FAKE SESSION\n\(title)\(paused ? "\npaused" : "")\nlast event: \(lastEvent)"
    }
}
#endif
#endif
