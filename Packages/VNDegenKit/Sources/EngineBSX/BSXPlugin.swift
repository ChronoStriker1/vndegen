import Foundation
import VNCore

extension EngineID {
    nonisolated public static let bsx = EngineID("bsx")
}

nonisolated public struct BSXPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .bsx, displayName: "BISHOP BSX", shortName: "BSX", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = BSXLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let layout = BSXLayout(FolderProbe(scanning: launch.folder)) else { throw BSXError.notBSX }
        let root = layout.url(in: launch.folder)
        return BSXSession(launch, root: root, saveDirectory: root)
    }

    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let layout = BSXLayout(FolderProbe(scanning: folder)) else { return [] }
        let directory = FileCase.resolve("save", in: layout.url(in: folder))
        return [SaveLocation(directory: directory) { path in Self.saveName(path) }]
    }

    public var defaultControls: ControlLayout { .visualNovel }

    nonisolated static func saveName(_ path: String) -> String? {
        guard let name = path.replacingOccurrences(of: "\\", with: "/").split(separator: "/").last.map(String.init) else { return nil }
        let lower = name.lowercased()
        if lower == "common.dat" { return name }
        guard lower.hasPrefix("no") else { return nil }
        let rest = lower.dropFirst(2)
        if rest.count == 3, rest.allSatisfy(\.isNumber) { return name }
        if rest.count == 3, rest.first == "-", rest.dropFirst().allSatisfy(\.isNumber) { return name }
        return nil
    }
}

nonisolated public enum BSXError: Error, LocalizedError {
    case notBSX

    public var errorDescription: String? {
        switch self {
        case .notBSX: "No BISHOP BSX game was found in this folder."
        }
    }
}
