import Foundation
import OSLog
import VNCore
import WebRuntime

nonisolated let rpgLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "rpgmaker")

extension EngineID {
    nonisolated public static let rpgMakerMV = EngineID("rpgmaker.mv")
    nonisolated public static let rpgMakerMZ = EngineID("rpgmaker.mz")
}

/// RPG Maker MV and MZ. Both are HTML5 games shipped inside NW.js, so one WebKit runtime runs both.
nonisolated public struct RPGMakerPlugin: EnginePlugin {
    public let flavor: Flavor

    public enum Flavor: Sendable { case mv, mz }

    public init(_ flavor: Flavor) { self.flavor = flavor }

    public var descriptor: EngineDescriptor {
        switch flavor {
        case .mv: EngineDescriptor(id: .rpgMakerMV, displayName: "RPG Maker MV", shortName: "MV", platforms: [.macOS, .iOS])
        case .mz: EngineDescriptor(id: .rpgMakerMZ, displayName: "RPG Maker MZ", shortName: "MZ", platforms: [.macOS, .iOS])
        }
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        let core = flavor == .mz ? "rmmz_core.js" : "rpg_core.js"
        // Deployed MV games keep the web root in www/. MZ always deploys it at the top level.
        for root in ["", "www/"] where probe.contains("\(root)js/\(core)") {
            return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: "\(root)js/\(core)")
        }
        return nil
    }

    /// Runs the game in the shared web host.
    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        let folder = GameFolder(rpgMakerGameAt: launch.folder)
        if flavor == .mv { folder.moveCommunityCoreSaves() }
        return try webConfiguration(folder).makeSession(launch)
    }

    /// The desktop user agent hides the iPhone from MZ's `Utils.isMobileDevice`, which would otherwise give up 10% of
    /// the height to browser chrome that a web view does not have. iPad and macOS already report this shape. MV's
    /// `Utils.isMobileDevice` would also make it ask for .m4a files. An MV game that ships only Ogg audio gets the shared
    /// Ogg decoder, and MV plays its .ogg files through it. MZ decodes Ogg with its own bundled decoder.
    @MainActor func webConfiguration(_ folder: GameFolder) throws -> WebGameConfiguration {
        guard let url = Bundle.module.url(forResource: "rpgmaker", withExtension: "js") else { throw RPGMakerError.scriptMissing }
        var configuration = try WebGameConfiguration(
            folder: folder, appName: "rmmz-game",
            userScripts: [try String(contentsOf: url, encoding: .utf8)], userAgent: Self.userAgent,
            buttonScript: { button, pressed in
                KeyStroke.key(for: button).map {
                    "__vndegen.key('\(pressed ? "keydown" : "keyup")', '\($0.key)', '\($0.code)', \($0.keyCode))"
                }
            },
            scriptArgument: "rpgmaker-script", log: rpgLog
        )
        configuration.preloadFonts = folder.fontNames
        if flavor == .mv && folder.shipsOnlyOggAudio {
            configuration.decodesOgg = true
            rpgLog.notice("MV game ships only Ogg audio, decoding it in the page")
        }
        return configuration
    }

    /// Buttons used to press these keys, so a remap from then presses the same key.
    public func legacyAction(for button: VirtualButton) -> ControlAction? {
        KeyStroke.key(for: button).map(ControlAction.key)
    }
}

extension RPGMakerPlugin {
    #if os(iOS)
    static let userAgent: String? = WebGameConfiguration.desktopUserAgent
    #else
    static let userAgent: String? = nil
    #endif
}

nonisolated enum RPGMakerError: LocalizedError {
    case scriptMissing

    var errorDescription: String? {
        switch self {
        case .scriptMissing: "The RPG Maker compatibility script is missing from the app bundle."
        }
    }
}
