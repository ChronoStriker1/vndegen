import Foundation
import VNCore

extension EngineID {
    nonisolated public static let exhibit = EngineID("exhibit")
}

nonisolated public struct ExHibitPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .exhibit, displayName: "ExHibit", shortName: "ExHibit", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = ExHibitLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let layout = ExHibitLayout(FolderProbe(scanning: launch.folder)) else { throw ExHibitError.notExHibit }
        let root = layout.url(in: launch.folder)
        return ExHibitSession(launch, root: root, saveDirectory: Self.userFolder(root))
    }

    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let layout = ExHibitLayout(FolderProbe(scanning: folder)) else { return [] }
        return [SaveLocation(directory: Self.userFolder(layout.url(in: folder))) { path in Self.saveName(path) }]
    }

    public var defaultControls: ControlLayout { .visualNovel }

    nonisolated static func userFolder(_ root: URL) -> URL { FileCase.resolve("userdata", in: root) }

    nonisolated static func saveName(_ path: String) -> String? {
        var parts = path.replacingOccurrences(of: "\\", with: "/").split(separator: "/", omittingEmptySubsequences: false).map(String.init)
        if let i = parts.lastIndex(where: { $0.lowercased() == "userdata" }) { parts = Array(parts[(i + 1)...]) }
        guard let first = parts.first, !parts.contains(where: { ["", ".", ".."].contains($0) }) else { return nil }
        let lower = first.lowercased()
        if parts.count == 2, lower == "thumbnail" || lower == "vndegen" { return parts.joined(separator: "/") }
        guard parts.count == 1 else { return nil }
        if ["udb", "qdb", "adb", "settings.dat"].contains(lower) || lower.hasSuffix(".system.dat") { return first }
        guard let head = lower.first, "uqa".contains(head) else { return nil }
        let digits = lower.dropFirst()
        return !digits.isEmpty && digits.count <= 3 && digits.allSatisfy(\.isNumber) ? first : nil
    }
}

nonisolated public enum ExHibitError: Error, LocalizedError {
    case notExHibit

    public var errorDescription: String? {
        switch self {
        case .notExHibit: "No ExHibit game was found in this folder."
        }
    }
}
