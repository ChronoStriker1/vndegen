import Foundation
import OSLog
import VNCore
import WebRuntime

extension EngineID {
    nonisolated public static let twine = EngineID("twine")
}

nonisolated let twineLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "twine")

/// Twine stories: one HTML file holding the story format's code and the passages. The shared web host shows it as a
/// browser would, whichever wrapper its PC release came in.
nonisolated public struct TwinePlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .twine, displayName: "Twine", shortName: "Twine", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let game = TwineGame(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, version: game.format, evidence: game.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let game = TwineGame(folder: launch.folder) else { throw TwineError.notTwine }
        guard let url = Bundle.module.url(forResource: "twine", withExtension: "js") else { throw TwineError.scriptMissing }
        twineLog.notice("starting \(game.evidence, privacy: .public), \(game.format ?? "no story format", privacy: .public)")
        var configuration = WebGameConfiguration(
            folder: GameFolder(root: launch.folder, webRoot: game.webRoot), appName: launch.game.title, runtime: .browser,
            userScripts: [try String(contentsOf: url, encoding: .utf8)],
            storageDirectory: WebStorage(dataDirectory: launch.dataDirectory).directory, startPage: Self.startPage(game.page),
            buttonScript: { _, _ in nil }, scriptArgument: "twine-script", log: twineLog
        )
        configuration.scrollsPage = true
        return try configuration.makeSession(launch)
    }

    /// Harlowe and SugarCube keep saves in localStorage. On the PC that is the browser's own database, or the wrapper's
    /// Chromium profile, which Ludusavi can't map to files, so saves move between VNDegen devices.
    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        [WebStorage(dataDirectory: dataDirectory).saveLocation]
    }

    /// Twine stories are read with the mouse or by touch, and some games take keys.
    public var defaultControls: ControlLayout { .keyboardAndMouse }

    /// The page's URL path, each character a file name may hold that a URL path can't encoded.
    static func startPage(_ page: String) -> String {
        "/" + (page.addingPercentEncoding(withAllowedCharacters: pathAllowed) ?? page)
    }

    private static let pathAllowed = CharacterSet.urlPathAllowed.subtracting(CharacterSet(charactersIn: "/;?#%"))
}

nonisolated enum TwineError: LocalizedError {
    case notTwine
    case scriptMissing

    var errorDescription: String? {
        switch self {
        case .notTwine: "This folder does not hold a Twine story."
        case .scriptMissing: "The Twine support script is missing from the app bundle."
        }
    }
}
