import Foundation
import OSLog
import VNCore
import WebRuntime

extension EngineID {
    nonisolated public static let visualNovelMaker = EngineID("visualnovelmaker")
}

nonisolated let visualNovelMakerLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "visualnovelmaker")

/// Visual Novel Maker games. Each ships its engine as HTML and JavaScript in `data/`, so the shared web host runs it as
/// its PC build would: in NW.js, in Electron, or as a plain page.
nonisolated public struct VisualNovelMakerPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .visualNovelMaker, displayName: "Visual Novel Maker", shortName: "VN Maker", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let game = VisualNovelMakerGame(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: game.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let game = VisualNovelMakerGame(folder: launch.folder) else { throw VisualNovelMakerError.notVisualNovelMaker }
        let folder = GameFolder(root: game.folder(in: launch.folder), webRoot: game.webRoot)
        visualNovelMakerLog.notice("starting \(game.evidence, privacy: .public)")
        // The engine picks .m4a and .mp4 files as Safari, and the games ship Ogg Vorbis and WebM, so the page sees the
        // Chrome its PC build runs on and decodes Ogg itself.
        let configuration = WebGameConfiguration(
            folder: folder, appName: launch.game.title, runtime: Self.runtime(for: game, folder: folder),
            userAgent: WebGameConfiguration.chromeUserAgent, storageDirectory: WebStorage(dataDirectory: launch.dataDirectory).directory,
            decodesOgg: true, buttonScript: Self.buttonScript, scriptArgument: "visualnovelmaker-script", log: visualNovelMakerLog
        )
        return try configuration.makeSession(launch)
    }

    /// Saves are `.vndata` files in the game's own folder, which is the PC build's working directory, or in the
    /// localStorage files of a web export. A game set to keep its data in AppData writes the same files there on the
    /// PC, and they restore to the game's folder here too.
    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let game = VisualNovelMakerGame(folder: folder) else { return [] }
        if game.packaging == .browser { return [WebStorage(dataDirectory: dataDirectory).saveLocation] }
        return [SaveLocation(directory: game.folder(in: folder), relativePath: Self.saveFileName)]
    }

    /// `C:/Games/Siren's Call/SaveGame_1.vndata` and `C:/Users/player/AppData/Roaming/Siren's Call/settings.vndata` map
    /// to their file names.
    static func saveFileName(_ path: String) -> String? {
        guard let name = path.split(separator: "/").last, name.lowercased().hasSuffix(".vndata") else { return nil }
        return String(name)
    }

    /// Visual Novel Maker reads the keyboard and mouse. Its gamepad code waits for a gamepad that nothing hands it, and
    /// its menus take only the mouse.
    public var defaultControls: ControlLayout { .keyboardAndMouse }

    static func runtime(for game: VisualNovelMakerGame, folder: GameFolder) -> PageRuntime {
        switch game.packaging {
        case .nwjs: .nwjs(execPath: "\(GameFolder.virtualRoot)/Game.exe")
        case .electron: .electron(appPath: folder.virtualWebRoot)
        case .browser: .browser
        }
    }

    /// Engine-neutral buttons, as DEBUG input scripts send them: confirm reads on with Enter, the engine's C button.
    static func buttonScript(_ button: VirtualButton, _ pressed: Bool) -> String? {
        guard button == .confirm else { return nil }
        return "__vndegen.key('\(pressed ? "keydown" : "keyup")', 'Enter', 'Enter', 13)"
    }
}

nonisolated enum VisualNovelMakerError: LocalizedError {
    case notVisualNovelMaker

    var errorDescription: String? {
        switch self {
        case .notVisualNovelMaker: "This folder does not hold a Visual Novel Maker game."
        }
    }
}
