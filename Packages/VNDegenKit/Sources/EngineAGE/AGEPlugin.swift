import Foundation
import VNCore

extension EngineID {
    nonisolated public static let age = EngineID("age")
}

nonisolated public struct AGEPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .age, displayName: "Eushully AGE", shortName: "AGE", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = AGELayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let layout = AGELayout(FolderProbe(scanning: launch.folder)) else { throw AGEError.notAGE }
        return AGESession(launch, root: layout.url(in: launch.folder), saveDirectory: Self.saveDirectory(launch.dataDirectory))
    }

    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard AGELayout(FolderProbe(scanning: folder)) != nil else { return [] }
        let directory = Self.saveDirectory(dataDirectory)
        return [SaveLocation(directory: directory) { path in Self.saveName(path, directory: directory) }]
    }

    public var defaultControls: ControlLayout { .mouse }

    nonisolated static func saveDirectory(_ dataDirectory: URL) -> URL {
        dataDirectory.appending(path: "saves", directoryHint: .isDirectory)
    }

    nonisolated static func saveName(_ path: String, directory: URL) -> String? {
        var path = path.replacingOccurrences(of: "\\", with: "/")
        let local = directory.path.hasSuffix("/") ? directory.path : directory.path + "/"
        var parts: [String]
        if path.hasPrefix(local) {
            path = String(path.dropFirst(local.count))
            parts = path.split(separator: "/", omittingEmptySubsequences: false).map(String.init)
        } else {
            let all = path.split(separator: "/", omittingEmptySubsequences: false).map(String.init)
            guard let i = all.lastIndex(where: { $0.lowercased() == "eushully" }), i + 2 < all.count else { return nil }
            parts = Array(all[(i + 2)...])
        }
        guard !parts.contains(where: { ["", ".", ".."].contains($0) }) else { return nil }
        if parts.count == 1 {
            let lower = parts[0].lowercased()
            return lower == "sys4reg.ini" ? parts[0] : nil
        }
        guard parts.count == 2, parts[0].lowercased() == "save" else { return nil }
        let name = parts[1], lower = name.lowercased()
        if ["save.dat", "save.bak", "rt.dat", "rt.bak"].contains(lower) { return parts.joined(separator: "/") }
        guard lower.hasPrefix("save"), lower.hasSuffix(".dat") || lower.hasSuffix(".sth") else { return nil }
        let digits = lower.dropFirst(4).dropLast(4)
        return digits.count == 2 && digits.allSatisfy(\.isNumber) ? parts.joined(separator: "/") : nil
    }
}

nonisolated public enum AGEError: Error, LocalizedError {
    case notAGE

    public var errorDescription: String? {
        switch self {
        case .notAGE: "No Eushully AGE game was found in this folder."
        }
    }
}
