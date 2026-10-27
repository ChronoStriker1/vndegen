import Foundation
import VNCore

extension EngineID {
    nonisolated public static let kiriKiri = EngineID("kirikiri")
}

/// KiriKiri 2 and KiriKiri Z games, run on krkrsdl3 inside VNDegen's process. The engine is the KiriKiri framework the
/// app embeds. The package only finds it at run time.
nonisolated public struct KiriKiriPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .kiriKiri, displayName: "KiriKiri", shortName: "KiriKiri", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        let layout = KiriKiriLayout.needsDeeperListing(probe) ? KiriKiriLayout(scanning: probe.root) : KiriKiriLayout(probe)
        guard let layout else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let engine = KiriKiriEngine.load() else { throw KiriKiriError.engineMissing }
        guard !KiriKiriSession.engineInUse else { throw KiriKiriError.stillClosing }
        guard let layout = KiriKiriLayout(scanning: launch.folder) else { throw KiriKiriError.notKiriKiri }
        return try KiriKiriSession(launch, layout: layout, engine: engine)
    }

    /// KiriKiri reads a mouse and keyboard and no gamepad, and KAG3 games are played with the mouse: a click reads on and
    /// presses buttons, the right click and the wheel hide the text and open the message history.
    public var defaultControls: ControlLayout { .keyboardAndMouse }

    /// Whether a game's frame or start is on the stack below the caller. Only the game's own modal window, such as its
    /// Yes/No dialog, runs the app's events there, and the game cannot close until the caller returns.
    @MainActor public static var frameIsRunning: Bool { KiriKiriSession.inFrame }
}

nonisolated public enum KiriKiriError: Error, LocalizedError {
    case engineMissing
    case notKiriKiri
    case stillClosing

    public var errorDescription: String? {
        switch self {
        case .engineMissing: "The KiriKiri engine is missing from this build of VNDegen."
        case .notKiriKiri: "No KiriKiri game was found in this folder."
        case .stillClosing: "The last KiriKiri game is still closing. Try again in a moment."
        }
    }
}
