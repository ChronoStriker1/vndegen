import CN2System
import Foundation
import VNCore

extension EngineID {
    nonisolated public static let n2system = EngineID("n2system")
}

nonisolated public struct N2SystemPlugin: EnginePlugin {
    public init() {}

    public var descriptor: EngineDescriptor {
        EngineDescriptor(id: .n2system, displayName: "Nitroplus N2System", shortName: "N2System", platforms: [.macOS, .iOS])
    }

    public func detect(_ probe: FolderProbe) -> EngineMatch? {
        guard let layout = N2SystemLayout(probe) else { return nil }
        return EngineMatch(engine: descriptor.id, confidence: .certain, evidence: layout.evidence)
    }

    @MainActor public func makeSession(_ launch: GameLaunch) throws -> any EngineSession {
        guard let layout = N2SystemLayout(FolderProbe(scanning: launch.folder)) else { throw N2SystemError.notN2System }
        let root = layout.url(in: launch.folder)
        guard let product = Self.product(root) else { throw N2SystemError.noSystemIni }
        return N2SystemSession(launch, root: root, saveDirectory: Self.saveDirectory(launch.dataDirectory, product: product))
    }

    public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let layout = N2SystemLayout(FolderProbe(scanning: folder)),
              let product = Self.product(layout.url(in: folder)) else { return [] }
        let directory = Self.saveDirectory(dataDirectory, product: product)
        return [SaveLocation(directory: directory) { path in
            Self.saveRelativePath(path, directory: directory, product: product)
        }]
    }

    public var defaultControls: ControlLayout { .visualNovel }

    nonisolated struct Product: Equatable, Sendable {
        var name: String
        var version: String
    }

    nonisolated static func product(_ root: URL) -> Product? {
        var name = [CChar](repeating: 0, count: 512)
        var version = [CChar](repeating: 0, count: 256)
        guard vnn2_product(root.path, &name, name.count, &version, version.count) else { return nil }
        let product = Product(name: String(decoding: name.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self),
                              version: String(decoding: version.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self))
        return product.name.isEmpty ? nil : product
    }

    nonisolated static func saveDirectory(_ dataDirectory: URL, product: Product) -> URL {
        dataDirectory.appending(path: "AppData/Roaming/Nitroplus", directoryHint: .isDirectory)
            .appending(path: product.name, directoryHint: .isDirectory)
            .appending(path: product.version, directoryHint: .isDirectory)
    }

    nonisolated static func saveRelativePath(_ path: String, directory: URL, product: Product) -> String? {
        let path = path.replacingOccurrences(of: "\\", with: "/")
        let local = directory.path.hasSuffix("/") ? directory.path : directory.path + "/"
        if path.hasPrefix(local) { return SaveLocation.insideFolder(String(path.dropFirst(local.count))) }
        let marker = "/nitroplus/\(product.name)/\(product.version)/".lowercased()
        guard let found = path.lowercased().range(of: marker, options: .backwards) else { return nil }
        let offset = path.lowercased().distance(from: path.lowercased().startIndex, to: found.upperBound)
        let relative = String(path.dropFirst(offset))
        return relative.isEmpty ? nil : SaveLocation.insideFolder(relative)
    }
}

nonisolated public enum N2SystemError: Error, LocalizedError {
    case notN2System
    case noSystemIni

    public var errorDescription: String? {
        switch self {
        case .notN2System: "No N2System game was found in this folder."
        case .noSystemIni: "This game's system.ini couldn't be read."
        }
    }
}
