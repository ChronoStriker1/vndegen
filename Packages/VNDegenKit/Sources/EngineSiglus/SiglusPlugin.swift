import Foundation
import VNCore

extension EngineID {
    nonisolated public static let siglus = EngineID("siglus")
    nonisolated public static let realLive = EngineID("reallive")
}

/// SiglusEngine games, run on siglus_rs inside VNDegen's process and drawn with Metal. The engine is a framework the
/// app embeds; the package only finds it at run time.
nonisolated public struct SiglusPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .siglus, displayName: "SiglusEngine", shortName: "Siglus", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = SiglusLayout(probe), layout.engine == .siglus else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let core = SiglusCore.shared else { throw SiglusError.engineMissing }
        guard !SiglusEngineLock.inUse else { throw SiglusError.stillClosing }
        guard let layout = SiglusLayout(scanning: launch.folder), layout.engine == .siglus else { throw SiglusError.notFound }
        return SiglusSession(launch, root: layout.url(in: launch.folder), core: core)
    }

    /// SiglusEngine games read a mouse and keyboard. A click reads on and presses buttons, and the right click opens
    /// the game's menu.
    public var defaultControls: ControlLayout { .keyboardAndMouse }
}

/// RealLive games, run on siglus_rs inside VNDegen's process. The engine draws each frame on the CPU, and FrameHost
/// shows it.
nonisolated public struct RealLivePlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .realLive, displayName: "RealLive", shortName: "RealLive", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = SiglusLayout(probe), layout.engine == .realLive else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let core = SiglusCore.shared else { throw SiglusError.engineMissing }
        guard !SiglusEngineLock.inUse else { throw SiglusError.stillClosing }
        guard let layout = SiglusLayout(scanning: launch.folder), layout.engine == .realLive else { throw SiglusError.notFound }
        return RealLiveSession(launch, root: layout.url(in: launch.folder), core: core)
    }

    /// RealLive games read a mouse and keyboard, as SiglusEngine games do.
    public var defaultControls: ControlLayout { .keyboardAndMouse }
}

nonisolated public enum SiglusError: Error, LocalizedError {
    case engineMissing
    case notFound
    case stillClosing

    public var errorDescription: String? {
        switch self {
        case .engineMissing: "The SiglusEngine and RealLive engine is missing from this build of VNDegen."
        case .notFound: "No SiglusEngine or RealLive game was found in this folder."
        case .stillClosing: "The last SiglusEngine or RealLive game is still closing. Try again in a moment."
        }
    }
}

/// The engine keeps some state per process, such as its file caches and default font, so one game runs at a time.
@MainActor enum SiglusEngineLock {
    static var inUse = false
}
