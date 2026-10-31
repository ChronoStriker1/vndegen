import FontPack
import Foundation
import OSLog
import VNCore
import WebRuntime

extension EngineID {
    nonisolated public static let tyrano = EngineID("tyranoscript")
}

nonisolated let tyranoLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "tyrano")

/// TyranoScript and TyranoBuilder games. Each ships the whole engine as HTML and JavaScript in tyrano/, so the shared web
/// host runs it as its PC build would: in NW.js, in Electron, or as a plain page.
nonisolated public struct TyranoPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .tyrano, displayName: "TyranoScript", shortName: "Tyrano", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let game = TyranoGame(folder: probe.root) else { return nil }
        let evidence = game.webRoot.isEmpty ? TyranoGame.engineFile : "\(game.webRoot)/\(TyranoGame.engineFile)"
        return EngineMatch(engine: descriptor.id, confidence: .certain, version: game.version, evidence: evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let game = TyranoGame(folder: launch.folder) else { throw TyranoError.notTyrano }
        guard let url = Bundle.module.url(forResource: "tyrano", withExtension: "js") else { throw TyranoError.scriptMissing }
        let folder = GameFolder(root: launch.folder, webRoot: game.webRoot)
        let storage = WebStorage(dataDirectory: launch.dataDirectory)
        do {
            try Self.adoptSaveKey(of: game, folder: launch.folder, storage: storage)
        } catch {
            tyranoLog.error("could not update the save key: \(error.localizedDescription, privacy: .public)")
        }
        // Electron builds also mark the user agent, which is how Tyrano tells Electron from NW.js.
        var configuration = try WebGameConfiguration(
            folder: folder, appName: game.projectID, runtime: Self.runtime(for: game),
            userScripts: [try String(contentsOf: url, encoding: .utf8)],
            userAgent: WebGameConfiguration.chromeUserAgent + (game.packaging == .electron ? " TyranoErectron" : ""),
            storageDirectory: storage.directory, decodesOgg: true,
            buttonScript: { button, pressed in "__vndegenTyrano.button('\(button.rawValue)', \(pressed))" },
            scriptArgument: "tyrano-script", log: tyranoLog
        )
        // Text is HTML here, so a font face that loads late only redraws it. The game's default font, which its
        // Config.tjs names, loads up front so the first lines don't show the fallback.
        configuration.preloadFonts = folder.read(servedPath: "/data/system/Config.tjs", limit: 1 << 20)
            .map { FontPack.referencedNames(in: String(decoding: $0, as: UTF8.self)) } ?? []
        return try configuration.makeSession(launch)
    }

    /// Buttons used to run Tyrano's actions here directly, which still works. As an Xbox button, such a remap would do
    /// what the game's own KeyConfig.js says when the game reads the gamepad.
    public func legacyAction(for button: VirtualButton) -> ControlAction? {
        button == .hub ? nil : .button(button)
    }

    /// TyranoScript games are played with a mouse. Clicks read on and press buttons, the right click and the wheel
    /// hide the text and open the backlog, and only TyranoScript 5 reads a gamepad, when the game turns it on. Games
    /// that turn it on often bind no buttons, so every game starts with the keyboard and mouse layout.
    public var defaultControls: ControlLayout { .keyboardAndMouse }

    /// Tyrano saves beside the PC executable. It finds that folder by cutting `process.execPath` (NW.js) or
    /// `app.getAppPath()` (Electron) at the `.app` bundle of a Mac build, so a Mac-shaped path inside the game folder
    /// puts the saves at its top, where the Windows build keeps them too.
    static func runtime(for game: TyranoGame) -> PageRuntime {
        switch game.packaging {
        case .browser: .browser
        case .nwjs: .nwjs(execPath: "\(GameFolder.virtualRoot)/Game.app/Contents/MacOS/nwjs")
        case .electron:
            .electron(appPath: "\(GameFolder.virtualRoot)/Game.app/Contents/Resources/\(game.webRoot.hasSuffix(".asar") ? "app.asar" : "app")")
        }
    }
}

nonisolated enum TyranoError: LocalizedError {
    case notTyrano
    case scriptMissing

    var errorDescription: String? {
        switch self {
        case .notTyrano: "This folder does not hold a TyranoScript game."
        case .scriptMissing: "The TyranoScript controller script is missing from the app bundle."
        }
    }
}
