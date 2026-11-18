import CIPAC
import Foundation
import VNCore

extension EngineID {
    nonisolated public static let ipac = EngineID("ipac")
}

nonisolated public struct IPACPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .ipac, displayName: "IPAC", shortName: "IPAC", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = IPACLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let layout = IPACLayout(FolderProbe(scanning: launch.folder)) else { throw IPACError.notIPAC }
        let root = layout.url(in: launch.folder)
        guard let folder = Self.saveFolder(root) else { throw IPACError.noGameInfo }
        return IPACSession(launch, root: root, saveDirectory: Self.saveDirectory(launch.dataDirectory, folder: folder))
    }

    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let layout = IPACLayout(FolderProbe(scanning: folder)), let names = Self.saveFolder(layout.url(in: folder)) else { return [] }
        let directory = Self.saveDirectory(dataDirectory, folder: names)
        return [SaveLocation(directory: directory) { path in
            Self.saveRelativePath(path, directory: directory, folder: names)
        }]
    }

    public var defaultControls: ControlLayout { .visualNovel }

    nonisolated struct SaveFolder: Equatable, Sendable {
        var brand: String
        var title: String
    }

    nonisolated static func saveFolder(_ root: URL) -> SaveFolder? {
        var brand = [CChar](repeating: 0, count: 256)
        var title = [CChar](repeating: 0, count: 512)
        guard vnipac_save_folder(root.path, &brand, brand.count, &title, title.count) else { return nil }
        let folder = SaveFolder(brand: String(decoding: brand.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self),
                                title: String(decoding: title.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self))
        return folder.brand.isEmpty || folder.title.isEmpty ? nil : folder
    }

    nonisolated static func saveDirectory(_ dataDirectory: URL, folder: SaveFolder) -> URL {
        dataDirectory.appending(path: "Documents", directoryHint: .isDirectory)
            .appending(path: folder.brand, directoryHint: .isDirectory)
            .appending(path: folder.title, directoryHint: .isDirectory)
    }

    nonisolated static func saveRelativePath(_ path: String, directory: URL, folder: SaveFolder) -> String? {
        let path = path.replacingOccurrences(of: "\\", with: "/").precomposedStringWithCanonicalMapping
        let local = directory.path.precomposedStringWithCanonicalMapping
        if path.hasPrefix(local + "/") { return SaveLocation.insideFolder(String(path.dropFirst(local.count + 1))) }
        let marker = "/\(folder.brand)/\(folder.title)/".precomposedStringWithCanonicalMapping.lowercased()
        let lower = path.lowercased()
        guard lower.count == path.count, let found = lower.range(of: marker, options: .backwards) else { return nil }
        let relative = String(path.dropFirst(lower.distance(from: lower.startIndex, to: found.upperBound)))
        return relative.isEmpty ? nil : SaveLocation.insideFolder(relative)
    }
}

nonisolated public enum IPACError: Error, LocalizedError {
    case notIPAC
    case noGameInfo

    public var errorDescription: String? {
        switch self {
        case .notIPAC: "No IPAC game was found in this folder."
        case .noGameInfo: "This game's scripts don't name its save folder."
        }
    }
}
