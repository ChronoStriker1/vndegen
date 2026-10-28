import Foundation
import OSLog
import VNCore
import WebRuntime

extension EngineID {
    nonisolated public static let flash = EngineID("flash")
}

nonisolated let flashLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "flash")

/// Flash games: SWF movies, Windows projectors with the movie appended, and HTML pages that launch movies. Ruffle, a
/// Flash Player written in Rust and built for the web, plays them in the shared web host.
nonisolated public struct FlashPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .flash, displayName: "Flash", shortName: "Flash", platforms: [.macOS, .iOS])
    }

    /// A projector is certainly Flash. A movie alone could be a cutscene in another engine's game, which that engine's
    /// own match outranks.
    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        // The probe is enough when it shows a movie or projector, which also keeps a non-Flash game with a movie in it
        // from a deeper scan at each library refresh. Archives often wrap a projector in a few folders, deeper than the
        // probe looks, so only then does detection look further, and only below the probe's deepest folders.
        let game: FlashGame? = if Self.showsFlash(probe.entries, in: probe.root) {
            FlashGame(probe)
        } else if Self.wrapsFlash(probe) {
            FlashGame(folder: probe.root)
        } else {
            nil
        }
        guard let game else { return nil }
        let confidence: DetectionConfidence = game.entries.contains { if case .projector = $0 { true } else { false } } ? .certain : .likely
        return EngineMatch(engine: descriptor.id, confidence: confidence, evidence: game.main.path)
    }

    static func showsFlash(_ paths: some Sequence<String>, in root: URL) -> Bool {
        paths.contains { path in
            switch (path as NSString).pathExtension.lowercased() {
            case "swf": FlashGame.isMovie(root.appendingPathComponent(path))
            case "exe": FlashGame.projectorMovie(root.appendingPathComponent(path)) != nil
            default: false
            }
        }
    }

    /// Whether a movie or projector lies below the folders at the probe's depth, down to `FlashGame.maxDepth`, looking at
    /// no more than `budget` entries.
    static func wrapsFlash(_ probe: FolderProbe, budget: Int = 2000) -> Bool {
        let probeDepth = probe.entries.map { $0.split(separator: "/").count }.max() ?? 0
        var pending = probe.entries.sorted().filter { $0.split(separator: "/").count == probeDepth }.prefix(budget)
            .filter { GameFolder.isDirectory(probe.root.appendingPathComponent($0)) }
        var remaining = budget
        while let folder = pending.popLast(), remaining > 0 {
            let names = (try? FileManager.default.contentsOfDirectory(atPath: probe.root.appendingPathComponent(folder).path)) ?? []
            let paths = names.sorted().prefix(remaining).filter { !$0.hasPrefix(".") }.map { "\(folder)/\($0)" }
            remaining -= min(names.count, remaining)
            if showsFlash(paths, in: probe.root) { return true }
            if folder.split(separator: "/").count + 1 < FlashGame.maxDepth {
                pending += paths.filter { GameFolder.isDirectory(probe.root.appendingPathComponent($0)) }
            }
        }
        return false
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let game = FlashGame(folder: launch.folder) else { throw FlashError.notFlash }
        guard let ruffle = Self.ruffleDirectory,
              let ruffleScript = try? String(contentsOf: ruffle.appendingPathComponent("ruffle.js"), encoding: .utf8)
        else { throw FlashError.ruffleMissing }
        guard let host = Self.hostDirectory,
              let pageSource = try? String(contentsOf: host.appendingPathComponent("flash-page.js"), encoding: .utf8)
        else { throw FlashError.hostMissing }
        let pageScript = try FlashFonts.pageScript(pageSource)
        let entry = Self.entry(for: game, dataDirectory: launch.dataDirectory)
        var folder = GameFolder(root: launch.folder, webRoot: "")
        folder.mounts = [Self.hostPath: host, Self.hostPath + "ruffle/": ruffle]
        // Every projector, since a launcher page can open any of them.
        for case let .projector(path, movie) in game.entries { folder.embedded["/" + path] = movie }
        flashLog.notice("starting \(entry.path, privacy: .public)")
        return try WebGameConfiguration(
            folder: folder, appName: launch.game.title, runtime: .browser, userScripts: [pageScript, ruffleScript],
            storage: Self.storage(launch.dataDirectory), startPage: Self.startPage(for: entry),
            buttonScript: { _, _ in nil }, scriptArgument: "flash-script", log: flashLog
        ).makeSession(launch)
    }

    /// Flash games are played with the mouse, and some with the keyboard. Flash Player read no gamepad.
    public var defaultControls: ControlLayout { .keyboardAndMouse }

    /// Where the page sees the files VNDegen ships for Flash: the host page, and Ruffle under `ruffle/`.
    static let hostPath = "/__vndegen/"

    /// The host page and its scripts, from this module's resources.
    static var hostDirectory: URL? { Bundle.module.url(forResource: "flash", withExtension: "html")?.deletingLastPathComponent() }

    /// Ruffle's web build, which the app bundles as Ruffle. Missing from package tests.
    static var ruffleDirectory: URL? { Bundle.main.url(forResource: "Ruffle", withExtension: nil) }

    /// The page the session opens: a launcher page itself, and otherwise the host page with the movie's URL path, which
    /// the query escapes once more so that it reaches `flash.js` still encoded.
    static func startPage(for entry: FlashGame.Entry) -> String {
        let path = urlPath(entry.path)
        if case .page = entry { return path }
        let query = CharacterSet(charactersIn: "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~/")
        return "\(hostPath)flash.html?movie=\(path.addingPercentEncoding(withAllowedCharacters: query) ?? path)"
    }

    /// A path in the game folder as a URL path, `/My%20Game/game.swf`. Each part is encoded as a browser encodes a path,
    /// and `%`, `#`, and `?` too, so the part reads back the same. Ruffle names shared objects with this encoding.
    static func urlPath(_ path: String) -> String {
        "/" + path.split(separator: "/").map { urlComponent(String($0)) }.joined(separator: "/")
    }

    static func urlComponent(_ name: String) -> String {
        name.addingPercentEncoding(withAllowedCharacters: urlAllowed) ?? name
    }

    /// Printable ASCII but the space, `"`, `#`, `%`, `/`, `<`, `>`, `?`, `` ` ``, `{`, and `}`. Everything else, such as
    /// Japanese, is percent-encoded as UTF-8.
    private static let urlAllowed = CharacterSet(charactersIn: Unicode.Scalar(0x21) ... Unicode.Scalar(0x7E))
        .subtracting(CharacterSet(charactersIn: "\"#%/<>?`{}"))
}

nonisolated enum FlashError: LocalizedError {
    case notFlash
    case ruffleMissing
    case hostMissing

    var errorDescription: String? {
        switch self {
        case .notFlash: "This folder does not hold a Flash game."
        case .ruffleMissing: "Ruffle, the Flash player, is missing from the app bundle."
        case .hostMissing: "The Flash host page is missing from the app bundle."
        }
    }
}
