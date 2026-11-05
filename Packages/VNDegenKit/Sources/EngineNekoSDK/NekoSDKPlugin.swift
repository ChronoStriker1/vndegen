import CNekoSDK
import Foundation
import VNCore

extension EngineID {
    nonisolated public static let nekoSDK = EngineID("nekosdk")
}

/// NekoSDK games, run on VNDegen's own NekoSDK runtime (the CNekoSDK target) inside the app.
nonisolated public struct NekoSDKPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .nekoSDK, displayName: "NekoSDK", shortName: "NekoSDK", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = NekoSDKLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: layout.confidence, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let layout = NekoSDKLayout(FolderProbe(scanning: launch.folder)) else { throw NekoSDKError.notNekoSDK }
        let root = layout.url(in: launch.folder)
        guard vnneko_is_supported(root.path) else { throw NekoSDKError.notYet }
        return NekoSDKSession(launch, root: root)
    }

    /// The player taps and clicks the game's own message window and menus, so no touch gamepad covers them.
    public var readsTouches: Bool { true }

    /// NekoSDK games keep their menus in their exes, so VNDegen holds a table for each. Until it has one, the game can't run.
    public func compatibilityNote(for game: Game, folder: URL, dataDirectory: URL) -> String? {
        guard let layout = NekoSDKLayout(FolderProbe(scanning: folder)), !vnneko_is_supported(layout.url(in: folder).path) else { return nil }
        return "VNDegen can't run this NekoSDK game yet. Its menus aren't built."
    }

    /// Windows keeps a NekoSDK game's saves in AppData\\Roaming under the folder its init.ini names, and the runtime uses the
    /// same path under the game's data folder, so a backup from Windows restores into the folder the game reads.
    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let layout = NekoSDKLayout(FolderProbe(scanning: folder)),
              let name = vnneko_user_data_directory(layout.url(in: folder).path).map({ String(cString: $0) }), !name.isEmpty else { return [] }
        let directory = dataDirectory.appending(path: "AppData/Roaming/\(name)/save", directoryHint: .isDirectory)
        let suffix = "/\(name.lowercased())/save/"
        return [SaveLocation(directory: directory) { path in
            guard let range = path.lowercased().range(of: suffix) else { return nil }
            let file = String(path.lowercased()[range.upperBound...])
            return file.isEmpty || file.contains("/") ? nil : String(path.suffix(file.count))
        }]
    }
}

nonisolated public enum NekoSDKError: Error, LocalizedError {
    case notNekoSDK
    case notYet

    public var errorDescription: String? {
        switch self {
        case .notNekoSDK: "No NekoSDK game was found in this folder."
        case .notYet: "VNDegen can't run this NekoSDK game yet. Its menus aren't built."
        }
    }
}
