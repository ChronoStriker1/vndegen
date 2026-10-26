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
        return try WebGameConfiguration(
            folder: folder, appName: game.projectID, runtime: Self.runtime(for: game),
            userScripts: [try String(contentsOf: url, encoding: .utf8)],
            userAgent: Self.chromeUserAgent + (game.packaging == .electron ? " TyranoErectron" : ""),
            storageDirectory: launch.dataDirectory.appendingPathComponent("localStorage", isDirectory: true), transcodesOgg: true,
            buttonScript: { button, pressed in "__vndegenTyrano.button('\(button.rawValue)', \(pressed))" },
            scriptArgument: "tyrano-script", log: tyranoLog
        ).makeSession(launch)
    }

    /// The PC builds run on Chromium, and Tyrano branches on the browser it sees. As Safari it would ask for .m4a files
    /// in place of the .ogg files the games ship. Electron builds also mark the user agent, which is how Tyrano tells
    /// Electron from NW.js.
    static let chromeUserAgent = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36"

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
